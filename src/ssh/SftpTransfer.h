#pragma once

#include "SftpDirectoryLister.h" // for SftpDirectoryLister::ProgressCallback

#include <QString>

#include <libssh2.h>
#include <libssh2_sftp.h>

class QFileInfo;

// Recursive SFTP file/directory transfer and path operations. These work on a
// ready libssh2 SFTP channel (see SftpConnectionPool) and report byte progress.
namespace SftpTransfer {

using ProgressCallback = SftpDirectoryLister::ProgressCallback;

// "/a" + "b" -> "/a/b"; handles the root directory special case.
QString joinRemotePath(const QString &base, const QString &name);

// Octal permission string ("644") from SFTP attributes, or empty if unavailable.
QString permissionsToOctal(const LIBSSH2_SFTP_ATTRIBUTES &attrs);

// Total byte size of a local file or directory tree.
qint64 localPathSize(const QFileInfo &info);

// Recursive byte size of a remote file or directory tree.
qint64 remotePathSizeRecursive(LIBSSH2_SFTP *sftp, const QString &remotePath);

// Creates a remote directory, succeeding if it already exists as a directory.
bool makeRemoteDirectory(LIBSSH2_SFTP *sftp, const QString &path);

bool uploadPathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &localPath,
                         const QString &remotePath,
                         QString *errorOut,
                         qint64 bytesTotal,
                         qint64 *bytesDone,
                         const ProgressCallback &progress);

bool downloadPathRecursive(LIBSSH2_SFTP *sftp,
                           const QString &remotePath,
                           const QString &localPath,
                           QString *errorOut,
                           qint64 bytesTotal,
                           qint64 *bytesDone,
                           const ProgressCallback &progress);

bool removePathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &remotePath,
                         bool recursive,
                         QString *errorOut);

} // namespace SftpTransfer
