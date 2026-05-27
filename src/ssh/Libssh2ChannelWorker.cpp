#include "Libssh2ChannelWorker.h"

#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QSocketNotifier>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <cstring>
#include <cstdlib>

#include <libssh2.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_len_t = int;
inline int openshell_close_socket(OpenShellSocket s) { return ::closesocket(static_cast<SOCKET>(s)); }
inline int openshell_socket_errno() { return WSAGetLastError(); }
inline bool openshell_socket_would_block(int err) { return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using socket_len_t = socklen_t;
inline int openshell_close_socket(OpenShellSocket s) { return ::close(s); }
inline int openshell_socket_errno() { return errno; }
inline bool openshell_socket_would_block(int err) { return err == EWOULDBLOCK || err == EAGAIN || err == EINPROGRESS; }
#endif

// Note: SessionAbstract is declared at file scope (not in anonymous namespace)
// so that Libssh2ChannelWorker member pointers can use it across TU boundaries.
struct SessionAbstract {
    LIBSSH2_CHANNEL **jumpTunnelSlot = nullptr;
    const QByteArray *kbdintPassword = nullptr;
};

namespace {

constexpr int kReadChunk = 32 * 1024;
constexpr int kPumpFallbackMs = 250; // 兜底心跳，避免 QSocketNotifier 漏事件

#ifdef _WIN32
bool openshell_set_nonblocking(OpenShellSocket s)
{
    u_long mode = 1;
    return ::ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &mode) == 0;
}

bool openshell_set_blocking(OpenShellSocket s)
{
    u_long mode = 0;
    return ::ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &mode) == 0;
}
#else
bool openshell_set_nonblocking(OpenShellSocket s)
{
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool openshell_set_blocking(OpenShellSocket s)
{
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(s, F_SETFL, flags & ~O_NONBLOCK) == 0;
}
#endif

bool isInvalidSocket(OpenShellSocket s)
{
#ifdef _WIN32
    return s == static_cast<OpenShellSocket>(INVALID_SOCKET);
#else
    return s < 0;
#endif
}

int connectTimeoutMs(const ConnectionProfile &profile)
{
    return profile.connectTimeoutSec > 0 ? profile.connectTimeoutSec * 1000 : 10000;
}

bool resolveHostAddress(const QString &host, QHostAddress *out, QString *errorOut)
{
    // IP literal short-circuit avoids QHostInfo's AAAA / reverse-DNS overhead on LAN IPs.
    if (out->setAddress(host)) {
        return true;
    }
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot resolve host %1: %2").arg(host, info.errorString());
        }
        return false;
    }
    for (const QHostAddress &candidate : info.addresses()) {
        if (candidate.protocol() == QAbstractSocket::IPv4Protocol) {
            *out = candidate;
            return true;
        }
    }
    *out = info.addresses().first();
    return true;
}

void applyTcpNoDelay(OpenShellSocket sock)
{
    int one = 1;
    ::setsockopt(static_cast<int>(sock), IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char *>(&one), sizeof(one));
}

// OS 层 TCP keepalive：libssh2 那层握手用不上、teardown 又因为线程被强杀
// 没发出 disconnect 时，至少让内核能更早把"对端死了"探测出来并发 FIN/RST，
// 服务端 sshd 不至于挂在 ESTABLISHED 上等到 TCP 默认 2 小时才超时。
void applyTcpKeepalive(OpenShellSocket sock)
{
    int one = 1;
    ::setsockopt(static_cast<int>(sock), SOL_SOCKET, SO_KEEPALIVE,
                 reinterpret_cast<const char *>(&one), sizeof(one));
}

// libssh2 的 session abstract 是一个 void* 槽位，被 jump callbacks
// 和 kbdint callback 同时使用。统一放一个 SessionAbstract，避免互相覆盖。
ssize_t jumpSendCallback(libssh2_socket_t /*socket*/, const void *buffer, size_t length,
                         int /*flags*/, void **abstract)
{
    auto *ctx = static_cast<SessionAbstract *>(*abstract);
    LIBSSH2_CHANNEL *tunnel = (ctx && ctx->jumpTunnelSlot) ? *ctx->jumpTunnelSlot : nullptr;
    if (!tunnel) {
        errno = ECONNRESET;
        return -1;
    }
    const ssize_t rc = libssh2_channel_write(tunnel,
                                             static_cast<const char *>(buffer),
                                             length);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        errno = EAGAIN;
        return -EAGAIN;
    }
    if (rc < 0) {
        errno = EIO;
        return -1;
    }
    return rc;
}

ssize_t jumpRecvCallback(libssh2_socket_t /*socket*/, void *buffer, size_t length,
                         int /*flags*/, void **abstract)
{
    auto *ctx = static_cast<SessionAbstract *>(*abstract);
    LIBSSH2_CHANNEL *tunnel = (ctx && ctx->jumpTunnelSlot) ? *ctx->jumpTunnelSlot : nullptr;
    if (!tunnel) {
        errno = ECONNRESET;
        return -1;
    }
    const ssize_t rc = libssh2_channel_read(tunnel, static_cast<char *>(buffer), length);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        errno = EAGAIN;
        return -EAGAIN;
    }
    if (rc == 0 && libssh2_channel_eof(tunnel)) {
        return 0;
    }
    if (rc < 0) {
        errno = EIO;
        return -1;
    }
    return rc;
}

LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC(keyboardInteractiveCallback)
{
    Q_UNUSED(name);
    Q_UNUSED(name_len);
    Q_UNUSED(instruction);
    Q_UNUSED(instruction_len);
    Q_UNUSED(prompts);

    if (!abstract || !*abstract || num_prompts <= 0) {
        return;
    }

    auto *ctx = static_cast<SessionAbstract *>(*abstract);
    if (!ctx->kbdintPassword) {
        return;
    }
    const QByteArray *password = ctx->kbdintPassword;
    for (int i = 0; i < num_prompts; ++i) {
        char *copy = static_cast<char *>(std::malloc(static_cast<size_t>(password->size()) + 1));
        if (!copy) {
            responses[i].text = nullptr;
            responses[i].length = 0;
            continue;
        }
        std::memcpy(copy, password->constData(), static_cast<size_t>(password->size()));
        copy[password->size()] = '\0';
        responses[i].text = copy;
        responses[i].length = static_cast<unsigned int>(password->size());
    }
}

} // namespace

Libssh2ChannelWorker::Libssh2ChannelWorker(const ConnectionProfile &profile, QObject *parent)
    : SshChannelWorker(profile, parent)
{
}

Libssh2ChannelWorker::~Libssh2ChannelWorker()
{
    teardown();
}

void Libssh2ChannelWorker::start()
{
    if (m_running) {
        return;
    }

    QString err;
    if (libssh2_init(0) != 0) {
        emit errorOccurred(tr("libssh2_init failed"));
        emit disconnected(tr("Initialization failed"));
        return;
    }
    m_libsshInited = true;

    const bool useJump = !m_profile.jumpHost.trimmed().isEmpty();
    if (useJump) {
        if (!openSocket(m_profile.jumpHost, m_profile.jumpPort > 0 ? m_profile.jumpPort : 22, &err)
            || !openJumpAndTunnel(&err)) {
            if (!err.isEmpty()) {
                emit errorOccurred(err);
            }
            emit disconnected(err.isEmpty() ? tr("Jump host connection failed") : err);
            teardown();
            return;
        }
    } else if (!openSocket(m_profile.host, m_profile.port > 0 ? m_profile.port : 22, &err)) {
        if (!err.isEmpty()) {
            emit errorOccurred(err);
        }
        emit disconnected(err.isEmpty() ? tr("Connection failed") : err);
        teardown();
        return;
    }

    m_session = libssh2_session_init();
    if (!m_session) {
        emit errorOccurred(tr("libssh2_session_init failed"));
        emit disconnected(tr("libssh2_session_init failed"));
        teardown();
        return;
    }
    m_sessionAbstract = new SessionAbstract;
    *libssh2_session_abstract(m_session) = m_sessionAbstract;
    if (useJump) {
        installJumpCallbacks();
    }

    AuthSpec targetSpec{m_profile.username, m_profile.authType, m_profile.password,
                        m_profile.privateKeyPath, m_profile.keyPassphrase};
    if (!handshake(m_session, &err) || !authenticate(m_session, targetSpec, &err)
        || !openShell(&err)) {
        if (!err.isEmpty()) {
            emit errorOccurred(err);
        }
        emit disconnected(err.isEmpty() ? tr("Connection failed") : err);
        teardown();
        return;
    }

    m_running = true;
    setupForwarding();
    emit connected();
    schedulePump();
}

void Libssh2ChannelWorker::stop()
{
    if (!m_running && !m_session) {
        return;
    }
    m_running = false;
    teardown();
    emit disconnected(tr("Disconnected"));
}

void Libssh2ChannelWorker::sendInput(const QByteArray &data)
{
    if (!m_running) {
        return;
    }
    m_pendingInput.append(data);
    schedulePump();
}

void Libssh2ChannelWorker::resizePty(int cols, int rows)
{
    if (cols <= 0 || rows <= 0) {
        return;
    }
    m_pendingCols = cols;
    m_pendingRows = rows;
    m_pendingResize = true;
    if (m_running) {
        schedulePump();
    }
    // 未连接时 pendingResize 也得保留，连上后 openShell 会把它当作初值塞进 PTY。
}

void Libssh2ChannelWorker::pump()
{
    m_pumpScheduled = false;

    if (!m_running || !m_channel) {
        return;
    }

    bool didWork = false;

    if (m_pendingResize) {
        m_pendingResize = false;
        libssh2_channel_request_pty_size(m_channel, m_pendingCols, m_pendingRows);
    }

    if (!m_pendingInput.isEmpty()) {
        ssize_t total = 0;
        while (total < m_pendingInput.size()) {
            const ssize_t n = libssh2_channel_write(m_channel,
                                                    m_pendingInput.constData() + total,
                                                    static_cast<size_t>(m_pendingInput.size() - total));
            if (n == LIBSSH2_ERROR_EAGAIN) {
                break;
            }
            if (n < 0) {
                emit errorOccurred(lastSessionError());
                stop();
                return;
            }
            total += n;
        }
        if (total > 0) {
            m_pendingInput.remove(0, static_cast<int>(total));
            didWork = true;
        }
        if (!m_pendingInput.isEmpty()) {
            // write 没排空（对端窗口已满），稍后再来。
            schedulePump(1);
        }
    }

    char buf[kReadChunk];
    for (;;) {
        const ssize_t n = libssh2_channel_read(m_channel, buf, sizeof(buf));
        if (n == LIBSSH2_ERROR_EAGAIN) {
            break;
        }
        if (n < 0) {
            emit errorOccurred(lastSessionError());
            stop();
            return;
        }
        if (n == 0) {
            break;
        }
        emit output(QByteArray(buf, static_cast<int>(n)));
        didWork = true;
    }

    if (libssh2_channel_eof(m_channel)) {
        stop();
        return;
    }

    pumpForwards();

    if (m_profile.keepaliveSec > 0) {
        int nextSec = 0;
        if (libssh2_keepalive_send(m_session, &nextSec) < 0) {
            emit errorOccurred(lastSessionError());
            stop();
            return;
        }
    }

    if (didWork) {
        // 刚有活，立刻再问一次，避免 select 唤醒间隙丢数据
        schedulePump(0);
    } else {
        // 否则交给 QSocketNotifier 唤醒，加一个粗心跳兜底
        schedulePump(kPumpFallbackMs);
    }
}

bool Libssh2ChannelWorker::openSocket(const QString &host, int port, QString *errorOut)
{
    const QString hostStr = host.isEmpty() ? QStringLiteral("127.0.0.1") : host;
    QHostAddress addr;
    if (!resolveHostAddress(hostStr, &addr, errorOut)) {
        return false;
    }
    if (port <= 0) {
        port = 22;
    }

#ifdef _WIN32
    WSADATA wsa;
    static bool wsaInited = false;
    if (!wsaInited) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            if (errorOut) {
                *errorOut = tr("WSAStartup failed");
            }
            return false;
        }
        wsaInited = true;
    }
#endif

    const int family = (addr.protocol() == QAbstractSocket::IPv6Protocol) ? AF_INET6 : AF_INET;
    m_socket = static_cast<OpenShellSocket>(::socket(family, SOCK_STREAM, 0));
    if (isInvalidSocket(m_socket)) {
        if (errorOut) {
            *errorOut = tr("socket() failed");
        }
        return false;
    }

    if (!openshell_set_nonblocking(m_socket)) {
        if (errorOut) {
            *errorOut = tr("Failed to switch socket to non-blocking mode");
        }
        return false;
    }

    int connectRc = -1;
    if (family == AF_INET6) {
        sockaddr_in6 sa{};
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(static_cast<uint16_t>(port));
        Q_IPV6ADDR raw = addr.toIPv6Address();
        memcpy(&sa.sin6_addr, &raw, sizeof(raw));
        connectRc = ::connect(static_cast<int>(m_socket), reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
    } else {
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(static_cast<uint16_t>(port));
        sa.sin_addr.s_addr = htonl(addr.toIPv4Address());
        connectRc = ::connect(static_cast<int>(m_socket), reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
    }

    if (connectRc == 0) {
        if (!openshell_set_blocking(m_socket)) {
            if (errorOut) {
                *errorOut = tr("Failed to switch socket to blocking mode");
            }
            return false;
        }
        applyTcpNoDelay(m_socket);
        applyTcpKeepalive(m_socket);
        return true;
    }

    if (connectRc != 0 && openshell_socket_would_block(openshell_socket_errno())) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
#ifdef _WIN32
        FD_SET(static_cast<SOCKET>(m_socket), &writeSet);
#else
        FD_SET(m_socket, &writeSet);
#endif

        timeval timeout{};
        const int timeoutMs = connectTimeoutMs(m_profile);
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int waitRc = ::select(static_cast<int>(m_socket) + 1, nullptr, &writeSet, nullptr, &timeout);
        if (waitRc > 0) {
            int socketError = 0;
            socket_len_t len = sizeof(socketError);
            if (::getsockopt(static_cast<int>(m_socket),
                             SOL_SOCKET,
                             SO_ERROR,
#ifdef _WIN32
                             reinterpret_cast<char *>(&socketError),
#else
                             &socketError,
#endif
                             &len) != 0) {
                socketError = openshell_socket_errno();
            }
            if (socketError == 0) {
                if (!openshell_set_blocking(m_socket)) {
                    if (errorOut) {
                        *errorOut = tr("Failed to switch socket to blocking mode");
                    }
                    return false;
                }
                applyTcpNoDelay(m_socket);
                applyTcpKeepalive(m_socket);
                return true;
            }
            if (errorOut) {
                *errorOut = tr("connect() to %1:%2 failed (errno=%3)")
                                .arg(addr.toString(), QString::number(port),
                                     QString::number(socketError));
            }
            return false;
        }

        if (waitRc == 0) {
            if (errorOut) {
                *errorOut = tr("connect() to %1:%2 timed out after %3 seconds")
                                .arg(addr.toString(), QString::number(port),
                                     QString::number(timeoutMs / 1000));
            }
            return false;
        }
    }

    if (errorOut) {
        *errorOut = tr("connect() to %1:%2 failed (errno=%3)")
                        .arg(addr.toString(), QString::number(port),
                             QString::number(openshell_socket_errno()));
    }
    return false;
}

bool Libssh2ChannelWorker::handshake(LIBSSH2_SESSION *session, QString *errorOut)
{
    if (!session) {
        if (errorOut) {
            *errorOut = tr("libssh2 session not initialized");
        }
        return false;
    }
    libssh2_session_set_blocking(session, 1);
    // 用 jump 时 socket fd 对应跳板机连接，没法 select 出主 session 的活动；
    // 把 timeout 设到底层 socket 上仍是 OK 的，因为 callback 会接管 I/O。
    libssh2_session_set_timeout(session, connectTimeoutMs(m_profile));

    const int rc = libssh2_session_handshake(session, static_cast<int>(m_socket));
    if (rc != 0) {
        if (errorOut) {
            *errorOut = tr("SSH handshake failed: %1").arg(lastSessionError(session));
        }
        return false;
    }
    return true;
}

bool Libssh2ChannelWorker::authenticate(LIBSSH2_SESSION *session,
                                        const AuthSpec &spec,
                                        QString *errorOut)
{
    const QString type = spec.authType.isEmpty()
                             ? QStringLiteral("password")
                             : spec.authType.toLower();
    bool ok = false;
    QString err;
    if (type == QStringLiteral("password")) {
        ok = authPassword(session, spec, &err);
    } else if (type == QStringLiteral("key") || type == QStringLiteral("publickey")) {
        ok = authKey(session, spec, &err);
    } else if (type == QStringLiteral("agent")) {
        ok = authAgent(session, spec, &err);
    } else {
        err = tr("Unknown auth type '%1'").arg(type);
    }

    if (!ok && errorOut) {
        *errorOut = err.isEmpty() ? tr("Authentication failed") : err;
    }
    return ok;
}

bool Libssh2ChannelWorker::authPassword(LIBSSH2_SESSION *session,
                                        const AuthSpec &spec,
                                        QString *errorOut)
{
    const QByteArray user = spec.username.toUtf8();
    const QByteArray pwd = spec.password.toUtf8();
    const int rc = libssh2_userauth_password(session, user.constData(), pwd.constData());
    if (rc != 0) {
        if (authKeyboardInteractive(session, spec, errorOut)) {
            return true;
        }
        if (errorOut) {
            *errorOut = tr("Password auth failed: %1").arg(lastSessionError(session));
        }
        return false;
    }
    return true;
}

bool Libssh2ChannelWorker::authKeyboardInteractive(LIBSSH2_SESSION *session,
                                                   const AuthSpec &spec,
                                                   QString *errorOut)
{
    const QByteArray user = spec.username.toUtf8();
    const QByteArray pwd = spec.password.toUtf8();
    void **abstract = libssh2_session_abstract(session);
    auto *ctx = static_cast<SessionAbstract *>(*abstract);
    if (!ctx) {
        if (errorOut) {
            *errorOut = tr("session abstract not initialized");
        }
        return false;
    }
    ctx->kbdintPassword = &pwd;
    const int rc = libssh2_userauth_keyboard_interactive(session,
                                                         user.constData(),
                                                         keyboardInteractiveCallback);
    ctx->kbdintPassword = nullptr;
    if (rc != 0) {
        if (errorOut) {
            *errorOut = tr("Keyboard-interactive auth failed: %1").arg(lastSessionError(session));
        }
        return false;
    }
    return true;
}

bool Libssh2ChannelWorker::authKey(LIBSSH2_SESSION *session,
                                   const AuthSpec &spec,
                                   QString *errorOut)
{
    if (spec.privateKeyPath.isEmpty()) {
        if (errorOut) {
            *errorOut = tr("Private key path is empty");
        }
        return false;
    }
    const QByteArray user = spec.username.toUtf8();
    const QByteArray keyPath = QFile::encodeName(spec.privateKeyPath);
    const QByteArray passphrase = spec.keyPassphrase.toUtf8();
    const int rc = libssh2_userauth_publickey_fromfile(session,
                                                       user.constData(),
                                                       nullptr,
                                                       keyPath.constData(),
                                                       passphrase.isEmpty() ? nullptr
                                                                            : passphrase.constData());
    if (rc != 0) {
        if (errorOut) {
            *errorOut = tr("Key auth failed: %1").arg(lastSessionError(session));
        }
        return false;
    }
    return true;
}

bool Libssh2ChannelWorker::authAgent(LIBSSH2_SESSION *session,
                                     const AuthSpec &spec,
                                     QString *errorOut)
{
    LIBSSH2_AGENT *agent = libssh2_agent_init(session);
    if (!agent) {
        if (errorOut) {
            *errorOut = tr("libssh2_agent_init failed");
        }
        return false;
    }

    auto cleanup = [&]() {
        libssh2_agent_disconnect(agent);
        libssh2_agent_free(agent);
    };

    if (libssh2_agent_connect(agent) != 0) {
        if (errorOut) {
            *errorOut = tr("Cannot connect to ssh-agent");
        }
        cleanup();
        return false;
    }
    if (libssh2_agent_list_identities(agent) != 0) {
        if (errorOut) {
            *errorOut = tr("ssh-agent: list_identities failed");
        }
        cleanup();
        return false;
    }

    const QByteArray user = spec.username.toUtf8();
    libssh2_agent_publickey *prev = nullptr;
    libssh2_agent_publickey *cur = nullptr;
    bool ok = false;
    for (;;) {
        const int rc = libssh2_agent_get_identity(agent, &cur, prev);
        if (rc < 0) {
            break;
        }
        if (rc == 1) { // 娌℃湁鏇村 identity
            break;
        }
        if (libssh2_agent_userauth(agent, user.constData(), cur) == 0) {
            ok = true;
            break;
        }
        prev = cur;
    }

    if (!ok && errorOut) {
        *errorOut = tr("ssh-agent: no identity accepted by server");
    }
    cleanup();
    return ok;
}

bool Libssh2ChannelWorker::openShell(QString *errorOut)
{
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        if (errorOut) {
            *errorOut = tr("channel_open_session failed: %1").arg(lastSessionError());
        }
        return false;
    }

    const int initialCols = m_pendingCols > 0 ? m_pendingCols : 80;
    const int initialRows = m_pendingRows > 0 ? m_pendingRows : 24;
    if (libssh2_channel_request_pty_ex(m_channel,
                                       "xterm-256color",
                                       static_cast<unsigned int>(std::strlen("xterm-256color")),
                                       nullptr, 0,
                                       initialCols, initialRows,
                                       0, 0) != 0) {
        if (errorOut) {
            *errorOut = tr("request_pty failed: %1").arg(lastSessionError());
        }
        return false;
    }
    m_pendingResize = false;

    if (libssh2_channel_shell(m_channel) != 0) {
        if (errorOut) {
            *errorOut = tr("channel_shell failed: %1").arg(lastSessionError());
        }
        return false;
    }

    if (m_profile.keepaliveSec > 0) {
        libssh2_keepalive_config(m_session, 1, m_profile.keepaliveSec);
    }

    if (!openshell_set_nonblocking(m_socket)) {
        if (errorOut) {
            *errorOut = tr("Failed to switch socket to non-blocking mode");
        }
        return false;
    }
    libssh2_session_set_blocking(m_session, 0);
    if (m_jumpSession) {
        libssh2_session_set_blocking(m_jumpSession, 0);
    }

    delete m_readNotifier;
    m_readNotifier = new QSocketNotifier(static_cast<qintptr>(m_socket),
                                         QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this,
            [this]() { schedulePump(0); });
    m_readNotifier->setEnabled(true);

    return true;
}

void Libssh2ChannelWorker::teardown()
{
    teardownForwarding();
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if (m_channel) {
        libssh2_channel_close(m_channel);
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
    }
    if (m_session) {
        libssh2_session_disconnect(m_session, "OpenShell closing");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    if (m_jumpTunnel) {
        libssh2_channel_close(m_jumpTunnel);
        libssh2_channel_free(m_jumpTunnel);
        m_jumpTunnel = nullptr;
    }
    if (m_jumpSession) {
        libssh2_session_disconnect(m_jumpSession, "OpenShell closing");
        libssh2_session_free(m_jumpSession);
        m_jumpSession = nullptr;
    }
    delete m_sessionAbstract;
    m_sessionAbstract = nullptr;
    delete m_jumpSessionAbstract;
    m_jumpSessionAbstract = nullptr;
    if (!isInvalidSocket(m_socket)) {
        openshell_close_socket(m_socket);
        m_socket = static_cast<OpenShellSocket>(-1);
    }
    if (m_libsshInited) {
        libssh2_exit();
        m_libsshInited = false;
    }
    m_pendingInput.clear();
    m_pendingResize = false;
    m_pumpScheduled = false;
}

void Libssh2ChannelWorker::schedulePump(int delayMs)
{
    if (m_pumpScheduled) {
        return;
    }
    m_pumpScheduled = true;
    QTimer::singleShot(delayMs, this, &Libssh2ChannelWorker::pump);
}

bool Libssh2ChannelWorker::openJumpAndTunnel(QString *errorOut)
{
    m_jumpSession = libssh2_session_init();
    if (!m_jumpSession) {
        if (errorOut) {
            *errorOut = tr("libssh2_session_init (jump) failed");
        }
        return false;
    }
    m_jumpSessionAbstract = new SessionAbstract;
    *libssh2_session_abstract(m_jumpSession) = m_jumpSessionAbstract;

    if (!handshake(m_jumpSession, errorOut)) {
        return false;
    }

    AuthSpec jumpSpec{m_profile.jumpUsername.isEmpty() ? m_profile.username
                                                       : m_profile.jumpUsername,
                      m_profile.jumpAuthType, m_profile.jumpPassword,
                      m_profile.jumpPrivateKeyPath, m_profile.jumpKeyPassphrase};
    if (!authenticate(m_jumpSession, jumpSpec, errorOut)) {
        return false;
    }

    const QByteArray targetHost = m_profile.host.toUtf8();
    const int targetPort = m_profile.port > 0 ? m_profile.port : 22;
    m_jumpTunnel = libssh2_channel_direct_tcpip_ex(m_jumpSession,
                                                   targetHost.constData(),
                                                   targetPort,
                                                   "127.0.0.1", 22);
    if (!m_jumpTunnel) {
        if (errorOut) {
            *errorOut = tr("direct-tcpip via jump host failed: %1")
                            .arg(lastSessionError(m_jumpSession));
        }
        return false;
    }
    return true;
}

void Libssh2ChannelWorker::installJumpCallbacks()
{
    if (!m_session || !m_sessionAbstract) {
        return;
    }
    m_sessionAbstract->jumpTunnelSlot = &m_jumpTunnel;
    libssh2_session_callback_set2(m_session, LIBSSH2_CALLBACK_SEND,
                                  reinterpret_cast<libssh2_cb_generic *>(&jumpSendCallback));
    libssh2_session_callback_set2(m_session, LIBSSH2_CALLBACK_RECV,
                                  reinterpret_cast<libssh2_cb_generic *>(&jumpRecvCallback));
}

void Libssh2ChannelWorker::setupForwarding()
{
    for (const PortForward &spec : m_profile.forwards) {
        if (spec.type != QStringLiteral("L") || !spec.isValid()) {
            if (spec.type == QStringLiteral("R") || spec.type == QStringLiteral("D")) {
                emit errorOccurred(tr("Port forward type '%1' is not yet implemented")
                                       .arg(spec.type));
            }
            continue;
        }
        auto *server = new QTcpServer(this);
        const QHostAddress addr = spec.bindHost.isEmpty()
                                      ? QHostAddress(QHostAddress::LocalHost)
                                      : QHostAddress(spec.bindHost);
        if (!server->listen(addr, static_cast<quint16>(spec.bindPort))) {
            emit errorOccurred(tr("Local forward %1:%2 listen failed: %3")
                                   .arg(spec.bindHost,
                                        QString::number(spec.bindPort),
                                        server->errorString()));
            delete server;
            continue;
        }
        PortForward specCopy = spec;
        connect(server, &QTcpServer::newConnection, this,
                [this, specCopy, server]() { acceptForwardConnection(specCopy, server); });
        m_forwardListeners.append({server, specCopy});
    }
}

void Libssh2ChannelWorker::acceptForwardConnection(const PortForward &spec, QTcpServer *server)
{
    while (server->hasPendingConnections()) {
        QTcpSocket *sock = server->nextPendingConnection();
        if (!sock) break;
        const QByteArray rhost = spec.remoteHost.toUtf8();
        const QByteArray shost = sock->peerAddress().toString().toUtf8();
        LIBSSH2_CHANNEL *channel = libssh2_channel_direct_tcpip_ex(
            m_session,
            rhost.constData(), spec.remotePort,
            shost.isEmpty() ? "127.0.0.1" : shost.constData(),
            sock->peerPort());
        if (!channel) {
            emit errorOccurred(tr("Local forward to %1:%2 failed: %3")
                                   .arg(spec.remoteHost,
                                        QString::number(spec.remotePort),
                                        lastSessionError()));
            sock->deleteLater();
            continue;
        }
        m_forwardPairs.append({sock, channel});
        connect(sock, &QTcpSocket::readyRead, this, [this]() { schedulePump(0); });
        connect(sock, &QTcpSocket::disconnected, this, [this]() { schedulePump(0); });
    }
}

void Libssh2ChannelWorker::pumpForwards()
{
    char buf[kReadChunk];
    for (int i = m_forwardPairs.size() - 1; i >= 0; --i) {
        ForwardPair &pair = m_forwardPairs[i];
        if (!pair.channel || !pair.socket) {
            closeForwardPair(pair);
            m_forwardPairs.removeAt(i);
            continue;
        }

        // socket -> channel
        while (pair.socket->bytesAvailable() > 0) {
            const QByteArray chunk = pair.socket->read(sizeof(buf));
            if (chunk.isEmpty()) break;
            qsizetype written = 0;
            while (written < chunk.size()) {
                const ssize_t n = libssh2_channel_write(pair.channel,
                                                        chunk.constData() + written,
                                                        static_cast<size_t>(chunk.size() - written));
                if (n == LIBSSH2_ERROR_EAGAIN) {
                    schedulePump(1);
                    break;
                }
                if (n < 0) {
                    closeForwardPair(pair);
                    m_forwardPairs.removeAt(i);
                    written = -1;
                    break;
                }
                written += n;
            }
            if (written < 0) break;
        }

        if (i >= m_forwardPairs.size() || !m_forwardPairs[i].channel) continue;

        // channel -> socket
        for (;;) {
            const ssize_t n = libssh2_channel_read(pair.channel, buf, sizeof(buf));
            if (n == LIBSSH2_ERROR_EAGAIN) break;
            if (n <= 0) {
                // EOF or error → tear down this pair
                closeForwardPair(pair);
                m_forwardPairs.removeAt(i);
                break;
            }
            pair.socket->write(buf, static_cast<int>(n));
        }

        if (i < m_forwardPairs.size()
            && (m_forwardPairs[i].socket->state() == QAbstractSocket::UnconnectedState
                || libssh2_channel_eof(m_forwardPairs[i].channel))) {
            closeForwardPair(m_forwardPairs[i]);
            m_forwardPairs.removeAt(i);
        }
    }
}

void Libssh2ChannelWorker::closeForwardPair(ForwardPair &pair)
{
    if (pair.channel) {
        libssh2_channel_close(pair.channel);
        libssh2_channel_free(pair.channel);
        pair.channel = nullptr;
    }
    if (pair.socket) {
        pair.socket->disconnectFromHost();
        pair.socket->deleteLater();
        pair.socket = nullptr;
    }
}

void Libssh2ChannelWorker::teardownForwarding()
{
    for (ForwardPair &pair : m_forwardPairs) {
        closeForwardPair(pair);
    }
    m_forwardPairs.clear();
    for (ForwardListener &l : m_forwardListeners) {
        if (l.server) {
            l.server->close();
            l.server->deleteLater();
            l.server = nullptr;
        }
    }
    m_forwardListeners.clear();
}

QString Libssh2ChannelWorker::lastSessionError(LIBSSH2_SESSION *session) const
{
    LIBSSH2_SESSION *target = session ? session : m_session;
    if (!target) {
        return tr("(no session)");
    }
    char *msg = nullptr;
    int len = 0;
    libssh2_session_last_error(target, &msg, &len, 0);
    if (msg && len > 0) {
        return QString::fromUtf8(msg, len);
    }
    return tr("(unknown libssh2 error)");
}

