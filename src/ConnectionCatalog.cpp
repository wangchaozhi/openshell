#include "ConnectionCatalog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

QVariantMap ConnectionProfile::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("protocol"), protocol);
    map.insert(QStringLiteral("host"), host);
    map.insert(QStringLiteral("port"), port);
    map.insert(QStringLiteral("username"), username);
    map.insert(QStringLiteral("authType"), authType);
    map.insert(QStringLiteral("password"), password);
    map.insert(QStringLiteral("privateKeyPath"), privateKeyPath);
    map.insert(QStringLiteral("keyPassphrase"), keyPassphrase);
    map.insert(QStringLiteral("group"), group);
    map.insert(QStringLiteral("notes"), notes);
    map.insert(QStringLiteral("lastUsedEpoch"), lastUsedEpoch);
    map.insert(QStringLiteral("connectTimeoutSec"), connectTimeoutSec);
    map.insert(QStringLiteral("keepaliveSec"), keepaliveSec);
    return map;
}

ConnectionProfile ConnectionProfile::fromVariantMap(const QVariantMap &map)
{
    ConnectionProfile p;
    p.id = map.value(QStringLiteral("id")).toString();
    p.name = map.value(QStringLiteral("name")).toString();
    p.protocol = map.value(QStringLiteral("protocol"), QStringLiteral("ssh")).toString();
    p.host = map.value(QStringLiteral("host")).toString();
    p.port = map.value(QStringLiteral("port"), 22).toInt();
    p.username = map.value(QStringLiteral("username")).toString();
    p.authType = map.value(QStringLiteral("authType"), QStringLiteral("password")).toString();
    p.password = map.value(QStringLiteral("password")).toString();
    p.privateKeyPath = map.value(QStringLiteral("privateKeyPath")).toString();
    p.keyPassphrase = map.value(QStringLiteral("keyPassphrase")).toString();
    p.group = map.value(QStringLiteral("group")).toString();
    p.notes = map.value(QStringLiteral("notes")).toString();
    p.lastUsedEpoch = map.value(QStringLiteral("lastUsedEpoch"), 0).toInt();
    p.connectTimeoutSec = map.value(QStringLiteral("connectTimeoutSec"), 10).toInt();
    p.keepaliveSec = map.value(QStringLiteral("keepaliveSec"), 30).toInt();
    return p;
}

ConnectionCatalog::ConnectionCatalog(QObject *parent)
    : QObject(parent)
{
    reload();
}

QString ConnectionCatalog::rootDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir = QDir(base).filePath(QStringLiteral("connections"));
    QDir().mkpath(dir);
    return dir;
}

QString ConnectionCatalog::filePathFor(const QString &id) const
{
    return QDir(rootDir()).filePath(id + QStringLiteral(".json"));
}

void ConnectionCatalog::reload()
{
    m_profiles.clear();
    const QDir dir(rootDir());
    const QFileInfoList entries = dir.entryInfoList(QStringList{QStringLiteral("*.json")},
                                                    QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const ConnectionProfile p = loadFromFile(entry.absoluteFilePath());
        if (!p.id.isEmpty()) {
            m_profiles.append(p);
        }
    }
}

QVector<ConnectionProfile> ConnectionCatalog::profiles() const
{
    return m_profiles;
}

ConnectionProfile ConnectionCatalog::profileById(const QString &id) const
{
    for (const ConnectionProfile &p : m_profiles) {
        if (p.id == id) {
            return p;
        }
    }
    return {};
}

bool ConnectionCatalog::contains(const QString &id) const
{
    for (const ConnectionProfile &p : m_profiles) {
        if (p.id == id) {
            return true;
        }
    }
    return false;
}

bool ConnectionCatalog::upsert(const ConnectionProfile &profile, QString *error)
{
    ConnectionProfile p = profile;
    if (p.name.trimmed().isEmpty()) {
        if (error) {
            *error = tr("Connection name is required");
        }
        return false;
    }
    if (p.host.trimmed().isEmpty()) {
        if (error) {
            *error = tr("Host is required");
        }
        return false;
    }
    if (p.id.isEmpty()) {
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (!saveToFile(p, error)) {
        return false;
    }

    reload();
    return true;
}

bool ConnectionCatalog::remove(const QString &id, QString *error)
{
    if (!contains(id)) {
        if (error) {
            *error = tr("Unknown connection");
        }
        return false;
    }

    const QString path = filePathFor(id);
    if (!QFile::remove(path)) {
        if (error) {
            *error = tr("Failed to delete connection file");
        }
        return false;
    }

    reload();
    return true;
}

ConnectionProfile ConnectionCatalog::loadFromFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) {
        return {};
    }
    return ConnectionProfile::fromVariantMap(doc.object().toVariantMap());
}

bool ConnectionCatalog::saveToFile(const ConnectionProfile &profile, QString *error) const
{
    const QString path = filePathFor(profile.id);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = tr("Cannot write %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(profile.toVariantMap()))
                   .toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
