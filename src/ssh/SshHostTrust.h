#pragma once

#include <QString>
#include <functional>

class QObject;
struct _LIBSSH2_SESSION;

namespace SshHostTrust {

using Prompt = std::function<void(const QString &requestId, const QString &endpoint,
                                  const QString &fingerprint, const QString &previousFingerprint)>;

void installPrompt(QObject *receiver, Prompt prompt);
void stop();
void resolve(const QString &requestId, bool trusted);

bool verify(_LIBSSH2_SESSION *session, const QString &host, int port, QString *error);

} // namespace SshHostTrust
