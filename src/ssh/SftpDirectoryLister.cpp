#include "SftpDirectoryLister.h"

#include "SftpConnectionPool.h"
#include "SftpTransfer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QUuid>
#include <QVariantMap>

#include <libssh2.h>
#include <libssh2_sftp.h>

using SftpConnectionPool::Lane;

namespace {

QString alternatePath(const QString &path)
{
    const auto separator = path.lastIndexOf(QLatin1Char('/'));
    const QString parent = path.left(separator + 1);
    const QString name = path.mid(separator + 1);
    const auto suffix = name.lastIndexOf(QLatin1Char('.'));
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    const QString extra = QStringLiteral(" (%1)").arg(id);
    return suffix > 0 ? parent + name.left(suffix) + extra + name.mid(suffix)
                      : parent + name + extra;
}

bool cancelled(const SftpDirectoryLister::CancelCallback &isCanceled, QString *error)
{
    if (!isCanceled || !isCanceled()) {
        return false;
    }
    if (error) {
        *error = QObject::tr("Transfer cancelled");
    }
    return true;
}

SftpTransfer::ReconnectCallback recoverTransfer(SftpConnectionPool::CachedConnection *connection,
                                             const ConnectionProfile &profile,
                                             const SftpDirectoryLister::CancelCallback &isCanceled,
                                             QString *error)
{
    return [connection, &profile, &isCanceled, error, retried = false](
               int code, LIBSSH2_SFTP *&sftp, LIBSSH2_SFTP_HANDLE *&handle) mutable {
        if (retried || cancelled(isCanceled, error) || !SftpConnectionPool::connectionLost(connection, code)) {
            return false;
        }
        retried = true;
        SftpConnectionPool::reset(connection, handle);
        handle = nullptr;
        sftp = nullptr;
        if (!SftpConnectionPool::ensureConnected(connection, profile, error)) {
            return false;
        }
        sftp = SftpConnectionPool::sftp(connection);
        return true;
    };
}

struct DirectoryListing
{
    QVariantList rows;
    QString error;
    bool connectionLost = false;
};

DirectoryListing readDirectory(SftpConnectionPool::CachedConnection *connection, const QString &path)
{
    DirectoryListing result;
    const QByteArray encoded = path.toUtf8();
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(SftpConnectionPool::sftp(connection),
                                                   encoded.constData());
    if (!dir) {
        result.error = QObject::tr("Cannot open remote directory %1").arg(path);
        result.connectionLost = SftpConnectionPool::connectionLost(
            connection, libssh2_session_last_errno(SftpConnectionPool::session(connection)));
        if (result.connectionLost) {
            SftpConnectionPool::reset(connection);
        }
        return result;
    }

    char name[512];
    char longentry[1024];
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir,
                                               name,
                                               sizeof(name),
                                               longentry,
                                               sizeof(longentry),
                                               &attrs);
        if (rc <= 0) {
            if (rc < 0) {
                result.error = QObject::tr("Cannot read remote directory %1").arg(path);
                result.connectionLost = SftpConnectionPool::connectionLost(connection, rc);
                result.rows.clear();
                if (result.connectionLost) {
                    SftpConnectionPool::reset(connection, dir);
                    return result;
                }
            }
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }

        const bool isDir = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

        // longentry 格式："-rw-r--r-- 1 root root 1234 Jan 01 12:00 filename"
        // 解析第3、4个空白分隔字段为 user/group
        QString owner;
        const QString le = QString::fromUtf8(longentry);
        const QStringList fields = le.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() >= 4) {
            owner = fields.at(2) + QLatin1Char('/') + fields.at(3);
        } else if (attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) {
            owner = QString::number(attrs.uid) + QLatin1Char('/') + QString::number(attrs.gid);
        }

        QVariantMap row;
        row.insert(QStringLiteral("name"), fileName);
        row.insert(QStringLiteral("path"), SftpTransfer::joinRemotePath(path, fileName));
        row.insert(QStringLiteral("isDir"), isDir);
        row.insert(QStringLiteral("size"),
                   isDir ? QStringLiteral("--") : QString::number(attrs.filesize));
        row.insert(QStringLiteral("permissions"), SftpTransfer::permissionsToOctal(attrs));
        row.insert(QStringLiteral("owner"), owner);
        row.insert(QStringLiteral("modified"),
                   attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME
                       ? QDateTime::fromSecsSinceEpoch(attrs.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                       : QString());
        result.rows.append(row);
    }

    libssh2_sftp_closedir(dir);
    return result;
}

} // namespace

QVariantList SftpDirectoryLister::list(const ConnectionProfile &profile,
                                       const QString &remotePath,
                                       QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return {};
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return {};
    }

    const QString path = remotePath.isEmpty() ? QStringLiteral("/") : remotePath;
    DirectoryListing result = readDirectory(connection, path);
    if (result.connectionLost) {
        if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
            return {};
        }
        result = readDirectory(connection, path);
    }
    if (errorOut) {
        *errorOut = result.error;
    }
    return result.rows;
}

QString SftpDirectoryLister::execute(const ConnectionProfile &profile,
                                     const QString &command,
                                     QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return QString();
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Exec);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return QString();
    }

    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(SftpConnectionPool::session(connection));
    if (!channel) {
        // Session may have gone stale (NAT/firewall silently dropped the TCP connection).
        // Reset the cached connection and try once more before giving up.
        SftpConnectionPool::reset(connection);
        if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
            return QString();
        }
        channel = libssh2_channel_open_session(SftpConnectionPool::session(connection));
        if (!channel) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot open SSH exec channel: %1")
                                .arg(SftpConnectionPool::lastError(connection));
            }
            return QString();
        }
    }

    const QByteArray encoded = command.toUtf8();
    if (libssh2_channel_exec(channel, encoded.constData()) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot execute remote command: %1")
                            .arg(SftpConnectionPool::lastError(connection));
        }
        libssh2_channel_free(channel);
        return QString();
    }

    QByteArray output;
    char buffer[4096];
    while (true) {
        const ssize_t n = libssh2_channel_read(channel, buffer, sizeof(buffer));
        if (n > 0) {
            output.append(buffer, static_cast<int>(n));
            continue;
        }
        if (n == LIBSSH2_ERROR_EAGAIN) {
            continue;
        }
        break;
    }

    QByteArray errorOutput;
    while (true) {
        const ssize_t n = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));
        if (n > 0) {
            errorOutput.append(buffer, static_cast<int>(n));
            continue;
        }
        if (n == LIBSSH2_ERROR_EAGAIN) {
            continue;
        }
        break;
    }

    libssh2_channel_close(channel);
    const int exitStatus = libssh2_channel_get_exit_status(channel);
    libssh2_channel_free(channel);

    if (exitStatus != 0 && errorOut) {
        *errorOut = QString::fromUtf8(errorOutput).trimmed();
        if (errorOut->isEmpty()) {
            *errorOut = QObject::tr("Remote command exited with status %1").arg(exitStatus);
        }
    } else if (errorOut) {
        errorOut->clear();
    }

    return QString::fromUtf8(output);
}

bool SftpDirectoryLister::upload(const ConnectionProfile &profile,
                                 const QString &localPath,
                                 const QString &remoteDirectory,
                                 QString *errorOut,
                                 ProgressCallback progress,
                                 CancelCallback isCanceled,
                                 Conflict conflict)
{
    if (cancelled(isCanceled, errorOut) || !SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Transfer);
    auto *connection = lease.get();
    if (cancelled(isCanceled, errorOut) || !SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    const QString cleanLocalPath = QDir::cleanPath(localPath);
    const QFileInfo info(cleanLocalPath);
    if (!info.exists()) {
        if (errorOut) {
            *errorOut = QObject::tr("Local path does not exist: %1").arg(localPath);
        }
        return false;
    }

    QString uploadName = info.fileName();
    if (uploadName.isEmpty() && info.isDir()) {
        uploadName = QDir(cleanLocalPath).dirName();
    }
    if (uploadName.isEmpty()) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot determine upload name for %1").arg(localPath);
        }
        return false;
    }

    const QString targetDir = remoteDirectory.isEmpty() ? QStringLiteral("/") : remoteDirectory;
    QString remotePath = SftpTransfer::joinRemotePath(targetDir, uploadName);
    if (conflict != Conflict::Overwrite) {
        LIBSSH2_SFTP_ATTRIBUTES attributes{};
        const QByteArray target = remotePath.toUtf8();
        if (libssh2_sftp_lstat(SftpConnectionPool::sftp(connection), target.constData(), &attributes) == 0) {
            if (conflict == Conflict::Skip) {
                if (errorOut) {
                    *errorOut = QObject::tr("Skipped existing destination %1").arg(remotePath);
                }
                return true;
            }
            remotePath = alternatePath(remotePath);
        }
    }
    const qint64 bytesTotal = SftpTransfer::localPathSize(info);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    auto *sftp = SftpConnectionPool::sftp(connection);
    const bool ok = SftpTransfer::uploadPathRecursive(sftp,
                                                      info.absoluteFilePath(),
                                                      remotePath,
                                                      errorOut,
                                                      bytesTotal,
                                                      &bytesDone,
                                                      progress,
                                                      isCanceled,
                                                      conflict == Conflict::Overwrite,
                                                      recoverTransfer(connection, profile, isCanceled, errorOut));
    if (ok && progress) {
        progress(bytesTotal, bytesTotal);
    }
    if (ok && errorOut) {
        errorOut->clear();
    }
    return ok;
}

bool SftpDirectoryLister::chmod(const ConnectionProfile &profile,
                                const QString &remotePath,
                                int permissions,
                                QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = static_cast<unsigned long>(permissions);

    const QByteArray encoded = remotePath.toUtf8();
    if (libssh2_sftp_setstat(SftpConnectionPool::sftp(connection), encoded.constData(), &attrs) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Failed to change permissions for %1").arg(remotePath);
        }
        return false;
    }

    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

bool SftpDirectoryLister::download(const ConnectionProfile &profile,
                                   const QString &remotePath,
                                   const QString &localDirectory,
                                   QString *downloadedPath,
                                   QString *errorOut,
                                   ProgressCallback progress,
                                   CancelCallback isCanceled,
                                   Conflict conflict)
{
    const QString name = remotePath.section(QLatin1Char('/'), -1);
    if (!name.isEmpty() && !SftpTransfer::isLocalFileName(name)) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(name);
        }
        return false;
    }
    if (cancelled(isCanceled, errorOut) || !SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Transfer);
    auto *connection = lease.get();
    if (cancelled(isCanceled, errorOut) || !SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    QString localPath = QDir(localDirectory).filePath(name.isEmpty() ? QStringLiteral("download") : name);
    const QFileInfo target(localPath);
    if (conflict != Conflict::Overwrite && (target.exists() || target.isSymLink())) {
        if (conflict == Conflict::Skip) {
            if (errorOut) {
                *errorOut = QObject::tr("Skipped existing destination %1").arg(localPath);
            }
            return true;
        }
        localPath = alternatePath(localPath);
    }
    const qint64 bytesTotal = SftpTransfer::remotePathSizeRecursive(SftpConnectionPool::sftp(connection),
                                                                    remotePath);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    auto *sftp = SftpConnectionPool::sftp(connection);
    const bool ok = SftpTransfer::downloadPathRecursive(sftp,
                                                        remotePath,
                                                        localPath,
                                                        errorOut,
                                                        bytesTotal,
                                                        &bytesDone,
                                                        progress,
                                                        isCanceled,
                                                        conflict == Conflict::Overwrite,
                                                        recoverTransfer(connection, profile, isCanceled, errorOut));
    if (ok) {
        if (progress) {
            progress(bytesTotal, bytesTotal);
        }
        if (downloadedPath) {
            *downloadedPath = localPath;
        }
        if (errorOut) {
            errorOut->clear();
        }
    }
    return ok;
}

bool SftpDirectoryLister::createDirectory(const ConnectionProfile &profile,
                                          const QString &remotePath,
                                          QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    return SftpConnectionPool::ensureConnected(connection, profile, errorOut)
           && SftpTransfer::makeRemoteDirectory(SftpConnectionPool::sftp(connection), remotePath);
}

bool SftpDirectoryLister::createFile(const ConnectionProfile &profile,
                                     const QString &remotePath,
                                     QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }
    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(SftpConnectionPool::sftp(connection),
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL,
                                                    0644);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot create remote file %1").arg(remotePath);
        }
        return false;
    }
    libssh2_sftp_close(handle);
    return true;
}

bool SftpDirectoryLister::renamePath(const ConnectionProfile &profile,
                                     const QString &oldPath,
                                     const QString &newPath,
                                     QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }
    const QByteArray oldEncoded = oldPath.toUtf8();
    const QByteArray newEncoded = newPath.toUtf8();
    if (libssh2_sftp_rename(SftpConnectionPool::sftp(connection),
                            oldEncoded.constData(),
                            newEncoded.constData()) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot rename %1").arg(oldPath);
        }
        return false;
    }
    return true;
}

bool SftpDirectoryLister::removePath(const ConnectionProfile &profile,
                                     const QString &remotePath,
                                     bool recursive,
                                     QString *errorOut)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    return SftpConnectionPool::ensureConnected(connection, profile, errorOut)
           && SftpTransfer::removePathRecursive(SftpConnectionPool::sftp(connection),
                                                remotePath, recursive, errorOut);
}
