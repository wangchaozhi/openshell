#pragma once

#include <QString>
#include <libssh2.h>

struct SessionAbstract
{
    LIBSSH2_CHANNEL **jumpTunnelSlot = nullptr;
    LIBSSH2_SESSION *jumpSession = nullptr;
    const QByteArray *kbdintPassword = nullptr;
};

namespace SshSupport {

struct AuthSpec
{
    QString username;
    QString authType;
    QString password;
    QString privateKeyPath;
    QString keyPassphrase;
};

bool initialize(QString *error);
bool authenticate(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *error);
QString lastError(LIBSSH2_SESSION *session);
void setJumpTunnel(LIBSSH2_SESSION *session, SessionAbstract &context,
                   LIBSSH2_SESSION *jumpSession, LIBSSH2_CHANNEL **tunnel);

} // namespace SshSupport
