#pragma once

#include <functional>

#include "ConnectionCatalog.h"

class SshChannelWorker;

// SessionController 通过这个工厂决定 worker 的具体实现：
// 默认生产构建注入 Libssh2 backend；测试/调试构建可以注入 Echo backend。
// 工厂返回的对象所有权交给调用者（SshSession 会接管并 moveToThread）。
using SshWorkerFactory = std::function<SshChannelWorker *(const ConnectionProfile &)>;
