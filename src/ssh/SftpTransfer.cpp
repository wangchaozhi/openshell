#include "SftpTransfer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

namespace SftpTransfer {

namespace {

bool uploadFile(LIBSSH2_SFTP *sftp,
                const QString &localPath,
                const QString &remotePath,
                QString *errorOut,
                qint64 bytesTotal,
                qint64 *bytesDone,
                const ProgressCallback &progress,
                const CancelCallback &isCanceled)
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
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(sftp,
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_WRITE
                                                        | LIBSSH2_FXF_CREAT
                                                        | LIBSSH2_FXF_TRUNC,
                                                    0644);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot create remote file %1").arg(remotePath);
        }
        return false;
    }

    while (!file.atEnd()) {
        if (isCanceled && isCanceled()) {
            libssh2_sftp_close(handle);
            if (errorOut) {
                *errorOut = QObject::tr("Transfer cancelled");
            }
            return false;
        }
        const QByteArray chunk = file.read(32768);
        qsizetype written = 0;
        while (written < chunk.size()) {
            if (isCanceled && isCanceled()) {
                libssh2_sftp_close(handle);
                if (errorOut) {
                    *errorOut = QObject::tr("Transfer cancelled");
                }
                return false;
            }
            const ssize_t n = libssh2_sftp_write(handle,
                                                 chunk.constData() + written,
                                                 static_cast<size_t>(chunk.size() - written));
            if (n < 0) {
                libssh2_sftp_close(handle);
                if (errorOut) {
                    *errorOut = QObject::tr("Failed to write remote file %1").arg(remotePath);
                }
                return false;
            }
            written += n;
            if (bytesDone) {
                *bytesDone += n;
                if (progress) {
                    progress(*bytesDone, bytesTotal);
                }
            }
        }
    }

    libssh2_sftp_close(handle);
    return true;
}

bool isRemoteDir(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    const QByteArray encoded = remotePath.toUtf8();
    return libssh2_sftp_stat(sftp, encoded.constData(), &attrs) == 0
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

bool downloadFile(LIBSSH2_SFTP *sftp,
                  const QString &remotePath,
                  const QString &localPath,
                  QString *errorOut,
                  qint64 bytesTotal,
                  qint64 *bytesDone,
                  const ProgressCallback &progress,
                  const CancelCallback &isCanceled)
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

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        libssh2_sftp_close(handle);
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }

    char buffer[32768];
    for (;;) {
        if (isCanceled && isCanceled()) {
            libssh2_sftp_close(handle);
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
            libssh2_sftp_close(handle);
            if (errorOut) {
                *errorOut = QObject::tr("Failed to read remote file %1").arg(remotePath);
            }
            return false;
        }
        const qint64 written = file.write(buffer, n);
        if (written < 0) {
            libssh2_sftp_close(handle);
            if (errorOut) {
                *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
            }
            return false;
        }
        if (bytesDone) {
            *bytesDone += written;
            if (progress) {
                progress(*bytesDone, bytesTotal);
            }
        }
    }

    libssh2_sftp_close(handle);
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
                                                         | QDir::Readable,
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
    if (libssh2_sftp_stat(sftp, encoded.constData(), &attrs) != 0) {
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
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
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
    return libssh2_sftp_stat(sftp, encoded.constData(), &attrs) == 0
           && (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

bool uploadPathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &localPath,
                         const QString &remotePath,
                         QString *errorOut,
                         qint64 bytesTotal,
                         qint64 *bytesDone,
                         const ProgressCallback &progress,
                         const CancelCallback &isCanceled)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    const QFileInfo info(localPath);
    if (info.isDir()) {
        if (!makeRemoteDirectory(sftp, remotePath)) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot create remote directory %1").arg(remotePath);
            }
            return false;
        }

        const QDir dir(localPath);
        const QFileInfoList children = dir.entryInfoList(QDir::AllEntries
                                                             | QDir::NoDotAndDotDot
                                                             | QDir::Readable,
                                                         QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &child : children) {
            if (!uploadPathRecursive(sftp,
                                     child.absoluteFilePath(),
                                     joinRemotePath(remotePath, child.fileName()),
                                     errorOut,
                                     bytesTotal,
                                     bytesDone,
                                     progress,
                                     isCanceled)) {
                return false;
            }
        }
        return true;
    }

    return uploadFile(sftp, localPath, remotePath, errorOut, bytesTotal, bytesDone, progress, isCanceled);
}

bool downloadPathRecursive(LIBSSH2_SFTP *sftp,
                           const QString &remotePath,
                           const QString &localPath,
                           QString *errorOut,
                           qint64 bytesTotal,
                           qint64 *bytesDone,
                           const ProgressCallback &progress,
                           const CancelCallback &isCanceled)
{
    if (isCanceled && isCanceled()) {
        if (errorOut) {
            *errorOut = QObject::tr("Transfer cancelled");
        }
        return false;
    }

    if (!isRemoteDir(sftp, remotePath)) {
        return downloadFile(sftp, remotePath, localPath, errorOut, bytesTotal, bytesDone, progress, isCanceled);
    }

    QDir().mkpath(localPath);
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
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &attrs);
        if (rc <= 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }
        if (!downloadPathRecursive(sftp,
                                   joinRemotePath(remotePath, fileName),
                                   QDir(localPath).filePath(fileName),
                                   errorOut,
                                   bytesTotal,
                                   bytesDone,
                                   progress,
                                   isCanceled)) {
            libssh2_sftp_closedir(dir);
            return false;
        }
    }

    libssh2_sftp_closedir(dir);
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
