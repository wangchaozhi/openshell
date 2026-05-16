#include "SftpDirectoryLister.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QHostInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>

#include <cstdlib>
#include <cstring>

#include <libssh2.h>
#include <libssh2_sftp.h>

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

namespace {

struct CachedSftpConnection
{
    SftpSocket sock = invalid_sftp_socket();
    LIBSSH2_SESSION *session = nullptr;
    LIBSSH2_SFTP *sftp = nullptr;
    QMutex mutex; // serializes work on this single libssh2 session (not session-thread-safe)
};

// Tiny mutex protecting only the cache map lookup/insert — long-running SFTP work
// runs under the per-connection mutex instead, so different connections don't block each other.
QMutex &cacheMapMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, CachedSftpConnection *> &connectionCache()
{
    static QHash<QString, CachedSftpConnection *> cache;
    return cache;
}

CachedSftpConnection *acquireConnection(const QString &key)
{
    QMutexLocker lock(&cacheMapMutex());
    auto &cache = connectionCache();
    auto it = cache.find(key);
    if (it == cache.end()) {
        it = cache.insert(key, new CachedSftpConnection);
    }
    return it.value();
}

bool ensureLibssh2(QString *errorOut)
{
    static bool initialized = false;
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

QString cacheKey(const ConnectionProfile &profile)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(profile.id,
             profile.host,
             QString::number(profile.port),
             profile.username,
             profile.authType,
             profile.privateKeyPath);
}

void resetConnection(CachedSftpConnection *connection)
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

bool ensureConnected(CachedSftpConnection *connection,
                     const ConnectionProfile &profile,
                     QString *errorOut)
{
    if (connection->session && connection->sftp && !is_invalid_sftp_socket(connection->sock)) {
        return true;
    }

    resetConnection(connection);

    if (!openSocket(profile, &connection->sock, errorOut)) {
        resetConnection(connection);
        return false;
    }

    connection->session = libssh2_session_init();
    if (!connection->session) {
        if (errorOut) {
            *errorOut = QObject::tr("libssh2_session_init failed");
        }
        resetConnection(connection);
        return false;
    }

    libssh2_session_set_blocking(connection->session, 1);
    libssh2_session_set_timeout(connection->session, connectTimeoutMs(profile));

    if (libssh2_session_handshake(connection->session, connection->sock) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("SFTP handshake failed: %1")
                            .arg(lastError(connection->session));
        }
        resetConnection(connection);
        return false;
    }

    if (!authenticate(connection->session, profile, errorOut)) {
        resetConnection(connection);
        return false;
    }

    connection->sftp = libssh2_sftp_init(connection->session);
    if (!connection->sftp) {
        if (errorOut) {
            *errorOut = QObject::tr("SFTP init failed: %1")
                            .arg(lastError(connection->session));
        }
        resetConnection(connection);
        return false;
    }

    return true;
}

QString joinRemotePath(const QString &base, const QString &name)
{
    if (base == QStringLiteral("/")) {
        return QStringLiteral("/") + name;
    }
    return base + QStringLiteral("/") + name;
}

QString permissionsToOctal(const LIBSSH2_SFTP_ATTRIBUTES &attrs)
{
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
        return QString();
    }
    return QString::number(static_cast<int>(attrs.permissions & 07777), 8).rightJustified(3, QLatin1Char('0'));
}

qint64 localPathSize(const QFileInfo &info)
{
    if (!info.exists()) {
        return 0;
    }
    if (info.isFile()) {
        return info.size();
    }
    if (!info.isDir()) {
        return 0;
    }
    qint64 total = 0;
    const QDir dir(info.absoluteFilePath());
    const QFileInfoList children = dir.entryInfoList(QDir::AllEntries
                                                         | QDir::NoDotAndDotDot
                                                         | QDir::Readable,
                                                     QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &child : children) {
        total += localPathSize(child);
    }
    return total;
}

bool makeRemoteDirectory(LIBSSH2_SFTP *sftp, const QString &path)
{
    const QByteArray encoded = path.toUtf8();
    if (libssh2_sftp_mkdir(sftp, encoded.constData(), 0755) == 0) {
        return true;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    return libssh2_sftp_stat(sftp, encoded.constData(), &attrs) == 0
           && (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

bool uploadFile(LIBSSH2_SFTP *sftp,
                const QString &localPath,
                const QString &remotePath,
                QString *errorOut,
                qint64 bytesTotal,
                qint64 *bytesDone,
                const SftpDirectoryLister::ProgressCallback &progress)
{
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot read local file %1").arg(localPath);
        }
        return false;
    }

    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(sftp,
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_WRITE
                                                        | LIBSSH2_FXF_CREAT
                                                        | LIBSSH2_FXF_TRUNC,
                                                    0644);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot create remote file %1").arg(remotePath);
        }
        return false;
    }

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(32768);
        qsizetype written = 0;
        while (written < chunk.size()) {
            const ssize_t n = libssh2_sftp_write(handle,
                                                 chunk.constData() + written,
                                                 static_cast<size_t>(chunk.size() - written));
            if (n < 0) {
                libssh2_sftp_close(handle);
                if (errorOut) {
                    *errorOut = QObject::tr("Failed to write remote file %1").arg(remotePath);
                }
                return false;
            }
            written += n;
            if (bytesDone) {
                *bytesDone += n;
                if (progress) {
                    progress(*bytesDone, bytesTotal);
                }
            }
        }
    }

    libssh2_sftp_close(handle);
    return true;
}

bool uploadPathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &localPath,
                         const QString &remotePath,
                         QString *errorOut,
                         qint64 bytesTotal,
                         qint64 *bytesDone,
                         const SftpDirectoryLister::ProgressCallback &progress)
{
    const QFileInfo info(localPath);
    if (info.isDir()) {
        if (!makeRemoteDirectory(sftp, remotePath)) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot create remote directory %1").arg(remotePath);
            }
            return false;
        }

        const QDir dir(localPath);
        const QFileInfoList children = dir.entryInfoList(QDir::AllEntries
                                                             | QDir::NoDotAndDotDot
                                                             | QDir::Readable,
                                                         QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &child : children) {
            if (!uploadPathRecursive(sftp,
                                     child.absoluteFilePath(),
                                     joinRemotePath(remotePath, child.fileName()),
                                     errorOut,
                                     bytesTotal,
                                     bytesDone,
                                     progress)) {
                return false;
            }
        }
        return true;
    }

    return uploadFile(sftp, localPath, remotePath, errorOut, bytesTotal, bytesDone, progress);
}

bool isRemoteDir(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    const QByteArray encoded = remotePath.toUtf8();
    return libssh2_sftp_stat(sftp, encoded.constData(), &attrs) == 0
           && (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

qint64 remoteFileSize(const LIBSSH2_SFTP_ATTRIBUTES &attrs)
{
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_SIZE)) {
        return 0;
    }
    return static_cast<qint64>(attrs.filesize);
}

qint64 remotePathSizeRecursive(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    const QByteArray encoded = remotePath.toUtf8();
    if (libssh2_sftp_stat(sftp, encoded.constData(), &attrs) != 0) {
        return 0;
    }
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
            || !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        return remoteFileSize(attrs);
    }

    qint64 total = 0;
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
    if (!dir) {
        return 0;
    }
    char name[512];
    LIBSSH2_SFTP_ATTRIBUTES childAttrs{};
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &childAttrs);
        if (rc <= 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }
        const QString childPath = joinRemotePath(remotePath, fileName);
        if ((childAttrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                && LIBSSH2_SFTP_S_ISDIR(childAttrs.permissions)) {
            total += remotePathSizeRecursive(sftp, childPath);
        } else {
            total += remoteFileSize(childAttrs);
        }
    }
    libssh2_sftp_closedir(dir);
    return total;
}

bool downloadFile(LIBSSH2_SFTP *sftp,
                  const QString &remotePath,
                  const QString &localPath,
                  QString *errorOut,
                  qint64 bytesTotal,
                  qint64 *bytesDone,
                  const SftpDirectoryLister::ProgressCallback &progress)
{
    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(sftp,
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_READ,
                                                    0);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote file %1").arg(remotePath);
        }
        return false;
    }

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        libssh2_sftp_close(handle);
        if (errorOut) {
            *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
        }
        return false;
    }

    char buffer[32768];
    for (;;) {
        const ssize_t n = libssh2_sftp_read(handle, buffer, sizeof(buffer));
        if (n == 0) {
            break;
        }
        if (n < 0) {
            libssh2_sftp_close(handle);
            if (errorOut) {
                *errorOut = QObject::tr("Failed to read remote file %1").arg(remotePath);
            }
            return false;
        }
        const qint64 written = file.write(buffer, n);
        if (written < 0) {
            libssh2_sftp_close(handle);
            if (errorOut) {
                *errorOut = QObject::tr("Cannot write local file %1").arg(localPath);
            }
            return false;
        }
        if (bytesDone) {
            *bytesDone += written;
            if (progress) {
                progress(*bytesDone, bytesTotal);
            }
        }
    }

    libssh2_sftp_close(handle);
    return true;
}

bool downloadPathRecursive(LIBSSH2_SFTP *sftp,
                           const QString &remotePath,
                           const QString &localPath,
                           QString *errorOut,
                           qint64 bytesTotal,
                           qint64 *bytesDone,
                           const SftpDirectoryLister::ProgressCallback &progress)
{
    if (!isRemoteDir(sftp, remotePath)) {
        return downloadFile(sftp, remotePath, localPath, errorOut, bytesTotal, bytesDone, progress);
    }

    QDir().mkpath(localPath);
    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
    if (!dir) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote directory %1").arg(remotePath);
        }
        return false;
    }

    char name[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &attrs);
        if (rc <= 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }
        if (!downloadPathRecursive(sftp,
                                   joinRemotePath(remotePath, fileName),
                                   QDir(localPath).filePath(fileName),
                                   errorOut,
                                   bytesTotal,
                                   bytesDone,
                                   progress)) {
            libssh2_sftp_closedir(dir);
            return false;
        }
    }

    libssh2_sftp_closedir(dir);
    return true;
}

bool removePathRecursive(LIBSSH2_SFTP *sftp,
                         const QString &remotePath,
                         bool recursive,
                         QString *errorOut)
{
    const QByteArray encoded = remotePath.toUtf8();
    if (!isRemoteDir(sftp, remotePath)) {
        if (libssh2_sftp_unlink(sftp, encoded.constData()) == 0) {
            return true;
        }
        if (errorOut) {
            *errorOut = QObject::tr("Cannot delete remote file %1").arg(remotePath);
        }
        return false;
    }

    if (recursive) {
        LIBSSH2_SFTP_HANDLE *dir = libssh2_sftp_opendir(sftp, encoded.constData());
        if (!dir) {
            if (errorOut) {
                *errorOut = QObject::tr("Cannot open remote directory %1").arg(remotePath);
            }
            return false;
        }
        char name[512];
        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        for (;;) {
            const int rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), nullptr, 0, &attrs);
            if (rc <= 0) {
                break;
            }
            const QString fileName = QString::fromUtf8(name, rc);
            if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
                continue;
            }
            if (!removePathRecursive(sftp, joinRemotePath(remotePath, fileName), true, errorOut)) {
                libssh2_sftp_closedir(dir);
                return false;
            }
        }
        libssh2_sftp_closedir(dir);
    }

    if (libssh2_sftp_rmdir(sftp, encoded.constData()) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot delete remote directory %1").arg(remotePath);
        }
        return false;
    }
    return true;
}

} // namespace

QVariantList SftpDirectoryLister::list(const ConnectionProfile &profile,
                                       const QString &remotePath,
                                       QString *errorOut)
{
    QVariantList rows;
    LIBSSH2_SFTP_HANDLE *dir = nullptr;

    if (!ensureLibssh2(errorOut)) {
        return rows;
    }

    auto closeDir = [&]() {
        if (dir) {
            libssh2_sftp_closedir(dir);
            dir = nullptr;
        }
    };

    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);

    if (!ensureConnected(connection, profile, errorOut)) {
        return rows;
    }

    const QString cleanPath = remotePath.isEmpty() ? QStringLiteral("/") : remotePath;
    const QByteArray path = cleanPath.toUtf8();
    dir = libssh2_sftp_opendir(connection->sftp, path.constData());
    if (!dir) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot open remote directory %1").arg(cleanPath);
        }
        return rows;
    }

    char name[512];
    char longentry[1024];
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    for (;;) {
        const int rc = libssh2_sftp_readdir_ex(dir,
                                               name,
                                               sizeof(name),
                                               longentry,
                                               sizeof(longentry),
                                               &attrs);
        if (rc <= 0) {
            break;
        }
        const QString fileName = QString::fromUtf8(name, rc);
        if (fileName == QStringLiteral(".") || fileName == QStringLiteral("..")) {
            continue;
        }

        const bool isDir = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
                           && LIBSSH2_SFTP_S_ISDIR(attrs.permissions);

        // longentry 格式："-rw-r--r-- 1 root root 1234 Jan 01 12:00 filename"
        // 解析第3、4个空白分隔字段为 user/group
        QString owner;
        const QString le = QString::fromUtf8(longentry);
        const QStringList fields = le.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() >= 4) {
            owner = fields.at(2) + QLatin1Char('/') + fields.at(3);
        } else if (attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) {
            owner = QString::number(attrs.uid) + QLatin1Char('/') + QString::number(attrs.gid);
        }

        QVariantMap row;
        row.insert(QStringLiteral("name"), fileName);
        row.insert(QStringLiteral("path"), joinRemotePath(cleanPath, fileName));
        row.insert(QStringLiteral("isDir"), isDir);
        row.insert(QStringLiteral("size"),
                   isDir ? QStringLiteral("--") : QString::number(attrs.filesize));
        row.insert(QStringLiteral("permissions"), permissionsToOctal(attrs));
        row.insert(QStringLiteral("owner"), owner);
        row.insert(QStringLiteral("modified"),
                   attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME
                       ? QDateTime::fromSecsSinceEpoch(attrs.mtime).toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                       : QString());
        rows.append(row);
    }

    closeDir();
    if (errorOut) {
        errorOut->clear();
    }
    return rows;
}

bool SftpDirectoryLister::upload(const ConnectionProfile &profile,
                                 const QString &localPath,
                                 const QString &remoteDirectory,
                                 QString *errorOut,
                                 ProgressCallback progress)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }

    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    if (!ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    const QFileInfo info(localPath);
    if (!info.exists()) {
        if (errorOut) {
            *errorOut = QObject::tr("Local path does not exist: %1").arg(localPath);
        }
        return false;
    }

    const QString targetDir = remoteDirectory.isEmpty() ? QStringLiteral("/") : remoteDirectory;
    const QString remotePath = joinRemotePath(targetDir, info.fileName());
    const qint64 bytesTotal = localPathSize(info);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    const bool ok = uploadPathRecursive(connection->sftp,
                                        info.absoluteFilePath(),
                                        remotePath,
                                        errorOut,
                                        bytesTotal,
                                        &bytesDone,
                                        progress);
    if (ok && progress) {
        progress(bytesTotal, bytesTotal);
    }
    if (ok && errorOut) {
        errorOut->clear();
    }
    return ok;
}

bool SftpDirectoryLister::chmod(const ConnectionProfile &profile,
                                const QString &remotePath,
                                int permissions,
                                QString *errorOut)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }

    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    if (!ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = static_cast<unsigned long>(permissions);

    const QByteArray encoded = remotePath.toUtf8();
    if (libssh2_sftp_setstat(connection->sftp, encoded.constData(), &attrs) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Failed to change permissions for %1").arg(remotePath);
        }
        return false;
    }

    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

bool SftpDirectoryLister::download(const ConnectionProfile &profile,
                                   const QString &remotePath,
                                   const QString &localDirectory,
                                   QString *downloadedPath,
                                   QString *errorOut,
                                   ProgressCallback progress)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }
    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    if (!ensureConnected(connection, profile, errorOut)) {
        return false;
    }

    const QString name = remotePath.section(QLatin1Char('/'), -1);
    const QString localPath = QDir(localDirectory).filePath(name.isEmpty() ? QStringLiteral("download") : name);
    const qint64 bytesTotal = remotePathSizeRecursive(connection->sftp, remotePath);
    qint64 bytesDone = 0;
    if (progress) {
        progress(bytesDone, bytesTotal);
    }
    const bool ok = downloadPathRecursive(connection->sftp,
                                          remotePath,
                                          localPath,
                                          errorOut,
                                          bytesTotal,
                                          &bytesDone,
                                          progress);
    if (ok) {
        if (progress) {
            progress(bytesTotal, bytesTotal);
        }
        if (downloadedPath) {
            *downloadedPath = localPath;
        }
        if (errorOut) {
            errorOut->clear();
        }
    }
    return ok;
}

bool SftpDirectoryLister::createDirectory(const ConnectionProfile &profile,
                                          const QString &remotePath,
                                          QString *errorOut)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }
    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    return ensureConnected(connection, profile, errorOut)
           && makeRemoteDirectory(connection->sftp, remotePath);
}

bool SftpDirectoryLister::createFile(const ConnectionProfile &profile,
                                     const QString &remotePath,
                                     QString *errorOut)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }
    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    if (!ensureConnected(connection, profile, errorOut)) {
        return false;
    }
    const QByteArray encoded = remotePath.toUtf8();
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(connection->sftp,
                                                    encoded.constData(),
                                                    LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL,
                                                    0644);
    if (!handle) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot create remote file %1").arg(remotePath);
        }
        return false;
    }
    libssh2_sftp_close(handle);
    return true;
}

bool SftpDirectoryLister::renamePath(const ConnectionProfile &profile,
                                     const QString &oldPath,
                                     const QString &newPath,
                                     QString *errorOut)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }
    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    if (!ensureConnected(connection, profile, errorOut)) {
        return false;
    }
    const QByteArray oldEncoded = oldPath.toUtf8();
    const QByteArray newEncoded = newPath.toUtf8();
    if (libssh2_sftp_rename(connection->sftp, oldEncoded.constData(), newEncoded.constData()) != 0) {
        if (errorOut) {
            *errorOut = QObject::tr("Cannot rename %1").arg(oldPath);
        }
        return false;
    }
    return true;
}

bool SftpDirectoryLister::removePath(const ConnectionProfile &profile,
                                     const QString &remotePath,
                                     bool recursive,
                                     QString *errorOut)
{
    if (!ensureLibssh2(errorOut)) {
        return false;
    }
    CachedSftpConnection *connection = acquireConnection(cacheKey(profile));
    QMutexLocker locker(&connection->mutex);
    return ensureConnected(connection, profile, errorOut)
           && removePathRecursive(connection->sftp, remotePath, recursive, errorOut);
}
