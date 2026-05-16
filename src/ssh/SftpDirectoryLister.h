#pragma once

#include "ConnectionCatalog.h"

#include <QVariantList>

class SftpDirectoryLister
{
public:
    static QVariantList list(const ConnectionProfile &profile,
                             const QString &remotePath,
                             QString *errorOut = nullptr);
};
