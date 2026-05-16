#include "SshSession.h"

#include "SshChannelWorker.h"

namespace {

bool applyTerminalOutput(QByteArray *buffer, const QByteArray &input)
{
    enum class State { Normal, Escape, Csi, Osc, OscEscape, Charset };

    if (!buffer) {
        return false;
    }

    const QByteArray before = *buffer;
    qsizetype cursor = buffer->size();
    qsizetype lineStart = buffer->lastIndexOf('\n') + 1;
    State state = State::Normal;
    QByteArray csi;

    for (unsigned char ch : input) {
        switch (state) {
        case State::Normal:
            if (ch == 0x1b) {
                state = State::Escape;
            } else if (ch == '\r') {
                cursor = lineStart;
            } else if (ch == '\n') {
                buffer->append('\n');
                cursor = buffer->size();
                lineStart = cursor;
            } else if (ch == '\b' || ch == 0x7f) {
                if (cursor > lineStart) {
                    buffer->remove(cursor - 1, 1);
                    --cursor;
                }
            } else if (ch == '\t' || ch >= 0x20) {
                if (cursor < buffer->size()) {
                    (*buffer)[cursor] = static_cast<char>(ch);
                } else {
                    buffer->append(static_cast<char>(ch));
                }
                ++cursor;
            }
            break;
        case State::Escape:
            if (ch == '[') {
                csi.clear();
                state = State::Csi;
            } else if (ch == ']') {
                state = State::Osc;
            } else if (ch == '(' || ch == ')') {
                state = State::Charset;
            } else {
                state = State::Normal;
            }
            break;
        case State::Csi:
            csi.append(static_cast<char>(ch));
            if (ch >= 0x40 && ch <= 0x7e) {
                if (ch == 'm') {
                    buffer->append("\x1b[");
                    buffer->append(csi);
                    cursor = buffer->size();
                }
                state = State::Normal;
            }
            break;
        case State::Osc:
            if (ch == 0x07) {
                state = State::Normal;
            } else if (ch == 0x1b) {
                state = State::OscEscape;
            }
            break;
        case State::OscEscape:
            state = State::Normal;
            break;
        case State::Charset:
            state = State::Normal;
            break;
        }
    }

    return *buffer != before;
}

} // namespace

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
    // worker 线程已退出，没人再访问 m_worker，可以直接销毁。
    delete m_worker;
    m_worker = nullptr;
}

QString SshSession::id() const { return m_id; }
QString SshSession::connectionId() const { return m_connectionId; }
QString SshSession::title() const { return m_title; }
QString SshSession::status() const { return m_status; }
QString SshSession::lastMessage() const { return m_lastMessage; }
QString SshSession::buffer() const { return QString::fromUtf8(m_buffer); }
qsizetype SshSession::bufferSize() const { return m_buffer.size(); }

void SshSession::start()
{
    setStatus(QStringLiteral("connecting"));
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
    QMetaObject::invokeMethod(m_worker, "resizePty", Qt::QueuedConnection,
                              Q_ARG(int, cols), Q_ARG(int, rows));
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
    if (!applyTerminalOutput(&m_buffer, chunk)) {
        return;
    }
    if (m_buffer.size() > kBufferCap) {
        m_buffer.remove(0, m_buffer.size() - kBufferCap / 2);
    }
    emit outputAppended(chunk);
}

void SshSession::handleError(const QString &message)
{
    setStatus(QStringLiteral("error"), message);
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

void SshSession::appendBuffer(const QByteArray &chunk)
{
    m_buffer.append(chunk);
    if (m_buffer.size() > kBufferCap) {
        // 截掉前面一半，保留较新输出。这是 stub 终端的最简策略，
        // 真 vt100 渲染器接入后会自带滚屏缓冲，本类的累积只用于切 tab 重放。
        m_buffer.remove(0, m_buffer.size() - kBufferCap / 2);
    }
}
