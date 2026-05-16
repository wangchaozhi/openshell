#pragma once

#include "SshChannelWorker.h"

// EchoChannelWorker 是用来证明信号管道走通的本地后端：
// 发出欢迎横幅、把用户输入回显到 output，遇到 "exit" 模拟断开。
// 真正的 SSH 后端（Libssh2ChannelWorker）会替换本类。
class EchoChannelWorker : public SshChannelWorker
{
    Q_OBJECT

public:
    explicit EchoChannelWorker(const ConnectionProfile &profile, QObject *parent = nullptr);

public slots:
    void start() override;
    void stop() override;
    void sendInput(const QByteArray &data) override;

private:
    void emitPrompt();

    bool m_running = false;
    QByteArray m_lineBuffer;
};
