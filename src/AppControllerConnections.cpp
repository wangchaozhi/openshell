#include "AppController.h"

#include "ConnectionCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include "TrayController.h"
#include <QFileDialog>
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

QVariantList AppController::chooseConnectionImport()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    setLastError({});
    const QString path = QFileDialog::getOpenFileName(nullptr, tr("Import connections"),
                                                      QDir::home().filePath(QStringLiteral(".ssh")));
    if (path.isEmpty()) {
        return {};
    }
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly)) {
        setLastError(tr("Cannot read %1").arg(path));
        return {};
    }
    const QByteArray content = source.readAll();
    const QJsonDocument json = QJsonDocument::fromJson(content);
    QVariantList candidates;
    if (json.isObject() && json.object().value(QStringLiteral("format")).toString()
                               == QStringLiteral("OpenShellConnections")) {
        if (json.object().value(QStringLiteral("version")).toInt() != 1) {
            setLastError(tr("Unsupported connection backup version"));
            return {};
        }
        candidates = json.object().value(QStringLiteral("connections")).toArray().toVariantList();
    } else {
        const auto profiles = parseOpenSshConnections(QString::fromUtf8(content), QFileInfo(path).dir());
        for (const ConnectionProfile &profile : profiles) {
            candidates.append(profile.toVariantMap());
        }
    }
    if (candidates.isEmpty()) {
        setLastError(tr("No connections found in %1").arg(path));
    }
    return candidates;
#else
    return {};
#endif
}

int AppController::importConnectionProfiles(const QVariantList &profiles)
{
    int imported = 0;
    setLastError({});
    for (const QVariant &entry : profiles) {
        auto profile = ConnectionProfile::fromVariantMap(entry.toMap());
        profile.id.clear();
        profile.password.clear();
        profile.keyPassphrase.clear();
        profile.jumpPassword.clear();
        profile.jumpKeyPassphrase.clear();
        profile.socks5.password.clear();
        profile.socks5.username.clear();
        const auto existing = m_catalog->profiles();
        if (std::any_of(existing.cbegin(), existing.cend(), [&](const ConnectionProfile &item) {
                return item.name == profile.name && item.host == profile.host
                       && item.port == profile.port && item.username == profile.username;
            })) {
            continue;
        }
        if (!saveConnectionProfile(profile.toVariantMap())) {
            break;
        }
        ++imported;
    }
    return imported;
}

bool AppController::exportConnectionProfiles()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    setLastError({});
    const QString path = QFileDialog::getSaveFileName(nullptr, tr("Back up connections"),
                                                      QStringLiteral("openshell-connections.json"),
                                                      QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return false;
    }
    QVariantList profiles;
    for (const ConnectionProfile &profile : m_catalog->profiles()) {
        auto saved = profile.toVariantMap();
        saved.remove(QStringLiteral("id"));
        saved.remove(QStringLiteral("password"));
        saved.remove(QStringLiteral("keyPassphrase"));
        saved.remove(QStringLiteral("jumpPassword"));
        saved.remove(QStringLiteral("jumpKeyPassphrase"));
        saved.remove(QStringLiteral("socks5Password"));
        profiles.append(saved);
    }
    const QJsonDocument backup(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("OpenShellConnections")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("connections"), QJsonArray::fromVariantList(profiles)}
    });
    const QByteArray data = backup.toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        setLastError(tr("Cannot write %1").arg(path));
        return false;
    }
    return true;
#else
    return false;
#endif
}

