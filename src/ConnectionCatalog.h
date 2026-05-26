#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

struct PortForward
{
    QString type = QStringLiteral("L"); // L / R / D；目前只实现了 L
    QString bindHost = QStringLiteral("127.0.0.1");
    int     bindPort = 0;
    QString remoteHost;
    int     remotePort = 0;

    bool isValid() const
    {
        if (bindPort <= 0 || bindPort > 65535) return false;
        if (type == QStringLiteral("L")) {
            return !remoteHost.isEmpty() && remotePort > 0 && remotePort <= 65535;
        }
        return false; // R / D 暂未实现
    }
};

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

    // 断线自动重连：发生非用户主动断开后，按指数退避重试。
    bool autoReconnect = true;
    int reconnectMaxAttempts = 5;       // <=0 视为关闭重连
    int reconnectInitialDelayMs = 1000; // 第 N 次延迟 = initial << (N-1)，封顶 30s

    // Telnet 专属设置。Telnet 仍然只走明文终端通道。
    bool telnetAutoLogin = true;
    QString telnetTerminalType = QStringLiteral("xterm-256color");

    // 跳板机 (ProxyJump)：通过另一台 SSH 主机 direct-tcpip 转发到目标。
    // 留空走直连。
    QString jumpHost;
    int     jumpPort = 22;
    QString jumpUsername;
    QString jumpAuthType = QStringLiteral("password"); // password / key / agent
    QString jumpPassword;
    QString jumpPrivateKeyPath;
    QString jumpKeyPassphrase;

    // 端口转发列表，目前 Libssh2ChannelWorker 仅消费 type == "L" 的条目。
    QVector<PortForward> forwards;

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
