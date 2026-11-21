#pragma once

#include <QtTypes>

class QString;
struct ConnectionProfile;

namespace TcpConnection {

#ifdef _WIN32
using Socket = quintptr;
#else
using Socket = int;
#endif

inline constexpr Socket invalidSocket = static_cast<Socket>(-1);

int timeoutMs(const ConnectionProfile &profile);
Socket open(const ConnectionProfile &profile, const QString &host, int port, QString *error);
bool setBlocking(Socket socket, bool blocking);
void close(Socket socket);

} // namespace TcpConnection
