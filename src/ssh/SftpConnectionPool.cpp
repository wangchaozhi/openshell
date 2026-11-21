#include "SftpConnectionPool.h"

#include "TcpConnection.h"

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QVector>

#include <cstdlib>
#include <cstring>

namespace SftpConnectionPool {

struct CachedConnection
{
    explicit CachedConnection(Lane connectionLane) : lane(connectionLane) {}

    const Lane lane;
    TcpConnection::Socket sock = TcpConnection::invalidSocket;
    Socks5Proxy socks5;
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
            pool.append(new CachedConnection(lane));
        }

        for (CachedConnection *connection : pool) {
            if (connection->mutex.tryLock()) {
                return Lease(connection, true);
            }
        }

        if (pool.size() < poolSize) {
            auto *connection = new CachedConnection(lane);
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
    if (connection->sock != TcpConnection::invalidSocket) {
        TcpConnection::close(connection->sock);
        connection->sock = TcpConnection::invalidSocket;
    }
    connection->socks5 = {};
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
    if (connection->session && connection->sftp && connection->socks5 == profile.socks5) {
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
    connection->sock = TcpConnection::open(profile, profile.host, port, errorOut);
    if (connection->sock == TcpConnection::invalidSocket) {
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
    libssh2_session_set_timeout(connection->session, TcpConnection::timeoutMs(profile));

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

    connection->socks5 = profile.socks5;
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
