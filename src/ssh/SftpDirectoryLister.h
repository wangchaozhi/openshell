#pragma once

#include "ConnectionCatalog.h"

#include <QVariantList>

class SftpDirectoryLister
{
public:
    static QVariantList list(const ConnectionProfile &profile,
                             const QString &remotePath,
                             QString *errorOut = nullptr);
    static bool upload(const ConnectionProfile &profile,
                       const QString &localPath,
                       const QString &remoteDirectory,
                       QString *errorOut = nullptr);
    static bool chmod(const ConnectionProfile &profile,
                      const QString &remotePath,
                      int permissions,
                      QString *errorOut = nullptr);
    static bool download(const ConnectionProfile &profile,
                         const QString &remotePath,
                         const QString &localDirectory,
                         QString *downloadedPath = nullptr,
                         QString *errorOut = nullptr);
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
