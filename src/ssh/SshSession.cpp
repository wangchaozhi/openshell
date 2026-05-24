#include "SshSession.h"

#include "SshChannelWorker.h"
#include "../terminal/VtScreen.h"

namespace {
constexpr int kReconnectDelayCapMs = 30 * 1000;
}

SshSession::SshSession(const QString &id,
                       const ConnectionProfile &profile,
                       SshChannelWorker *worker,
                       QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_connectionId(profile.id)
    , m_title(profile.name.isEmpty()
                  ? QStringLiteral("%1@%2").arg(profile.username, profile.host)
                  : profile.name)
    , m_status(QStringLiteral("disconnected"))
    , m_profile(profile)
    , m_screen(new VtScreen(this))
    , m_worker(worker)
{
    Q_ASSERT(worker);
    m_worker->setParent(nullptr);
    m_worker->moveToThread(&m_thread);

    // worker -> session 一律 queued，跨线程安全。
    connect(m_worker, &SshChannelWorker::connected, this, &SshSession::handleConnected);
    connect(m_worker, &SshChannelWorker::disconnected, this, &SshSession::handleDisconnected);
    // 注意：不再把 disconnected 直接挂到 m_thread.quit。线程要继续活着
    // 才能在重连时复用同一个 worker。线程只在用户主动停止时退出。
    connect(m_worker, &SshChannelWorker::output, this, &SshSession::handleOutput);
    connect(m_worker, &SshChannelWorker::errorOccurred, this, &SshSession::handleError);
    connect(&m_thread, &QThread::finished, this, &SshSession::workerThreadFinished);

    // VtScreen 任何变化都告诉 SessionController/QML 重绘
    connect(m_screen, &VtScreen::damaged, this, [this](const QRect &) { emit screenUpdated(); });
    connect(m_screen, &VtScreen::cursorMoved, this, &SshSession::screenUpdated);
    connect(m_screen, &VtScreen::sizeChanged, this, &SshSession::screenUpdated);
    connect(m_screen, &VtScreen::titleChanged, this,
            [this](const QString &) { emit screenUpdated(); });

    // libvterm 要发给远端的字节（键盘/鼠标响应等）走这里送回 worker
    connect(m_screen, &VtScreen::outputReady, this, &SshSession::handleScreenOutputReady);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_userRequestedStop || !m_worker) {
            return;
        }
        appendSessionNotice(tr("Reconnecting… (attempt %1)").arg(m_reconnectAttempt));
        setStatus(QStringLiteral("connecting"));
        QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection);
    });

    m_thread.start();
}

SshSession::~SshSession()
{
    m_reconnectTimer.stop();
    m_userRequestedStop = true;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
    }
    m_thread.quit();
    if (!m_thread.wait(2000)) {
        m_thread.terminate();
        m_thread.wait();
    }
    delete m_worker;
    m_worker = nullptr;
}

QString SshSession::id() const { return m_id; }
QString SshSession::connectionId() const { return m_connectionId; }
QString SshSession::title() const { return m_title; }
QString SshSession::status() const { return m_status; }
QString SshSession::lastMessage() const { return m_lastMessage; }
bool SshSession::isWorkerThreadRunning() const { return m_thread.isRunning(); }
QString SshSession::buffer() const { return m_screen->plainTextSnapshot(); }

void SshSession::start()
{
    setStatus(QStringLiteral("connecting"));
    if (m_pendingCols > 0 && m_pendingRows > 0) {
        QMetaObject::invokeMethod(m_worker, "resizePty", Qt::QueuedConnection,
                                  Q_ARG(int, m_pendingCols), Q_ARG(int, m_pendingRows));
    }
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection);
}

void SshSession::requestStop()
{
    m_userRequestedStop = true;
    m_reconnectTimer.stop();
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
    // 线程的 quit 由 handleDisconnected 在收到 worker 的 disconnected 后触发；
    // 这样 teardown 能在 worker 线程里跑完再退线程，避免悬挂资源。
}

void SshSession::sendInput(const QByteArray &data)
{
    QMetaObject::invokeMethod(m_worker, "sendInput", Qt::QueuedConnection,
                              Q_ARG(QByteArray, data));
}

void SshSession::requestResize(int cols, int rows)
{
    if (cols <= 0 || rows <= 0) {
        return;
    }
    m_pendingCols = cols;
    m_pendingRows = rows;
    m_screen->resize(cols, rows);
    QMetaObject::invokeMethod(m_worker, "resizePty", Qt::QueuedConnection,
                              Q_ARG(int, cols), Q_ARG(int, rows));
}

void SshSession::clearScreen()
{
    m_screen->clear();
}

void SshSession::handleConnected()
{
    m_reconnectAttempt = 0;
    setStatus(QStringLiteral("connected"));
}

void SshSession::handleDisconnected(const QString &reason)
{
    if (m_status != QStringLiteral("disconnected")) {
        appendSessionNotice(tr("Connection disconnected"));
    }

    if (!m_userRequestedStop && m_profile.autoReconnect
        && m_profile.reconnectMaxAttempts > 0
        && m_reconnectAttempt < m_profile.reconnectMaxAttempts) {
        scheduleReconnect();
        setStatus(QStringLiteral("reconnecting"), reason);
        return;
    }

    setStatus(QStringLiteral("disconnected"), reason);

    if (m_userRequestedStop) {
        m_thread.quit();
    }
}

void SshSession::handleOutput(const QByteArray &chunk)
{
    m_screen->feed(chunk);
}

void SshSession::handleError(const QString &message)
{
    setStatus(QStringLiteral("error"), message);
}

void SshSession::handleScreenOutputReady()
{
    const QByteArray data = m_screen->takePendingOutput();
    if (data.isEmpty() || !m_worker) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, "sendInput", Qt::QueuedConnection,
                              Q_ARG(QByteArray, data));
}

void SshSession::setStatus(const QString &status, const QString &message)
{
    const bool statusChangedActually = (m_status != status);
    const bool messageChangedActually = (!message.isNull() && m_lastMessage != message);
    if (!statusChangedActually && !messageChangedActually) {
        return;
    }
    m_status = status;
    if (!message.isNull()) {
        m_lastMessage = message;
    }
    emit statusChanged();
}

void SshSession::appendSessionNotice(const QString &text)
{
    if (!m_screen || text.isEmpty()) {
        return;
    }
    const QString line = QStringLiteral("\r\n%1\r\n").arg(text);
    m_screen->feed(line.toUtf8());
}

void SshSession::scheduleReconnect()
{
    ++m_reconnectAttempt;
    const int base = m_profile.reconnectInitialDelayMs > 0
                         ? m_profile.reconnectInitialDelayMs
                         : 1000;
    qint64 delay = static_cast<qint64>(base) << (m_reconnectAttempt - 1);
    if (delay > kReconnectDelayCapMs) {
        delay = kReconnectDelayCapMs;
    }
    m_reconnectTimer.start(static_cast<int>(delay));
}
