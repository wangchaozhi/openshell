#include "TcpConnection.h"

#include "ConnectionCatalog.h"

#include <QDeadlineTimer>
#include <QHostAddress>
#include <QHostInfo>
#include <QUrl>

#include <climits>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using SocketLength = int;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketLength = socklen_t;
#endif

namespace TcpConnection {
namespace {

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool interrupted()
{
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

bool connectPending()
{
#ifdef _WIN32
    const int code = WSAGetLastError();
    return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS;
#else
    return errno == EINPROGRESS || errno == EINTR;
#endif
}

bool waitForSocket(Socket socket, bool writing, const QDeadlineTimer &deadline)
{
    while (!deadline.hasExpired()) {
        const int remaining = static_cast<int>(deadline.remainingTime());
#ifdef _WIN32
        fd_set events;
        fd_set exceptions;
        FD_ZERO(&events);
        FD_ZERO(&exceptions);
        FD_SET(socket, &events);
        FD_SET(socket, &exceptions);
        timeval timeout{remaining / 1000, (remaining % 1000) * 1000};
        const int result = ::select(0, writing ? nullptr : &events,
                                    writing ? &events : nullptr, &exceptions, &timeout);
#else
        pollfd event{socket, static_cast<short>(writing ? POLLOUT : POLLIN), 0};
        const int result = ::poll(&event, 1, remaining);
#endif
        if (result >= 0) {
            return result > 0;
        }
        if (!interrupted()) {
            return false;
        }
    }
    return false;
}

QHostAddress resolve(const QString &host, QString *error)
{
    QHostAddress address;
    if (address.setAddress(host)) {
        return address;
    }
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        fail(error, QObject::tr("Cannot resolve host %1: %2").arg(host, info.errorString()));
        return {};
    }
    const auto addresses = info.addresses();
    for (const QHostAddress &candidate : addresses) {
        if (candidate.protocol() == QAbstractSocket::IPv4Protocol) {
            return candidate;
        }
    }
    return addresses.first();
}

bool connectSocket(Socket socket, const QHostAddress &address, int port,
                   const QDeadlineTimer &deadline)
{
    int result;
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        sockaddr_in6 endpoint{};
        endpoint.sin6_family = AF_INET6;
        endpoint.sin6_port = htons(static_cast<quint16>(port));
        const Q_IPV6ADDR raw = address.toIPv6Address();
        std::memcpy(&endpoint.sin6_addr, &raw, sizeof(raw));
        result = ::connect(socket, reinterpret_cast<const sockaddr *>(&endpoint), sizeof(endpoint));
    } else {
        sockaddr_in endpoint{};
        endpoint.sin_family = AF_INET;
        endpoint.sin_port = htons(static_cast<quint16>(port));
        endpoint.sin_addr.s_addr = htonl(address.toIPv4Address());
        result = ::connect(socket, reinterpret_cast<const sockaddr *>(&endpoint), sizeof(endpoint));
    }
    if (result == 0) {
        return true;
    }
    if (!connectPending() || !waitForSocket(socket, true, deadline)) {
        return false;
    }
    int socketError = 0;
    SocketLength length = sizeof(socketError);
    return ::getsockopt(socket, SOL_SOCKET, SO_ERROR,
                        reinterpret_cast<char *>(&socketError), &length) == 0 && socketError == 0;
}

bool transfer(Socket socket, char *data, int length, bool writing, const QDeadlineTimer &deadline)
{
#ifdef MSG_NOSIGNAL
    constexpr int sendFlags = MSG_NOSIGNAL;
#else
    constexpr int sendFlags = 0;
#endif
    // Read exactly the SOCKS frame. The server may send its SSH or Telnet banner
    // in the same packet as the proxy's final reply.
    while (length > 0 && waitForSocket(socket, writing, deadline)) {
        const auto count = writing ? ::send(socket, data, length, sendFlags)
                                   : ::recv(socket, data, length, 0);
        if (count < 0 && interrupted()) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        data += count;
        length -= static_cast<int>(count);
    }
    return length == 0;
}

bool openSocksTunnel(Socket socket, const ConnectionProfile &profile, const QString &host,
                     int port, const QDeadlineTimer &deadline, QString *error)
{
    QByteArray request("\x05\x01\x00", 3);
    const char method = profile.socks5.username.isEmpty() ? 0x00 : 0x02;
    request[2] = method;
    char response[4] = {};
    const auto write = [&](QByteArray &bytes) {
        return transfer(socket, bytes.data(), static_cast<int>(bytes.size()), true, deadline);
    };
    const auto read = [&](int count) {
        return transfer(socket, response, count, false, deadline);
    };
    const auto ioError = [&] {
        return fail(error, deadline.hasExpired() ? QObject::tr("SOCKS5 handshake timed out")
                                               : QObject::tr("SOCKS5 handshake failed"));
    };

    if (!write(request) || !read(2)) {
        return ioError();
    }
    if (response[0] != 0x05 || response[1] != method) {
        return fail(error, QObject::tr("SOCKS5 proxy rejected the authentication method"));
    }

    if (method == 0x02) {
        const QByteArray user = profile.socks5.username.toUtf8();
        const QByteArray password = profile.socks5.password.toUtf8();
        request = QByteArray(1, '\x01');
        request.append(static_cast<char>(user.size())).append(user);
        request.append(static_cast<char>(password.size())).append(password);
        if (!write(request) || !read(2)) {
            return ioError();
        }
        if (response[0] != 0x01 || response[1] != 0x00) {
            return fail(error, QObject::tr("SOCKS5 username or password rejected"));
        }
    }

    request = QByteArray("\x05\x01\x00", 3);
    const QHostAddress address(host);
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = htonl(address.toIPv4Address());
        request.append('\x01').append(reinterpret_cast<const char *>(&ip), sizeof(ip));
    } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR ip = address.toIPv6Address();
        request.append('\x04').append(reinterpret_cast<const char *>(&ip), sizeof(ip));
    } else {
        const QByteArray name = QUrl::toAce(host);
        if (name.isEmpty() || name.size() > 255) {
            return fail(error, QObject::tr("Invalid SOCKS5 destination hostname"));
        }
        // The SOCKS server resolves the destination hostname.
        request.append('\x03').append(static_cast<char>(name.size())).append(name);
    }
    request.append(static_cast<char>(port >> 8)).append(static_cast<char>(port & 0xff));
    if (!write(request) || !read(4)) {
        return ioError();
    }
    if (response[0] != 0x05 || response[2] != 0x00) {
        return fail(error, QObject::tr("Invalid SOCKS5 proxy reply"));
    }
    const auto status = static_cast<unsigned char>(response[1]);
    if (status != 0) {
        return fail(error, QObject::tr("SOCKS5 proxy refused the connection (code %1)").arg(status));
    }

    int addressLength;
    switch (response[3]) {
    case 0x01:
        addressLength = 4;
        break;
    case 0x03:
        if (!read(1)) {
            return ioError();
        }
        addressLength = static_cast<unsigned char>(response[0]);
        if (addressLength == 0) {
            return fail(error, QObject::tr("Invalid SOCKS5 proxy reply"));
        }
        break;
    case 0x04:
        addressLength = 16;
        break;
    default:
        return fail(error, QObject::tr("Invalid SOCKS5 proxy reply"));
    }
    char boundAddressAndPort[255 + 2];
    if (!transfer(socket, boundAddressAndPort, addressLength + 2, false, deadline)) {
        return ioError();
    }
    return true;
}

} // namespace

int timeoutMs(const ConnectionProfile &profile)
{
    const int seconds = profile.connectTimeoutSec > 0 ? profile.connectTimeoutSec : 10;
    return qMin(seconds, INT_MAX / 1000) * 1000;
}

Socket open(const ConnectionProfile &profile, const QString &host, int port, QString *error)
{
    const QString proxyHost = profile.socks5.host.trimmed();
    if (!proxyHost.isEmpty()) {
        const QString proxyError = profile.socks5.validationError();
        if (!proxyError.isEmpty()) {
            fail(error, proxyError);
            return invalidSocket;
        }
    }

    const QString endpoint = proxyHost.isEmpty() ? host : proxyHost;
    const int endpointPort = proxyHost.isEmpty() ? port : profile.socks5.port;
    const QHostAddress address = resolve(endpoint, error);
    if (address.isNull()) {
        return invalidSocket;
    }

#ifdef _WIN32
    static const int startupResult = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (startupResult != 0) {
        fail(error, QObject::tr("WSAStartup failed"));
        return invalidSocket;
    }
#endif

    const int family = address.protocol() == QAbstractSocket::IPv6Protocol ? AF_INET6 : AF_INET;
    const Socket socket = ::socket(family, SOCK_STREAM, 0);
    if (socket == invalidSocket) {
        fail(error, QObject::tr("socket() failed"));
        return invalidSocket;
    }
#ifdef SO_NOSIGPIPE
    const int noSigpipe = 1;
    ::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(noSigpipe));
#endif

    const QDeadlineTimer deadline(timeoutMs(profile), Qt::PreciseTimer);
    if (!setBlocking(socket, false) || !connectSocket(socket, address, endpointPort, deadline)) {
        fail(error, deadline.hasExpired()
                        ? QObject::tr("connect() to %1:%2 timed out after %3 seconds")
                              .arg(endpoint).arg(endpointPort).arg(timeoutMs(profile) / 1000)
                        : QObject::tr("Could not connect to %1:%2").arg(endpoint).arg(endpointPort));
        close(socket);
        return invalidSocket;
    }
    if (!proxyHost.isEmpty() && !openSocksTunnel(socket, profile, host, port, deadline, error)) {
        close(socket);
        return invalidSocket;
    }
    if (!setBlocking(socket, true)) {
        fail(error, QObject::tr("Failed to switch socket to blocking mode"));
        close(socket);
        return invalidSocket;
    }

    const int one = 1;
    ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof(one));
    return socket;
}

bool setBlocking(Socket socket, bool blocking)
{
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    return ::ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    const int flags = ::fcntl(socket, F_GETFL, 0);
    return flags >= 0
           && ::fcntl(socket, F_SETFL, blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK) == 0;
#endif
}

void shutdown(Socket socket)
{
#ifdef _WIN32
    ::shutdown(socket, SD_BOTH);
#else
    ::shutdown(socket, SHUT_RDWR);
#endif
}

void close(Socket socket)
{
#ifdef _WIN32
    ::closesocket(socket);
#else
    ::close(socket);
#endif
}

} // namespace TcpConnection
