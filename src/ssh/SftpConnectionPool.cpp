#include "SftpConnectionPool.h"

#include "TcpConnection.h"
#include "SshHostTrust.h"
#include "SshSupport.h"

#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QVector>


namespace SftpConnectionPool {

struct CachedConnection
{
    explicit CachedConnection(Lane connectionLane) : lane(connectionLane) {}
    ~CachedConnection() { reset(this); }

    const Lane lane;
    TcpConnection::Socket sock = TcpConnection::invalidSocket;
    ConnectionProfile connectedProfile;
    LIBSSH2_SESSION *session = nullptr;
    LIBSSH2_SESSION *jumpSession = nullptr;
    LIBSSH2_CHANNEL *jumpTunnel = nullptr;
    SessionAbstract sessionContext;
    SessionAbstract jumpContext;
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

QHash<QString, QVector<QSharedPointer<CachedConnection>>> &connectionCache()
{
    static QHash<QString, QVector<QSharedPointer<CachedConnection>>> cache;
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

bool sameCredentialsAndRoute(const ConnectionProfile &first, const ConnectionProfile &second)
{
    return first.password == second.password
           && first.keyPassphrase == second.keyPassphrase
           && first.socks5 == second.socks5
           && first.jumpHost == second.jumpHost
           && first.jumpPort == second.jumpPort
           && first.jumpUsername == second.jumpUsername
           && first.jumpAuthType == second.jumpAuthType
           && first.jumpPassword == second.jumpPassword
           && first.jumpPrivateKeyPath == second.jumpPrivateKeyPath
           && first.jumpKeyPassphrase == second.jumpKeyPassphrase;
}

bool establishSession(LIBSSH2_SESSION **session, SessionAbstract &context,
                      TcpConnection::Socket socket, const QString &host, int port,
                      const SshSupport::AuthSpec &auth, const ConnectionProfile &profile,
                      LIBSSH2_SESSION *jumpSession, LIBSSH2_CHANNEL **tunnel, QString *error)
{
    *session = libssh2_session_init();
    if (!*session) {
        if (error) {
            *error = QObject::tr("libssh2_session_init failed");
        }
        return false;
    }
    *libssh2_session_abstract(*session) = &context;
    if (tunnel) {
        SshSupport::setJumpTunnel(*session, context, jumpSession, tunnel);
    }
    libssh2_session_set_blocking(*session, 1);
    libssh2_session_set_timeout(*session, TcpConnection::timeoutMs(profile));
    if (libssh2_session_handshake(*session, socket) != 0) {
        if (error) {
            *error = QObject::tr("SFTP handshake failed: %1").arg(SshSupport::lastError(*session));
        }
        return false;
    }
    return SshHostTrust::verify(*session, host, port, error)
           && SshSupport::authenticate(*session, auth, error);
}

} // namespace

Lease::Lease(QSharedPointer<CachedConnection> connection, bool alreadyLocked)
    : m_connection(std::move(connection))
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
    : m_connection(std::move(other.m_connection))
{
}

Lease &Lease::operator=(Lease &&other) noexcept
{
    if (this == &other) {
        return *this;
    }
    if (m_connection) {
        m_connection->mutex.unlock();
    }
    m_connection = std::move(other.m_connection);
    return *this;
}

bool ensureLibssh2(QString *errorOut)
{
    return SshSupport::initialize(errorOut);
}

Lease acquire(const ConnectionProfile &profile, Lane lane)
{
    int poolSize = lanePoolSize(lane);
    if (poolSize < 1) {
        poolSize = 1;
    }
    const QString key = cacheKey(profile, lane);

    QSharedPointer<CachedConnection> fallback;
    {
        QMutexLocker lock(&cacheMapMutex());
        auto &pool = connectionCache()[key];
        if (pool.isEmpty()) {
            pool.append(QSharedPointer<CachedConnection>::create(lane));
        }

        for (const auto &connection : std::as_const(pool)) {
            if (connection->mutex.tryLock()) {
                return Lease(connection, true);
            }
        }

        if (pool.size() < poolSize) {
            auto connection = QSharedPointer<CachedConnection>::create(lane);
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

void releaseProfile(const QString &connectionId)
{
    const QString prefix = connectionId + QLatin1Char('|');
    QVector<QSharedPointer<CachedConnection>> retired;
    {
        const QMutexLocker guard(&cacheMapMutex());
        auto &cache = connectionCache();
        for (auto it = cache.begin(); it != cache.end();) {
            if (it.key().startsWith(prefix)) {
                retired.append(it.value());
                connectionWaitCursor().remove(it.key());
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void reset(CachedConnection *connection, LIBSSH2_SFTP_HANDLE *openHandle)
{
    if (!connection) {
        return;
    }
    if (connection->sock != TcpConnection::invalidSocket) {
        // Stop network I/O before freeing libssh2 state, keeping the descriptor
        // reserved until libssh2 has finished with it.
        TcpConnection::shutdown(connection->sock);
    }
    if (openHandle) {
        libssh2_sftp_close_handle(openHandle);
    }
    if (connection->sftp) {
        libssh2_sftp_shutdown(connection->sftp);
        connection->sftp = nullptr;
    }
    if (connection->session) {
        libssh2_session_free(connection->session);
        connection->session = nullptr;
    }
    if (connection->jumpTunnel) {
        libssh2_channel_free(connection->jumpTunnel);
        connection->jumpTunnel = nullptr;
    }
    if (connection->jumpSession) {
        libssh2_session_free(connection->jumpSession);
        connection->jumpSession = nullptr;
    }
    connection->sessionContext = {};
    connection->jumpContext = {};
    if (connection->sock != TcpConnection::invalidSocket) {
        TcpConnection::close(connection->sock);
        connection->sock = TcpConnection::invalidSocket;
    }
    connection->connectedProfile = {};
}

bool connectionLost(CachedConnection *connection, int errorCode)
{
    if (libssh2_channel_eof(libssh2_sftp_get_channel(connection->sftp))) {
        return true;
    }
    switch (errorCode) {
    case LIBSSH2_ERROR_SOCKET_SEND:
    case LIBSSH2_ERROR_SOCKET_RECV:
    case LIBSSH2_ERROR_SOCKET_DISCONNECT:
    case LIBSSH2_ERROR_SOCKET_TIMEOUT:
    case LIBSSH2_ERROR_BAD_SOCKET:
    case LIBSSH2_ERROR_CHANNEL_CLOSED:
    case LIBSSH2_ERROR_CHANNEL_EOF_SENT:
    case LIBSSH2_ERROR_TIMEOUT:
        return true;
    case LIBSSH2_ERROR_SFTP_PROTOCOL: {
        const auto status = libssh2_sftp_last_error(connection->sftp);
        return status == LIBSSH2_FX_NO_CONNECTION || status == LIBSSH2_FX_CONNECTION_LOST;
    }
    default:
        return false;
    }
}

bool ensureConnected(CachedConnection *connection,
                     const ConnectionProfile &profile,
                     QString *errorOut)
{
    if (connection->session && (connection->lane == Lane::Exec || connection->sftp)
        && sameCredentialsAndRoute(connection->connectedProfile, profile)) {
        if (connection->lane != Lane::Transfer) {
            return true;
        }
        // Verify the reused connection before a transfer can create or
        // truncate a file. A server-side permission error still proves liveness.
        LIBSSH2_SFTP_ATTRIBUTES attributes{};
        const int rc = libssh2_sftp_stat(connection->sftp, ".", &attributes);
        if (rc == 0 || !connectionLost(connection, rc)) {
            return true;
        }
    }

    reset(connection);

    const int port = profile.port > 0 ? profile.port : 22;
    const QString jumpHost = profile.jumpHost.trimmed();
    const bool useJump = !jumpHost.isEmpty();
    const int jumpPort = profile.jumpPort > 0 ? profile.jumpPort : 22;
    connection->sock = TcpConnection::open(profile, useJump ? jumpHost : profile.host,
                                           useJump ? jumpPort : port, errorOut);
    if (connection->sock == TcpConnection::invalidSocket) {
        return false;
    }

    if (useJump) {
        const SshSupport::AuthSpec jumpAuth{
            profile.jumpUsername.isEmpty() ? profile.username : profile.jumpUsername,
            profile.jumpAuthType, profile.jumpPassword, profile.jumpPrivateKeyPath,
            profile.jumpKeyPassphrase
        };
        if (!establishSession(&connection->jumpSession, connection->jumpContext, connection->sock,
                              jumpHost, jumpPort, jumpAuth, profile, nullptr, nullptr, errorOut)) {
            reset(connection);
            return false;
        }
        const QByteArray targetHost = profile.host.toUtf8();
        connection->jumpTunnel = libssh2_channel_direct_tcpip_ex(connection->jumpSession,
                                                                 targetHost.constData(), port,
                                                                 "127.0.0.1", 22);
        if (!connection->jumpTunnel) {
            if (errorOut) {
                *errorOut = QCoreApplication::translate("Libssh2ChannelWorker",
                                                       "direct-tcpip via jump host failed: %1")
                                .arg(SshSupport::lastError(connection->jumpSession));
            }
            reset(connection);
            return false;
        }
    }

    const SshSupport::AuthSpec auth{profile.username, profile.authType, profile.password,
                                    profile.privateKeyPath, profile.keyPassphrase};
    if (!establishSession(&connection->session, connection->sessionContext, connection->sock,
                          profile.host, port, auth, profile, connection->jumpSession,
                          useJump ? &connection->jumpTunnel : nullptr, errorOut)) {
        reset(connection);
        return false;
    }

    if (connection->lane != Lane::Exec) {
        connection->sftp = libssh2_sftp_init(connection->session);
        if (!connection->sftp) {
            if (errorOut) {
                *errorOut = QObject::tr("SFTP init failed: %1")
                                .arg(SshSupport::lastError(connection->session));
            }
            reset(connection);
            return false;
        }
    }
    connection->connectedProfile = profile;
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
    return SshSupport::lastError(connection ? connection->session : nullptr);
}

} // namespace SftpConnectionPool
