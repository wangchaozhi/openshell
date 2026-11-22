#include "SshHostTrust.h"

#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QHash>
#include <QHostAddress>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QSettings>
#include <QSharedPointer>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QWaitCondition>

#include <libssh2.h>
#include <utility>

namespace SshHostTrust {
namespace {

struct Request
{
    QString id;
    QByteArray digest;
    bool finished = false;
    bool accepted = false;
};

struct TrustState
{
    QMutex mutex;
    QWaitCondition changed;
    QObject *receiver = nullptr;
    Prompt prompt;
    QHash<QString, QSharedPointer<Request>> pendingByEndpoint;
};

TrustState &state()
{
    static TrustState trust;
    return trust;
}

QString fingerprint(const QByteArray &digest)
{
    return digest.isEmpty() ? QString()
           : QStringLiteral("SHA256:") + QString::fromLatin1(digest.toBase64(QByteArray::OmitTrailingEquals));
}

QString trustKey(const QString &endpoint)
{
    return QStringLiteral("trustedSshHosts/")
           + QString::fromLatin1(endpoint.toUtf8().toBase64(QByteArray::Base64UrlEncoding
                                                           | QByteArray::OmitTrailingEquals));
}

QString normalizedHost(const QString &host)
{
    const QString name = host.trimmed();
    const QHostAddress address(name);
    if (!address.isNull()) {
        return address.toString();
    }
    const QByteArray encoded = QUrl::toAce(name);
    return (encoded.isEmpty() ? name : QString::fromLatin1(encoded)).toLower();
}

} // namespace

void installPrompt(QObject *receiver, Prompt prompt)
{
    auto &trust = state();
    const QMutexLocker lock(&trust.mutex);
    trust.receiver = receiver;
    trust.prompt = std::move(prompt);
}

void stop()
{
    auto &trust = state();
    const QMutexLocker lock(&trust.mutex);
    trust.receiver = nullptr;
    trust.prompt = {};
    for (const auto &request : std::as_const(trust.pendingByEndpoint)) {
        request->finished = true;
    }
    trust.pendingByEndpoint.clear();
    trust.changed.wakeAll();
}

void resolve(const QString &requestId, bool trusted)
{
    auto &trust = state();
    const QMutexLocker lock(&trust.mutex);
    for (auto it = trust.pendingByEndpoint.begin(); it != trust.pendingByEndpoint.end(); ++it) {
        const auto request = it.value();
        if (request->id != requestId || request->finished) {
            continue;
        }
        if (trusted) {
            QSettings settings;
            settings.setValue(trustKey(it.key()), request->digest);
            settings.sync();
            request->accepted = settings.status() == QSettings::NoError;
        }
        request->finished = true;
        trust.pendingByEndpoint.erase(it);
        trust.changed.wakeAll();
        return;
    }
}

bool verify(LIBSSH2_SESSION *session, const QString &host, int port, QString *error)
{
    size_t keyLength = 0;
    const char *key = libssh2_session_hostkey(session, &keyLength, nullptr);
    const QString endpoint = QStringLiteral("[%1]:%2").arg(normalizedHost(host)).arg(port);
    if (!key || keyLength == 0) {
        if (error) {
            *error = QObject::tr("Cannot read the SSH host key for %1").arg(endpoint);
        }
        return false;
    }
    const QByteArray digest = QCryptographicHash::hash(
        QByteArrayView(key, static_cast<qsizetype>(keyLength)), QCryptographicHash::Sha256);
    auto &trust = state();
    const QMutexLocker lock(&trust.mutex);
    const QByteArray known = QSettings().value(trustKey(endpoint)).toByteArray();
    if (known == digest) {
        return true;
    }
    if (error) {
        *error = QObject::tr("SSH host key was not verified for %1 (%2)")
                     .arg(endpoint, fingerprint(digest));
    }
    if (!trust.receiver || !trust.prompt || trust.receiver->thread() == QThread::currentThread()) {
        return false;
    }

    auto request = trust.pendingByEndpoint.value(endpoint);
    if (request && request->digest != digest) {
        return false;
    }
    if (!request) {
        request = QSharedPointer<Request>::create();
        request->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request->digest = digest;
        trust.pendingByEndpoint.insert(endpoint, request);
        QMetaObject::invokeMethod(trust.receiver,
            [prompt = trust.prompt, id = request->id, endpoint,
             current = fingerprint(digest), previous = fingerprint(known)] {
                prompt(id, endpoint, current, previous);
            }, Qt::QueuedConnection);
    }

    const QDeadlineTimer deadline(90000);
    while (!request->finished && trust.changed.wait(&trust.mutex, deadline)) {
    }
    if (!request->finished) {
        request->finished = true;
        trust.pendingByEndpoint.remove(endpoint);
        trust.changed.wakeAll();
    }
    if (request->accepted && error) {
        error->clear();
    }
    return request->accepted;
}

} // namespace SshHostTrust
