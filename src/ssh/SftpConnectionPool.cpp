#include "SftpConnectionPool.h"

#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QHostInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QVector>

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SftpSocket = SOCKET;
inline int close_sftp_socket(SftpSocket s) { return ::closesocket(s); }
inline int sftp_socket_errno() { return WSAGetLastError(); }
inline bool sftp_socket_would_block(int err) { return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS; }
inline SftpSocket invalid_sftp_socket() { return INVALID_SOCKET; }
inline bool is_invalid_sftp_socket(SftpSocket s) { return s == INVALID_SOCKET; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SftpSocket = int;
inline int close_sftp_socket(SftpSocket s) { return ::close(s); }
inline int sftp_socket_errno() { return errno; }
inline bool sftp_socket_would_block(int err) { return err == EWOULDBLOCK || err == EAGAIN || err == EINPROGRESS; }
inline SftpSocket invalid_sftp_socket() { return -1; }
inline bool is_invalid_sftp_socket(SftpSocket s) { return s < 0; }
#endif

namespace SftpConnectionPool {

struct CachedConnection
{
    SftpSocket sock = invalid_sftp_socket();
    LIBSSH2_SESSION *session = nullptr;
    LIBSSH2_SFTP *sftp = nullptr;
    QMutex mutex; // serializes work on this single libssh2 session (not session-thread-safe)
};

namespace {

// Tiny mutex protecting only the cache map lookup/insert — long-running SFTP work
// runs under the per-connection mutex instead, so different connections don't block each other.
QMutex &cacheMapMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, QVector<CachedConnection *>> &connectionCache()
{
    static QHash<QString, QVector<CachedConnection *>> cache;
    return cache;
}

QHash<QString, int> &connectionWaitCursor()
{
    static QHash<QString, int> cursors;
    return cursors;
}

QString laneName(Lane lane)
{
    switch (lane) {
    case Lane::Browse:
        return QStringLiteral("browse");
    case Lane::Transfer:
        return QStringLiteral("transfer");
    case Lane::Exec:
        return QStringLiteral("exec");
    }
    return QStringLiteral("browse");
}

int lanePoolSize(Lane lane)
{
    switch (lane) {
    case Lane::Browse:
        return 2;
    case Lane::Transfer:
        return 4;
    case Lane::Exec:
        return 1;
    }
    return 1;
}

QString cacheKey(const ConnectionProfile &profile, Lane lane)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(profile.id,
             profile.host,
             QString::number(profile.port),
             profile.username,
             profile.authType,
             profile.privateKeyPath,
             laneName(lane));
}

LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC(kbdInteractiveCallback)
{
    Q_UNUSED(name);
    Q_UNUSED(name_len);
    Q_UNUSED(instruction);
    Q_UNUSED(instruction_len);
    Q_UNUSED(prompts);

    if (!abstract || !*abstract || num_prompts <= 0) {
        return;
    }

    const auto *password = static_cast<const QByteArray *>(*abstract);
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

QString lastError(LIBSSH2_SESSION *session)
{
    if (!session) {
        return QStringLiteral("(no session)");
    }
    char *msg = nullptr;
    int len = 0;
    libssh2_session_last_error(session, &msg, &len, 0);
    if (msg && len > 0) {
        return QString::fromUtf8(msg, len);
    }
    return QStringLiteral("(unknown libssh2 error)");
}

int connectTimeoutMs(const ConnectionProfile &profile)
{
    return profile.connectTimeoutSec > 0 ? profile.connectTimeoutSec * 1000 : 10000;
}

#ifdef _WIN32
bool setSocketNonBlocking(SftpSocket s)
{
    u_long mode = 1;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}

bool setSocketBlocking(SftpSocket s)
{
    u_long mode = 0;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}
#else
bool setSocketNonBlocking(SftpSocket s)
{
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool setSocketBlocking(SftpSocket s)
{
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(s, F_SETFL, flags & ~O_NONBLOCK) == 0;
}
#endif

bool resolveHostAddress(const QString &host, QHostAddress *out, QString *errorOut)
{
    // IP literal short-circuit: skip the resolver entirely so LAN IPs don't pay
    // the AAAA/reverse-lookup tax that QHostInfo::fromName can incur on Windows.
    if (out->setAddress(host)) {
        return true;
    }
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot resolve host %1: %2")
                            .arg(host, info.errorString());
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

void applyTcpNoDelay(SftpSocket sock)
{
    int one = 1;
    ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char *>(&one), sizeof(one));
}

bool openSocket(const ConnectionProfile &profile, SftpSocket *socketOut, QString *errorOut)
{
    QHostAddress addr;
    if (!resolveHostAddress(profile.host, &addr, errorOut)) {
        return false;
    }

#ifdef _WIN32
    static bool wsaStarted = false;
    if (!wsaStarted) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            if (errorOut) {
                *errorOut = QObject::tr("WSAStartup failed");
            }
            return false;
        }
        wsaStarted = true;
    }
#endif

    const int family = addr.protocol() == QAbstractSocket::IPv6Protocol ? AF_INET6 : AF_INET;
    SftpSocket sock = ::socket(family, SOCK_STREAM, 0);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
#else
    if (sock < 0) {
#endif
        if (errorOut) {
            *errorOut = QObject::tr("socket() failed");
        }
        return false;
    }

    if (!setSocketNonBlocking(sock)) {
        close_sftp_socket(sock);
        if (errorOut) {
            *errorOut = QObject::tr("Failed to switch SFTP socket to non-blocking mode");
        }
        return false;
    }

    const int port = profile.port > 0 ? profile.port : 22;
    int rc = -1;
    if (family == AF_INET6) {
        sockaddr_in6 sa{};
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(static_cast<uint16_t>(port));
        Q_IPV6ADDR raw = addr.toIPv6Address();
        std::memcpy(&sa.sin6_addr, &raw, sizeof(raw));
        rc = ::connect(sock, reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
    } else {
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(static_cast<uint16_t>(port));
        sa.sin_addr.s_addr = htonl(addr.toIPv4Address());
        rc = ::connect(sock, reinterpret_cast<sockaddr *>(&sa), sizeof(sa));
    }

    if (rc == 0) {
        if (!setSocketBlocking(sock)) {
            close_sftp_socket(sock);
            if (errorOut) {
                *errorOut = QObject::tr("Failed to switch SFTP socket to blocking mode");
            }
            return false;
        }
        applyTcpNoDelay(sock);
        *socketOut = sock;
        return true;
    }

    if (sftp_socket_would_block(sftp_socket_errno())) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);

        timeval timeout{};
        const int timeoutMs = connectTimeoutMs(profile);
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int waitRc = ::select(static_cast<int>(sock) + 1, nullptr, &writeSet, nullptr, &timeout);
        if (waitRc > 0) {
            int socketError = 0;
#ifdef _WIN32
            int len = sizeof(socketError);
            const int optRc = ::getsockopt(sock, SOL_SOCKET, SO_ERROR,
                                           reinterpret_cast<char *>(&socketError), &len);
#else
            socklen_t len = sizeof(socketError);
            const int optRc = ::getsockopt(sock, SOL_SOCKET, SO_ERROR, &socketError, &len);
#endif
            if (optRc != 0) {
                socketError = sftp_socket_errno();
            }
            if (socketError == 0) {
                if (!setSocketBlocking(sock)) {
                    close_sftp_socket(sock);
                    if (errorOut) {
                        *errorOut = QObject::tr("Failed to switch SFTP socket to blocking mode");
                    }
                    return false;
                }
                applyTcpNoDelay(sock);
                *socketOut = sock;
                return true;
            }
            close_sftp_socket(sock);
            if (errorOut) {
                *errorOut = QObject::tr("connect() to %1:%2 failed (errno=%3)")
                                .arg(addr.toString(), QString::number(port),
                                     QString::number(socketError));
            }
            return false;
        }

        close_sftp_socket(sock);
        if (errorOut) {
            *errorOut = waitRc == 0
                            ? QObject::tr("connect() to %1:%2 timed out after %3 seconds")
                                  .arg(addr.toString(), QString::number(port),
                                       QString::number(timeoutMs / 1000))
                            : QObject::tr("connect() to %1:%2 failed (errno=%3)")
                                  .arg(addr.toString(), QString::number(port),
                                       QString::number(sftp_socket_errno()));
        }
        return false;
    }

    {
        const int err = sftp_socket_errno();
        close_sftp_socket(sock);
        if (errorOut) {
            *errorOut = QObject::tr("connect() to %1:%2 failed (errno=%3)")
                            .arg(addr.toString(), QString::number(port),
                                 QString::number(err));
        }
        return false;
    }
}

bool authenticate(LIBSSH2_SESSION *session, const ConnectionProfile &profile, QString *errorOut)
{
    const QByteArray user = profile.username.toUtf8();
    const QByteArray password = profile.password.toUtf8();
    const QString authType = profile.authType.isEmpty()
                                 ? QStringLiteral("password")
                                 : profile.authType.toLower();

    if (authType == QStringLiteral("key") || authType == QStringLiteral("publickey")) {
        const QByteArray keyPath = QFile::encodeName(profile.privateKeyPath);
        const QByteArray passphrase = profile.keyPassphrase.toUtf8();
        const int rc = libssh2_userauth_publickey_fromfile(session,
                                                           user.constData(),
                                                           nullptr,
                                                           keyPath.constData(),
                                                           passphrase.isEmpty()
                                                               ? nullptr
                                                               : passphrase.constData());
        if (rc == 0) {
            return true;
        }
        if (errorOut) {
            *errorOut = QObject::tr("SFTP key auth failed: %1").arg(lastError(session));
        }
        return false;
    }

    if (libssh2_userauth_password(session, user.constData(), password.constData()) == 0) {
        return true;
    }

    void **abstract = libssh2_session_abstract(session);
    *abstract = const_cast<QByteArray *>(&password);
    const int rc = libssh2_userauth_keyboard_interactive(session,
                                                         user.constData(),
                                                         kbdInteractiveCallback);
    *abstract = nullptr;
    if (rc == 0) {
        return true;
    }

    if (errorOut) {
        *errorOut = QObject::tr("SFTP password auth failed: %1").arg(lastError(session));
    }
    return false;
}

} // namespace

Lease::Lease(CachedConnection *connection, bool alreadyLocked)
    : m_connection(connection)
{
    if (m_connection && !alreadyLocked) {
        m_connection->mutex.lock();
    }
}

Lease::~Lease()
{
    if (m_connection) {
        m_connection->mutex.unlock();
    }
}

Lease::Lease(Lease &&other) noexcept
    : m_connection(other.m_connection)
{
    other.m_connection = nullptr;
}

Lease &Lease::operator=(Lease &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (m_connection) {
        m_connection->mutex.unlock();
    }
    m_connection = other.m_connection;
    other.m_connection = nullptr;
    return *this;
}

bool ensureLibssh2(QString *errorOut)
{
    static bool initialized = false;
    static QMutex initMutex;
    QMutexLocker lock(&initMutex);
    if (initialized) {
        return true;
    }
    if (libssh2_init(0) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("libssh2_init failed");
        }
        return false;
    }
    initialized = true;
    return true;
}

Lease acquire(const ConnectionProfile &profile, Lane lane)
{
    int poolSize = lanePoolSize(lane);
    if (poolSize < 1) {
        poolSize = 1;
    }
    const QString key = cacheKey(profile, lane);

    CachedConnection *fallback = nullptr;
    {
        QMutexLocker lock(&cacheMapMutex());
        auto &pool = connectionCache()[key];
        if (pool.isEmpty()) {
            pool.append(new CachedConnection);
        }

        for (CachedConnection *connection : pool) {
            if (connection->mutex.tryLock()) {
                return Lease(connection, true);
            }
        }

        if (pool.size() < poolSize) {
            auto *connection = new CachedConnection;
            connection->mutex.lock();
            pool.append(connection);
            return Lease(connection, true);
        }

        auto &cursor = connectionWaitCursor()[key];
        fallback = pool.at(cursor % pool.size());
        cursor = (cursor + 1) % pool.size();
    }

    return Lease(fallback, false);
}

void reset(CachedConnection *connection)
{
    if (!connection) {
        return;
    }
    if (connection->sftp) {
        libssh2_sftp_shutdown(connection->sftp);
        connection->sftp = nullptr;
    }
    if (connection->session) {
        libssh2_session_disconnect(connection->session, "OpenShell SFTP closing");
        libssh2_session_free(connection->session);
        connection->session = nullptr;
    }
    if (!is_invalid_sftp_socket(connection->sock)) {
        close_sftp_socket(connection->sock);
        connection->sock = invalid_sftp_socket();
    }
}

bool ensureConnected(CachedConnection *connection,
                     const ConnectionProfile &profile,
                     QString *errorOut)
{
    if (connection->session && connection->sftp && !is_invalid_sftp_socket(connection->sock)) {
        return true;
    }

    reset(connection);

    if (!openSocket(profile, &connection->sock, errorOut)) {
        reset(connection);
        return false;
    }

    connection->session = libssh2_session_init();
    if (!connection->session) {
        if (errorOut) {
            *errorOut = QObject::tr("libssh2_session_init failed");
        }
        reset(connection);
        return false;
    }

    libssh2_session_set_blocking(connection->session, 1);
    libssh2_session_set_timeout(connection->session, connectTimeoutMs(profile));

    if (libssh2_session_handshake(connection->session, connection->sock) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("SFTP handshake failed: %1")
                            .arg(lastError(connection->session));
        }
        reset(connection);
        return false;
    }

    if (!authenticate(connection->session, profile, errorOut)) {
        reset(connection);
        return false;
    }

    connection->sftp = libssh2_sftp_init(connection->session);
    if (!connection->sftp) {
        if (errorOut) {
            *errorOut = QObject::tr("SFTP init failed: %1")
                            .arg(lastError(connection->session));
        }
        reset(connection);
        return false;
    }

    return true;
}

LIBSSH2_SESSION *session(CachedConnection *connection)
{
    return connection ? connection->session : nullptr;
}

LIBSSH2_SFTP *sftp(CachedConnection *connection)
{
    return connection ? connection->sftp : nullptr;
}

QString lastError(CachedConnection *connection)
{
    return lastError(connection ? connection->session : nullptr);
}

} // namespace SftpConnectionPool
