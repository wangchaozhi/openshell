#include "ConnectionCatalog.h"

#ifdef OPENSHELL_USE_KEYCHAIN
#include "CredentialStore.h"
#endif

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
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
    map.insert(QStringLiteral("autoReconnect"), autoReconnect);
    map.insert(QStringLiteral("reconnectMaxAttempts"), reconnectMaxAttempts);
    map.insert(QStringLiteral("reconnectInitialDelayMs"), reconnectInitialDelayMs);
    map.insert(QStringLiteral("jumpHost"), jumpHost);
    map.insert(QStringLiteral("jumpPort"), jumpPort);
    map.insert(QStringLiteral("jumpUsername"), jumpUsername);
    map.insert(QStringLiteral("jumpAuthType"), jumpAuthType);
    map.insert(QStringLiteral("jumpPassword"), jumpPassword);
    map.insert(QStringLiteral("jumpPrivateKeyPath"), jumpPrivateKeyPath);
    map.insert(QStringLiteral("jumpKeyPassphrase"), jumpKeyPassphrase);

    QVariantList fwd;
    fwd.reserve(forwards.size());
    for (const PortForward &f : forwards) {
        QVariantMap m;
        m.insert(QStringLiteral("type"), f.type);
        m.insert(QStringLiteral("bindHost"), f.bindHost);
        m.insert(QStringLiteral("bindPort"), f.bindPort);
        m.insert(QStringLiteral("remoteHost"), f.remoteHost);
        m.insert(QStringLiteral("remotePort"), f.remotePort);
        fwd.append(m);
    }
    map.insert(QStringLiteral("forwards"), fwd);
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
    p.autoReconnect = map.value(QStringLiteral("autoReconnect"), true).toBool();
    p.reconnectMaxAttempts = map.value(QStringLiteral("reconnectMaxAttempts"), 5).toInt();
    p.reconnectInitialDelayMs = map.value(QStringLiteral("reconnectInitialDelayMs"), 1000).toInt();
    p.jumpHost = map.value(QStringLiteral("jumpHost")).toString();
    p.jumpPort = map.value(QStringLiteral("jumpPort"), 22).toInt();
    p.jumpUsername = map.value(QStringLiteral("jumpUsername")).toString();
    p.jumpAuthType = map.value(QStringLiteral("jumpAuthType"), QStringLiteral("password")).toString();
    p.jumpPassword = map.value(QStringLiteral("jumpPassword")).toString();
    p.jumpPrivateKeyPath = map.value(QStringLiteral("jumpPrivateKeyPath")).toString();
    p.jumpKeyPassphrase = map.value(QStringLiteral("jumpKeyPassphrase")).toString();

    const QVariantList fwd = map.value(QStringLiteral("forwards")).toList();
    p.forwards.reserve(fwd.size());
    for (const QVariant &v : fwd) {
        const QVariantMap m = v.toMap();
        PortForward f;
        f.type = m.value(QStringLiteral("type"), QStringLiteral("L")).toString();
        f.bindHost = m.value(QStringLiteral("bindHost"), QStringLiteral("127.0.0.1")).toString();
        f.bindPort = m.value(QStringLiteral("bindPort"), 0).toInt();
        f.remoteHost = m.value(QStringLiteral("remoteHost")).toString();
        f.remotePort = m.value(QStringLiteral("remotePort"), 0).toInt();
        p.forwards.append(f);
    }
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

#ifdef OPENSHELL_USE_KEYCHAIN
    CredentialStore::remove(id, QStringLiteral("password"));
    CredentialStore::remove(id, QStringLiteral("keyPassphrase"));
    CredentialStore::remove(id, QStringLiteral("jumpPassword"));
    CredentialStore::remove(id, QStringLiteral("jumpKeyPassphrase"));
#endif

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
    const QJsonObject obj = doc.object();
    ConnectionProfile p = ConnectionProfile::fromVariantMap(obj.toVariantMap());

#ifdef OPENSHELL_USE_KEYCHAIN
    if (!p.id.isEmpty()) {
        // 旧版本把明文密码写在 JSON 里。这里优先用文件里残留的明文（保证升级
        // 不丢凭据），否则回落到钥匙串。一旦发现文件还带着 secret 字段，立即
        // 重存一次：saveToFile 会把 secret 移进钥匙串并从文件里抹掉。
        const bool hadPlainSecret = obj.contains(QStringLiteral("password"))
                                    || obj.contains(QStringLiteral("keyPassphrase"))
                                    || obj.contains(QStringLiteral("jumpPassword"))
                                    || obj.contains(QStringLiteral("jumpKeyPassphrase"));
        if (p.password.isEmpty()) {
            p.password = CredentialStore::load(p.id, QStringLiteral("password"));
        }
        if (p.keyPassphrase.isEmpty()) {
            p.keyPassphrase = CredentialStore::load(p.id, QStringLiteral("keyPassphrase"));
        }
        if (p.jumpPassword.isEmpty()) {
            p.jumpPassword = CredentialStore::load(p.id, QStringLiteral("jumpPassword"));
        }
        if (p.jumpKeyPassphrase.isEmpty()) {
            p.jumpKeyPassphrase = CredentialStore::load(p.id, QStringLiteral("jumpKeyPassphrase"));
        }
        if (hadPlainSecret) {
            saveToFile(p, nullptr);
        }
    }
#endif

    return p;
}

bool ConnectionCatalog::saveToFile(const ConnectionProfile &profile, QString *error) const
{
    QVariantMap map = profile.toVariantMap();

#ifdef OPENSHELL_USE_KEYCHAIN
    // 敏感字段进系统钥匙串，绝不落进 JSON 明文。
    if (!CredentialStore::save(profile.id, QStringLiteral("password"),
                               profile.password, error)) {
        return false;
    }
    if (!CredentialStore::save(profile.id, QStringLiteral("keyPassphrase"),
                               profile.keyPassphrase, error)) {
        return false;
    }
    if (!CredentialStore::save(profile.id, QStringLiteral("jumpPassword"),
                               profile.jumpPassword, error)) {
        return false;
    }
    if (!CredentialStore::save(profile.id, QStringLiteral("jumpKeyPassphrase"),
                               profile.jumpKeyPassphrase, error)) {
        return false;
    }
    map.remove(QStringLiteral("password"));
    map.remove(QStringLiteral("keyPassphrase"));
    map.remove(QStringLiteral("jumpPassword"));
    map.remove(QStringLiteral("jumpKeyPassphrase"));
#endif

    // QSaveFile writes to a temp file and atomically renames on commit, so a
    // crash mid-write leaves the previous connection JSON intact.
    const QString path = filePathFor(profile.id);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = tr("Cannot write %1").arg(path);
        }
        return false;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(map))
                   .toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = tr("Cannot write %1").arg(path);
        }
        return false;
    }
    return true;
}
