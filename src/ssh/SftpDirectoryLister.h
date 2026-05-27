#pragma once

#include "ConnectionCatalog.h"

#include <functional>
#include <QVariantList>

class SftpDirectoryLister
{
public:
    using ProgressCallback = std::function<void(qint64 bytesDone, qint64 bytesTotal)>;
    using CancelCallback = std::function<bool()>;

    static QVariantList list(const ConnectionProfile &profile,
                             const QString &remotePath,
                             QString *errorOut = nullptr);
    static QString execute(const ConnectionProfile &profile,
                           const QString &command,
                           QString *errorOut = nullptr);
    static bool upload(const ConnectionProfile &profile,
                       const QString &localPath,
                       const QString &remoteDirectory,
                       QString *errorOut = nullptr,
                       ProgressCallback progress = {},
                       CancelCallback isCanceled = {});
    static bool chmod(const ConnectionProfile &profile,
                      const QString &remotePath,
                      int permissions,
                      QString *errorOut = nullptr);
    static bool download(const ConnectionProfile &profile,
                         const QString &remotePath,
                         const QString &localDirectory,
                         QString *downloadedPath = nullptr,
                         QString *errorOut = nullptr,
                         ProgressCallback progress = {},
                         CancelCallback isCanceled = {});
    static bool createDirectory(const ConnectionProfile &profile,
                                const QString &remotePath,
                                QString *errorOut = nullptr);
    static bool createFile(const ConnectionProfile &profile,
                           const QString &remotePath,
                           QString *errorOut = nullptr);
    static bool renamePath(const ConnectionProfile &profile,
                           const QString &oldPath,
                           const QString &newPath,
                           QString *errorOut = nullptr);
    static bool removePath(const ConnectionProfile &profile,
                           const QString &remotePath,
                           bool recursive,
                           QString *errorOut = nullptr);
};
