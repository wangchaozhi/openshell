#pragma once

#include "ssh/SshChannelWorker.h"

// EchoChannelWorker 是用来在没有真实 SSH 服务可用时（单测 / 开发调试）走通
// SessionController → SshSession → Worker 信号管道的本地后端：
// 启动时发欢迎横幅，把用户输入按行回显，遇到 "exit" 模拟断开。
//
// 生产构建里不应该出现这个类；它只在 tests/ 目录被编译进 test_session_controller
// 以及（可选）由 OPENSHELL_ENABLE_ECHO_BACKEND=ON 打开的开发版主程序。
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
