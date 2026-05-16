#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

struct ConnectionProfile
{
    QString id;
    QString name;
    QString protocol = QStringLiteral("ssh"); // ssh, sftp, telnet
    QString host;
    int port = 22;
    QString username;
    QString authType = QStringLiteral("password"); // password, key, agent
    QString password;       // stored only when user opts in (TODO: encrypt at rest)
    QString privateKeyPath;
    QString keyPassphrase;  // 私钥密码，phase 4 接 keychain 之前先明文
    QString group;
    QString notes;
    int lastUsedEpoch = 0;
    int connectTimeoutSec = 10;
    int keepaliveSec = 30;  // libssh2_keepalive_config 间隔，<=0 关闭

    QVariantMap toVariantMap() const;
    static ConnectionProfile fromVariantMap(const QVariantMap &map);
};

class ConnectionCatalog : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionCatalog(QObject *parent = nullptr);

    void reload();
    QVector<ConnectionProfile> profiles() const;
    ConnectionProfile profileById(const QString &id) const;
    bool contains(const QString &id) const;

    bool upsert(const ConnectionProfile &profile, QString *error = nullptr);
    bool remove(const QString &id, QString *error = nullptr);

private:
    QString filePathFor(const QString &id) const;
    ConnectionProfile loadFromFile(const QString &path) const;
    bool saveToFile(const ConnectionProfile &profile, QString *error) const;
    QString rootDir() const;

    QVector<ConnectionProfile> m_profiles;
};
