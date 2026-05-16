#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "ConnectionCatalog.h"

// SshChannelWorker 跑在专属 worker QThread 上。SshSession 通过 queued
// connection 触发它的 slots（start/stop/sendInput），它通过信号把数据/状态
// 异步抛回 GUI 线程。具体 SSH 实现（libssh2/echo stub 等）继承本类。
class SshChannelWorker : public QObject
{
    Q_OBJECT

public:
    explicit SshChannelWorker(const ConnectionProfile &profile, QObject *parent = nullptr);
    ~SshChannelWorker() override;

    ConnectionProfile profile() const;

public slots:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void sendInput(const QByteArray &data) = 0;
    virtual void resizePty(int cols, int rows);

signals:
    void connected();
    void disconnected(const QString &reason);
    void output(const QByteArray &chunk);
    void errorOccurred(const QString &message);

protected:
    ConnectionProfile m_profile;
};
