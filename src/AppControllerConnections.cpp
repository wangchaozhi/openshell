#include "AppController.h"

#include "ConnectionCatalog.h"

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include "TrayController.h"
#endif

QVariantList AppController::connectionProfiles() const
{
    QVariantList list;
    for (const ConnectionProfile &profile : m_catalog->profiles()) {
        list.append(profile.toVariantMap());
    }
    return list;
}

QVariantList AppController::reloadConnectionProfiles()
{
    m_catalog->reload();
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_tray) {
        m_tray->setConnections(m_catalog->profiles());
    }
#endif
    return connectionProfiles();
}

bool AppController::saveConnectionProfile(const QVariantMap &profile)
{
    QString error;
    if (!m_catalog->upsert(ConnectionProfile::fromVariantMap(profile), &error)) {
        setLastError(error);
        return false;
    }

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_tray) {
        m_tray->setConnections(m_catalog->profiles());
    }
#endif
    setLastError(QString());
    return true;
}

bool AppController::deleteConnection(const QString &id)
{
    QString error;
    if (!m_catalog->remove(id, &error)) {
        setLastError(error);
        return false;
    }

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_tray) {
        m_tray->setConnections(m_catalog->profiles());
    }
#endif
    if (m_currentConnectionId == id) {
        setCurrentConnectionId(QString());
    }
    setLastError(QString());
    return true;
}

