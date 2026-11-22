#include "SshSupport.h"

#include <QCoreApplication>
#include <QFile>

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace SshSupport {
namespace {

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
    // libssh2 probes for incoming packets before sending. The tunnel read
    // must return immediately when idle, even while the target API is blocking.
    const int wasBlocking = libssh2_session_get_blocking(ctx->jumpSession);
    libssh2_session_set_blocking(ctx->jumpSession, 0);
    const ssize_t rc = libssh2_channel_read(tunnel, static_cast<char *>(buffer), length);
    libssh2_session_set_blocking(ctx->jumpSession, wasBlocking);
    if (rc == LIBSSH2_ERROR_EAGAIN || (rc == 0 && !libssh2_channel_eof(tunnel))) {
        errno = EAGAIN;
        return -EAGAIN;
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

bool authKeyboardInteractive(LIBSSH2_SESSION *session, const AuthSpec &spec, QString *errorOut);

bool authPassword(LIBSSH2_SESSION *session,
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
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "Password auth failed: %1").arg(lastError(session));
        }
        return false;
    }
    return true;
}

bool authKeyboardInteractive(LIBSSH2_SESSION *session,
                                                   const AuthSpec &spec,
                                                   QString *errorOut)
{
    const QByteArray user = spec.username.toUtf8();
    const QByteArray pwd = spec.password.toUtf8();
    void **abstract = libssh2_session_abstract(session);
    auto *ctx = static_cast<SessionAbstract *>(*abstract);
    if (!ctx) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "session abstract not initialized");
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
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "Keyboard-interactive auth failed: %1").arg(lastError(session));
        }
        return false;
    }
    return true;
}

bool authKey(LIBSSH2_SESSION *session,
                                   const AuthSpec &spec,
                                   QString *errorOut)
{
    if (spec.privateKeyPath.isEmpty()) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "Private key path is empty");
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
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "Key auth failed: %1").arg(lastError(session));
        }
        return false;
    }
    return true;
}

bool authAgent(LIBSSH2_SESSION *session,
                                     const AuthSpec &spec,
                                     QString *errorOut)
{
    LIBSSH2_AGENT *agent = libssh2_agent_init(session);
    if (!agent) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "libssh2_agent_init failed");
        }
        return false;
    }

    auto cleanup = [&]() {
        libssh2_agent_disconnect(agent);
        libssh2_agent_free(agent);
    };

    if (libssh2_agent_connect(agent) != 0) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "Cannot connect to ssh-agent");
        }
        cleanup();
        return false;
    }
    if (libssh2_agent_list_identities(agent) != 0) {
        if (errorOut) {
            *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "ssh-agent: list_identities failed");
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
        if (rc == 1) {
            break;
        }
        if (libssh2_agent_userauth(agent, user.constData(), cur) == 0) {
            ok = true;
            break;
        }
        prev = cur;
    }

    if (!ok && errorOut) {
        *errorOut = QCoreApplication::translate("Libssh2ChannelWorker", "ssh-agent: no identity accepted by server");
    }
    cleanup();
    return ok;
}

} // namespace

bool initialize(QString *error)
{
    static const int result = libssh2_init(0);
    if (result != 0 && error) {
        *error = QCoreApplication::translate("Libssh2ChannelWorker", "libssh2_init failed");
    }
    return result == 0;
}

QString lastError(LIBSSH2_SESSION *session)
{
    if (!session) {
        return QCoreApplication::translate("Libssh2ChannelWorker", "(no session)");
    }
    char *message = nullptr;
    int length = 0;
    libssh2_session_last_error(session, &message, &length, 0);
    return message && length > 0 ? QString::fromUtf8(message, length)
           : QCoreApplication::translate("Libssh2ChannelWorker", "(unknown libssh2 error)");
}

void setJumpTunnel(LIBSSH2_SESSION *session, SessionAbstract &context,
                   LIBSSH2_SESSION *jumpSession, LIBSSH2_CHANNEL **tunnel)
{
    context.jumpTunnelSlot = tunnel;
    context.jumpSession = jumpSession;
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_SEND,
                                  reinterpret_cast<libssh2_cb_generic *>(&jumpSendCallback));
    libssh2_session_callback_set2(session, LIBSSH2_CALLBACK_RECV,
                                  reinterpret_cast<libssh2_cb_generic *>(&jumpRecvCallback));
}

bool authenticate(LIBSSH2_SESSION *session,
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
        err = QCoreApplication::translate("Libssh2ChannelWorker", "Unknown auth type '%1'").arg(type);
    }

    if (!ok && errorOut) {
        *errorOut = err.isEmpty() ? QCoreApplication::translate("Libssh2ChannelWorker", "Authentication failed") : err;
    }
    return ok;
}

} // namespace SshSupport
