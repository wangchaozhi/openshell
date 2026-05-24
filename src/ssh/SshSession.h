#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>

#include "ConnectionCatalog.h"

class SshChannelWorker;
class VtScreen;

// SshSession 把一个 worker thread + 一个 SshChannelWorker 打包起来。
// SessionController 持有它，QML 通过 sessionId 来引用它。
// 状态机：disconnected -> connecting -> connected -> disconnected/error。
//
// 终端模型由本类持有一个 VtScreen（跑在 GUI 线程）：worker 线程的输出 chunk
// 通过 queued connection 投递到 GUI 线程后喂给 VtScreen；vterm 要回写给远端
// 的字节通过 VtScreen::outputReady 信号反向汇聚到本类，再调度回 worker。
class SshSession : public QObject
{
    Q_OBJECT

public:
    SshSession(const QString &id,
               const ConnectionProfile &profile,
               SshChannelWorker *worker,
               QObject *parent = nullptr);
    ~SshSession() override;

    QString id() const;
    QString connectionId() const;
    QString title() const;
    QString status() const; // connecting/connected/disconnected/error
    QString lastMessage() const;
    bool isWorkerThreadRunning() const;

    // 纯文本快照（行末空白裁掉），仅用于切 tab 重放与单测断言。
    QString buffer() const;

    VtScreen *screen() const { return m_screen; }

    void start();
    void requestStop();
    void sendInput(const QByteArray &data);
    void requestResize(int cols, int rows);
    void clearScreen();

signals:
    void statusChanged();
    void screenUpdated(); // 终端模型已变更（光标/cells/title 等任意维度）
    void workerThreadFinished();

private slots:
    void handleConnected();
    void handleDisconnected(const QString &reason);
    void handleOutput(const QByteArray &chunk);
    void handleError(const QString &message);
    void handleScreenOutputReady();

private:
    void setStatus(const QString &status, const QString &message = QString());
    void appendSessionNotice(const QString &text);

    void scheduleReconnect();

    QString m_id;
    QString m_connectionId;
    QString m_title;
    QString m_status;
    QString m_lastMessage;

    ConnectionProfile m_profile;
    VtScreen *m_screen = nullptr; // GUI 线程
    QThread m_thread;
    SshChannelWorker *m_worker = nullptr; // worker 线程

    int m_pendingCols = 0;
    int m_pendingRows = 0;

    bool m_userRequestedStop = false;
    int m_reconnectAttempt = 0;
    QTimer m_reconnectTimer;
};
