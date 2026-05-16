#include "SshSession.h"

#include "SshChannelWorker.h"
#include "../terminal/VtScreen.h"

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
    , m_screen(new VtScreen(this))
    , m_worker(worker)
{
    Q_ASSERT(worker);
    m_worker->setParent(nullptr);
    m_worker->moveToThread(&m_thread);

    // worker -> session 一律 queued，跨线程安全。
    connect(m_worker, &SshChannelWorker::connected, this, &SshSession::handleConnected);
    connect(m_worker, &SshChannelWorker::disconnected, this, &SshSession::handleDisconnected);
    connect(m_worker, &SshChannelWorker::output, this, &SshSession::handleOutput);
    connect(m_worker, &SshChannelWorker::errorOccurred, this, &SshSession::handleError);

    // VtScreen 任何变化都告诉 SessionController/QML 重绘
    connect(m_screen, &VtScreen::damaged, this, [this](const QRect &) { emit screenUpdated(); });
    connect(m_screen, &VtScreen::cursorMoved, this, &SshSession::screenUpdated);
    connect(m_screen, &VtScreen::sizeChanged, this, &SshSession::screenUpdated);
    connect(m_screen, &VtScreen::titleChanged, this,
            [this](const QString &) { emit screenUpdated(); });

    // libvterm 要发给远端的字节（键盘/鼠标响应等）走这里送回 worker
    connect(m_screen, &VtScreen::outputReady, this, &SshSession::handleScreenOutputReady);

    m_thread.start();
}

SshSession::~SshSession()
{
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
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
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
    setStatus(QStringLiteral("connected"));
}

void SshSession::handleDisconnected(const QString &reason)
{
    setStatus(QStringLiteral("disconnected"), reason);
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
