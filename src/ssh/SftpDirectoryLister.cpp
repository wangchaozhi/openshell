#include "SftpDirectoryLister.h"

#include "SftpConnectionPool.h"
#include "SftpTransfer.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QVariantMap>

#include <libssh2.h>
#include <libssh2_sftp.h>

using SftpConnectionPool::Lane;

QVariantList SftpDirectoryLister::list(const ConnectionProfile &profile,
                                       const QString &remotePath,
                                       QString *errorOut)
{
    QVariantList rows;

    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return rows;
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Browse);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return rows;
    }

    const QString cleanPath = remotePath.isEmpty() ? QStringLiteral("/") : remotePath;
    const QByteArray path = cleanPath.toUtf8();
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(SftpConnectionPool::sftp(connection),
                                                    path.constData());
    if (!dir) {
        // The cached SFTP session can look valid after the SSH server drops it.
        // Rebuild the browse connection once so a manual refresh behaves like a reconnect.
        SftpConnectionPool::reset(connection);
        if (SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
            dir = libssh2_sftp_opendir(SftpConnectionPool::sftp(connection), path.constData());
        }
    }
    if (!dir) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote directory %1").arg(cleanPath);
        }
        return rows;
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
        row.insert(QStringLiteral("path"), SftpTransfer::joinRemotePath(cleanPath, fileName));
        row.insert(QStringLiteral("isDir"), isDir);
        row.insert(QStringLiteral("size"),
                   isDir ? QStringLiteral("--") : QString::number(attrs.filesize));
        row.insert(QStringLiteral("permissions"), SftpTransfer::permissionsToOctal(attrs));
        row.insert(QStringLiteral("owner"), owner);
        row.insert(QStringLiteral("modified"),
                   attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME
                       ? QDateTime::fromSecsSinceEpoch(attrs.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                       : QString());
        rows.append(row);
    }

    libssh2_sftp_closedir(dir);
    if (errorOut) {
        errorOut->clear();
    }
    return rows;
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
                                 ProgressCallback progress)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }

    auto lease = SftpConnectionPool::acquire(profile, Lane::Transfer);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    const QFileInfo info(localPath);
    if (!info.exists()) {
        if (errorOut) {
            *errorOut = QObject::tr("Local path does not exist: %1").arg(localPath);
        }
        return false;
    }

    const QString targetDir = remoteDirectory.isEmpty() ? QStringLiteral("/") : remoteDirectory;
    const QString remotePath = SftpTransfer::joinRemotePath(targetDir, info.fileName());
    const qint64 bytesTotal = SftpTransfer::localPathSize(info);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    const bool ok = SftpTransfer::uploadPathRecursive(SftpConnectionPool::sftp(connection),
                                                      info.absoluteFilePath(),
                                                      remotePath,
                                                      errorOut,
                                                      bytesTotal,
                                                      &bytesDone,
                                                      progress);
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
                                   ProgressCallback progress)
{
    if (!SftpConnectionPool::ensureLibssh2(errorOut)) {
        return false;
    }
    auto lease = SftpConnectionPool::acquire(profile, Lane::Transfer);
    auto *connection = lease.get();
    if (!SftpConnectionPool::ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    const QString name = remotePath.section(QLatin1Char('/'), -1);
    const QString localPath = QDir(localDirectory).filePath(name.isEmpty() ? QStringLiteral("download") : name);
    const qint64 bytesTotal = SftpTransfer::remotePathSizeRecursive(SftpConnectionPool::sftp(connection),
                                                                    remotePath);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    const bool ok = SftpTransfer::downloadPathRecursive(SftpConnectionPool::sftp(connection),
                                                        remotePath,
                                                        localPath,
                                                        errorOut,
                                                        bytesTotal,
                                                        &bytesDone,
                                                        progress);
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
