#pragma once

#include <QByteArray>
#include <QString>

#include "SshChannelWorker.h"

#ifdef _WIN32
using OpenShellSocket = quintptr;   // SOCKET 是无符号整型
#else
using OpenShellSocket = int;
#endif

struct _LIBSSH2_SESSION;
struct _LIBSSH2_CHANNEL;
typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
typedef struct _LIBSSH2_CHANNEL LIBSSH2_CHANNEL;

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
    bool openSocket(QString *errorOut);
    bool handshake(QString *errorOut);
    bool authenticate(QString *errorOut);
    bool openShell(QString *errorOut);
    bool authPassword(QString *errorOut);
    bool authKeyboardInteractive(QString *errorOut);
    bool authKey(QString *errorOut);
    bool authAgent(QString *errorOut);
    void teardown();
    void schedulePump(int delayMs = 0);
    QString lastSessionError() const;

    OpenShellSocket m_socket = static_cast<OpenShellSocket>(-1);
    LIBSSH2_SESSION *m_session = nullptr;
    LIBSSH2_CHANNEL *m_channel = nullptr;

    QByteArray m_pendingInput;
    int m_pendingCols = 0;
    int m_pendingRows = 0;
    bool m_pendingResize = false;

    bool m_running = false;
    bool m_libsshInited = false;
};
