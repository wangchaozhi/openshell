#include "SftpTransfer.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QScopeGuard>
#include <QUuid>
#include <utility>

namespace SftpTransfer {

namespace {

bool uploadFile(LIBSSH2_SFTP *&sftp,
                const QString &localPath,
                const QString &remotePath,
                QString *errorOut,
                qint64 bytesTotal,
                qint64 *bytesDone,
                const ProgressCallback &progress,
                const CancelCallback &isCanceled,
                bool overwrite,
                const ReconnectCallback &reconnect)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot read local file %1").arg(localPath);
        }
        return false;
    }

    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_ATTRIBUTES destination{};
    const bool preservePermissions = overwrite && libssh2_sftp_lstat(sftp, encoded.constData(), &destination) == 0
                                     && (destination.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                                     && LIBSSH2_SFTP_S_ISREG(destination.permissions);
    const QString temporaryPath = remotePath.left(remotePath.lastIndexOf(QLatin1Char('/')) + 1)
                                  + QStringLiteral(".openshell-%1.part")
                                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray temporary = temporaryPath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(sftp,
                                                    temporary.constData(),
                                                    LIBSSH2_FXF_WRITE
                                                        | LIBSSH2_FXF_CREAT
                                                        | LIBSSH2_FXF_EXCL,
                                                    0600);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot create remote file %1").arg(remotePath);
        }
        return false;
    }
    auto discardTemporaryFile = qScopeGuard([&] {
        if (handle) {
            libssh2_sftp_close(handle);
        }
        if (sftp) {
            libssh2_sftp_unlink(sftp, temporary.constData());
        }
    });

    const qint64 expectedSize = file.size();
    const QDateTime expectedModified = QFileInfo(localPath).lastModified();
    qint64 fileBytesWritten = 0;
    char buffer[32768];
    for (;;) {
        if (isCanceled && isCanceled()) {
            if (errorOut) {
                *errorOut = QObject::tr("Transfer cancelled");
            }
            return false;
        }
        const qint64 count = file.read(buffer, sizeof(buffer));
        if (count < 0) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot read local file %1").arg(localPath);
            }
            return false;
        }
        if (count == 0) {
            break;
        }
        qint64 written = 0;
        while (written < count) {
            if (isCanceled && isCanceled()) {
                if (errorOut) {
                    *errorOut = QObject::tr("Transfer cancelled");
                }
                return false;
            }
            const ssize_t n = libssh2_sftp_write(handle,
                                                 buffer + written,
                                                 static_cast<size_t>(count - written));
            if (n <= 0) {
                if (reconnect && reconnect(static_cast<int>(n), sftp, handle)) {
                    LIBSSH2_SFTP_ATTRIBUTES partial{};
                    if (libssh2_sftp_lstat(sftp, temporary.constData(), &partial) == 0
                        && (partial.flags & LIBSSH2_SFTP_ATTR_SIZE)
                        && partial.filesize <= static_cast<quint64>(expectedSize)
                        && file.size() == expectedSize && QFileInfo(localPath).lastModified() == expectedModified
                        && file.seek(static_cast<qint64>(partial.filesize))) {
                        handle = libssh2_sftp_open(sftp, temporary.constData(), LIBSSH2_FXF_WRITE, 0);
                        if (handle) {
                            libssh2_sftp_seek64(handle, partial.filesize);
                            if (bytesDone) {
                                *bytesDone += static_cast<qint64>(partial.filesize) - fileBytesWritten;
                            }
                            fileBytesWritten = static_cast<qint64>(partial.filesize);
                            break;
                        }
                    }
                }
                if (errorOut) {
                    if (errorOut->isEmpty()) {
                        *errorOut = QObject::tr("Failed to write remote file %1").arg(remotePath);
                    }
                }
                return false;
            }
            written += n;
            fileBytesWritten += n;
            if (bytesDone) {
                *bytesDone += n;
                if (progress) {
                    progress(*bytesDone, bytesTotal);
                }
            }
        }
    }

    if (preservePermissions) {
        LIBSSH2_SFTP_ATTRIBUTES access{};
        access.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
        access.permissions = destination.permissions & 0777;
        if (libssh2_sftp_fsetstat(handle, &access) != 0) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot replace remote file %1").arg(remotePath);
            }
            return false;
        }
    }
    const int closeResult = libssh2_sftp_close(handle);
    handle = nullptr;
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }
    if (closeResult != 0 || fileBytesWritten != expectedSize
        || QFileInfo(localPath).lastModified() != expectedModified) {
        if (errorOut) {
            *errorOut = QObject::tr("Failed to write remote file %1").arg(remotePath);
        }
        return false;
    }

    int renameResult = LIBSSH2_ERROR_METHOD_NOT_SUPPORTED;
    if (overwrite) {
        renameResult = libssh2_sftp_posix_rename(sftp, temporary.constData(), encoded.constData());
    }
    if (renameResult == LIBSSH2_FX_OP_UNSUPPORTED || renameResult == LIBSSH2_ERROR_METHOD_NOT_SUPPORTED
            || (renameResult == LIBSSH2_ERROR_SFTP_PROTOCOL
                && libssh2_sftp_last_error(sftp) == LIBSSH2_FX_OP_UNSUPPORTED)) {
        renameResult = libssh2_sftp_rename_ex(sftp, temporary.constData(), temporary.size(),
                                            encoded.constData(), encoded.size(),
                                            overwrite ? LIBSSH2_SFTP_RENAME_OVERWRITE : 0);
    }
    if (renameResult != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot replace remote file %1").arg(remotePath);
        }
        return false;
    }
    discardTemporaryFile.dismiss();
    return true;
}

bool isRemoteDir(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    const QByteArray encoded = remotePath.toUtf8();
    return libssh2_sftp_lstat(sftp, encoded.constData(), &attrs) == 0
           && (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

qint64 remoteFileSize(const LIBSSH2_SFTP_ATTRIBUTES &attrs)
{
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_SIZE)) {
        return 0;
    }
    return static_cast<qint64>(attrs.filesize);
}

bool downloadFile(LIBSSH2_SFTP *&sftp,
                  const QString &remotePath,
                  const QString &localPath,
                  QString *errorOut,
                  qint64 bytesTotal,
                  qint64 *bytesDone,
                  const ProgressCallback &progress,
                  const CancelCallback &isCanceled,
                  bool overwrite,
                  const ReconnectCallback &reconnect)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(sftp,
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_READ,
                                                    0);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote file %1").arg(remotePath);
        }
        return false;
    }

    const auto closeRemoteFile = qScopeGuard([&] {
        if (handle) {
            libssh2_sftp_close(handle);
        }
    });
    LIBSSH2_SFTP_ATTRIBUTES attributes{};
    const bool sizeKnown = libssh2_sftp_fstat(handle, &attributes) == 0
                           && (attributes.flags & LIBSSH2_SFTP_ATTR_SIZE);
    const QString temporaryPath = overwrite ? localPath
        : QDir(QFileInfo(localPath).absolutePath()).filePath(
              QStringLiteral(".openshell-%1.part").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QSaveFile file(temporaryPath);
    if (QFileInfo(localPath).isSymLink() || !file.open(QIODevice::WriteOnly)) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }

    qint64 fileBytesRead = 0;
    char buffer[32768];
    for (;;) {
        if (isCanceled && isCanceled()) {
            if (errorOut) {
                *errorOut = QObject::tr("Transfer cancelled");
            }
            return false;
        }
        const ssize_t n = libssh2_sftp_read(handle, buffer, sizeof(buffer));
        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (reconnect && (fileBytesRead == 0
                              || (sizeKnown && (attributes.flags & LIBSSH2_SFTP_ATTR_ACMODTIME)))
                && reconnect(static_cast<int>(n), sftp, handle)) {
                handle = libssh2_sftp_open(sftp, encoded.constData(), LIBSSH2_FXF_READ, 0);
                if (handle && fileBytesRead == 0) {
                    continue;
                }
                LIBSSH2_SFTP_ATTRIBUTES current{};
                const auto required = LIBSSH2_SFTP_ATTR_SIZE | LIBSSH2_SFTP_ATTR_ACMODTIME;
                if (handle && libssh2_sftp_fstat(handle, &current) == 0
                    && (current.flags & required) == required && current.filesize == attributes.filesize
                    && current.mtime == attributes.mtime) {
                    libssh2_sftp_seek64(handle, static_cast<quint64>(fileBytesRead));
                    continue;
                }
            }
            if (errorOut) {
                if (errorOut->isEmpty()) {
                    *errorOut = QObject::tr("Failed to read remote file %1").arg(remotePath);
                }
            }
            return false;
        }
        const qint64 written = file.write(buffer, n);
        if (written != n) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
            }
            return false;
        }
        fileBytesRead += written;
        if (bytesDone) {
            *bytesDone += written;
            if (progress) {
                progress(*bytesDone, bytesTotal);
            }
        }
    }

    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }
    if (sizeKnown && static_cast<quint64>(fileBytesRead) != attributes.filesize) {
        if (errorOut) {
            *errorOut = QObject::tr("Failed to read remote file %1").arg(remotePath);
        }
        return false;
    }
    if (!file.commit()) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }
    if (!overwrite && !QFile::rename(temporaryPath, localPath)) {
        QFile::remove(temporaryPath);
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }
    return true;
}

} // namespace

QString joinRemotePath(const QString &base, const QString &name)
{
    if (base == QStringLiteral("/")) {
        return QStringLiteral("/") + name;
    }
    return base + QStringLiteral("/") + name;
}

bool isLocalFileName(const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")
        || name.contains(QLatin1Char('/')) || name.contains(QChar::Null)) {
        return false;
    }
#ifdef Q_OS_WIN
    return !name.contains(QLatin1Char('\\')) && !name.contains(QLatin1Char(':'))
           && !name.endsWith(QLatin1Char('.')) && !name.endsWith(QLatin1Char(' '));
#else
    return true;
#endif
}

QString permissionsToOctal(const LIBSSH2_SFTP_ATTRIBUTES &attrs)
{
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
        return QString();
    }
    return QString::number(static_cast<int>(attrs.permissions & 07777), 8).rightJustified(3, QLatin1Char('0'));
}

qint64 localPathSize(const QFileInfo &info)
{
    if (!info.exists()) {
        return 0;
    }
    if (info.isFile()) {
        return info.size();
    }
    if (!info.isDir()) {
        return 0;
    }
    qint64 total = 0;
    const QDir dir(info.absoluteFilePath());
    const QFileInfoList children = dir.entryInfoList(QDir::AllEntries
                                                         | QDir::NoDotAndDotDot
                                                         | QDir::Readable | QDir::NoSymLinks,
                                                     QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &child : children) {
        total += localPathSize(child);
    }
    return total;
}

qint64 remotePathSizeRecursive(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    const QByteArray encoded = remotePath.toUtf8();
    if (libssh2_sftp_lstat(sftp, encoded.constData(), &attrs) != 0) {
        return 0;
    }
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
            || !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        return remoteFileSize(attrs);
    }

    qint64 total = 0;
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
    if (!dir) {
        return 0;
    }
    char name[512];
    LIBSSH2_SFTP_ATTRIBUTES childAttrs{};
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &childAttrs);
        if (rc <= 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (!isLocalFileName(fileName)) {
            continue;
        }
        const QString childPath = joinRemotePath(remotePath, fileName);
        if ((childAttrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                && LIBSSH2_SFTP_S_ISDIR(childAttrs.permissions)) {
            total += remotePathSizeRecursive(sftp, childPath);
        } else {
            total += remoteFileSize(childAttrs);
        }
    }
    libssh2_sftp_closedir(dir);
    return total;
}

bool makeRemoteDirectory(LIBSSH2_SFTP *sftp, const QString &path)
{
    const QByteArray encoded = path.toUtf8();
    if (libssh2_sftp_mkdir(sftp, encoded.constData(), 0755) == 0) {
        return true;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    return libssh2_sftp_lstat(sftp, encoded.constData(), &attrs) == 0
           && (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

bool uploadPathRecursive(LIBSSH2_SFTP *&sftp,
                         const QString &localPath,
                         const QString &remotePath,
                         QString *errorOut,
                         qint64 bytesTotal,
                         qint64 *bytesDone,
                         const ProgressCallback &progress,
                         const CancelCallback &isCanceled,
                         bool overwrite,
                         const ReconnectCallback &reconnect)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    const QFileInfo info(localPath);
    if (info.isDir()) {
        const QByteArray encoded = remotePath.toUtf8();
        if (!(overwrite ? makeRemoteDirectory(sftp, remotePath)
                         : libssh2_sftp_mkdir(sftp, encoded.constData(), 0755) == 0)) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot create remote directory %1").arg(remotePath);
            }
            return false;
        }

        const QDir dir(localPath);
        const QFileInfoList children = dir.entryInfoList(QDir::AllEntries
                                                             | QDir::NoDotAndDotDot
                                                             | QDir::Readable | QDir::NoSymLinks,
                                                         QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &child : children) {
            if (!uploadPathRecursive(sftp,
                                     child.absoluteFilePath(),
                                     joinRemotePath(remotePath, child.fileName()),
                                     errorOut,
                                     bytesTotal,
                                     bytesDone,
                                     progress,
                                     isCanceled, overwrite, reconnect)) {
                return false;
            }
        }
        return true;
    }

    return uploadFile(sftp, localPath, remotePath, errorOut, bytesTotal, bytesDone,
                      progress, isCanceled, overwrite, reconnect);
}

bool downloadPathRecursive(LIBSSH2_SFTP *&sftp,
                           const QString &remotePath,
                           const QString &localPath,
                           QString *errorOut,
                           qint64 bytesTotal,
                           qint64 *bytesDone,
                           const ProgressCallback &progress,
                           const CancelCallback &isCanceled,
                           bool overwrite,
                           const ReconnectCallback &reconnect)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    if (!isRemoteDir(sftp, remotePath)) {
        return downloadFile(sftp, remotePath, localPath, errorOut, bytesTotal, bytesDone,
                            progress, isCanceled, overwrite, reconnect);
    }

    const QFileInfo localInfo(localPath);
    if (localInfo.isSymLink()
        || !(overwrite ? QDir().mkpath(localPath) : QDir(localInfo.absolutePath()).mkdir(localInfo.fileName()))) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }
    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
    if (!dir) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote directory %1").arg(remotePath);
        }
        return false;
    }

    char name[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    QStringList children;
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &attrs);
        if (rc < 0) {
            libssh2_sftp_closedir(dir);
            if (errorOut) {
                *errorOut = QObject::tr("Cannot read remote directory %1").arg(remotePath);
            }
            return false;
        }
        if (rc == 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }
        if (!isLocalFileName(fileName)) {
            libssh2_sftp_closedir(dir);
            if (errorOut) {
                *errorOut = QObject::tr("Cannot read remote directory %1").arg(remotePath);
            }
            return false;
        }
        if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) || !LIBSSH2_SFTP_S_ISLNK(attrs.permissions)) {
            children.append(fileName);
        }
    }
    libssh2_sftp_closedir(dir);
    for (const QString &fileName : std::as_const(children)) {
        if (!downloadPathRecursive(sftp,
                                   joinRemotePath(remotePath, fileName),
                                   QDir(localPath).filePath(fileName),
                                   errorOut,
                                   bytesTotal,
                                   bytesDone,
                                   progress,
                                   isCanceled, overwrite, reconnect)) {
            return false;
        }
    }

    return true;
}

bool removePathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &remotePath,
                         bool recursive,
                         QString *errorOut)
{
    const QByteArray encoded = remotePath.toUtf8();
    if (!isRemoteDir(sftp, remotePath)) {
        if (libssh2_sftp_unlink(sftp, encoded.constData()) == 0) {
            return true;
        }
        if (errorOut) {
            *errorOut = QObject::tr("Cannot delete remote file %1").arg(remotePath);
        }
        return false;
    }

    if (recursive) {
        LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
        if (!dir) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot open remote directory %1").arg(remotePath);
            }
            return false;
        }
        char name[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        for (;;) {
            const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &attrs);
            if (rc <= 0) {
                break;
            }
            const QString fileName = QString::fromUtf8(name, rc);
            if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
                continue;
            }
            if (!removePathRecursive(sftp, joinRemotePath(remotePath, fileName), true, errorOut)) {
                libssh2_sftp_closedir(dir);
                return false;
            }
        }
        libssh2_sftp_closedir(dir);
    }

    if (libssh2_sftp_rmdir(sftp, encoded.constData()) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot delete remote directory %1").arg(remotePath);
        }
        return false;
    }
    return true;
}

} // namespace SftpTransfer
