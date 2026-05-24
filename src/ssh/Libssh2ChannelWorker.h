#pragma once

#include <QByteArray>
#include <QString>

#include "SshChannelWorker.h"

class QSocketNotifier;
class QTcpServer;
class QTcpSocket;

#ifdef _WIN32
using OpenShellSocket = quintptr;   // SOCKET 是无符号整型
#else
using OpenShellSocket = int;
#endif

struct _LIBSSH2_SESSION;
struct _LIBSSH2_CHANNEL;
typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
typedef struct _LIBSSH2_CHANNEL LIBSSH2_CHANNEL;

// 前向声明给成员用；实现细节藏在 .cpp 的 anonymous namespace 里。
struct SessionAbstract;

// 真正的 SSH 后端：在 SshSession 创建的 worker QThread 上跑非阻塞 libssh2
// 循环。所有公开 slot 都被 queued connection 触发，所以可以放心访问内部状态。
//
// 生命周期：
//   construct (任意线程)
//   moveToThread(workerThread)  ← 由 SshSession 完成
//   start()  → 建 socket / handshake / auth / pty / shell → connected()
//   pump 循环（QTimer::singleShot 自驱）→ 读 channel → output(chunk)
//   sendInput / resizePty / stop（都是 slot）
//   析构：所有资源在 stop() 里清理；析构兜底再清一次。
class Libssh2ChannelWorker : public SshChannelWorker
{
    Q_OBJECT

public:
    explicit Libssh2ChannelWorker(const ConnectionProfile &profile, QObject *parent = nullptr);
    ~Libssh2ChannelWorker() override;

public slots:
    void start() override;
    void stop() override;
    void sendInput(const QByteArray &data) override;
    void resizePty(int cols, int rows) override;

private slots:
    void pump();

private:
    struct AuthSpec {
        QString username;
        QString authType;
        QString password;
        QString privateKeyPath;
        QString keyPassphrase;
    };

    bool openSocket(const QString &host, int port, QString *errorOut);
    bool handshake(LIBSSH2_SESSION *session, QString *errorOut);
    bool authenticate(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);
    bool openShell(QString *errorOut);
    bool authPassword(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);
    bool authKeyboardInteractive(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);
    bool authKey(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);
    bool authAgent(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);

    bool openJumpAndTunnel(QString *errorOut);
    void installJumpCallbacks();
    void teardown();
    void schedulePump(int delayMs = 0);
    QString lastSessionError(LIBSSH2_SESSION *session = nullptr) const;

    OpenShellSocket m_socket = static_cast<OpenShellSocket>(-1);
    LIBSSH2_SESSION *m_session = nullptr;
    LIBSSH2_CHANNEL *m_channel = nullptr;
    LIBSSH2_SESSION *m_jumpSession = nullptr;  // 跳板机的 SSH session
    LIBSSH2_CHANNEL *m_jumpTunnel = nullptr;   // direct-tcpip tunnel: jump -> 目标
    SessionAbstract *m_sessionAbstract = nullptr;     // 主 session 的 abstract 数据
    SessionAbstract *m_jumpSessionAbstract = nullptr; // 跳板机 session 的 abstract 数据
    QSocketNotifier *m_readNotifier = nullptr;

    // Local 端口转发：QTcpServer 接客户端连接，每个连接对应一个 direct-tcpip 通道。
    struct ForwardListener {
        QTcpServer *server = nullptr;
        PortForward spec;
    };
    struct ForwardPair {
        QTcpSocket *socket = nullptr;
        LIBSSH2_CHANNEL *channel = nullptr;
    };
    QList<ForwardListener> m_forwardListeners;
    QList<ForwardPair> m_forwardPairs;

    void setupForwarding();
    void acceptForwardConnection(const PortForward &spec, QTcpServer *server);
    void pumpForwards();
    void closeForwardPair(ForwardPair &pair);
    void teardownForwarding();

    QByteArray m_pendingInput;
    int m_pendingCols = 0;
    int m_pendingRows = 0;
    bool m_pendingResize = false;
    bool m_pumpScheduled = false;

    bool m_running = false;
    bool m_libsshInited = false;
};
