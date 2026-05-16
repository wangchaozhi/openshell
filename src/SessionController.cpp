#include "SessionController.h"

#include "ssh/Libssh2ChannelWorker.h"
#include "ssh/SshSession.h"

#include <QUuid>
#include <QVariantMap>

SessionController::SessionController(QObject *parent)
    : QObject(parent)
    , m_workerFactory([](const ConnectionProfile &profile) -> SshChannelWorker * {
          return new Libssh2ChannelWorker(profile);
      })
{
}

void SessionController::setWorkerFactory(SshWorkerFactory factory)
{
    if (factory) {
        m_workerFactory = std::move(factory);
    }
}

SessionController::~SessionController()
{
    qDeleteAll(m_sessions);
    m_sessions.clear();
}

QString SessionController::open(const ConnectionProfile &profile, QString *error)
{
    if (profile.id.isEmpty()) {
        if (error) {
            *error = tr("Cannot open session without a connection");
        }
        return QString();
    }

    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    SshChannelWorker *worker = m_workerFactory(profile);
    if (!worker) {
        if (error) {
            *error = tr("Worker factory returned null");
        }
        return QString();
    }
    auto *session = new SshSession(sessionId, profile, worker, this);

    connect(session, &SshSession::outputAppended, this,
            [this, sessionId](const QByteArray &chunk) {
                emit sessionOutput(sessionId, chunk);
            });
    connect(session, &SshSession::statusChanged, this,
            [this, session]() {
                emit sessionStatusChanged(session->id(), session->status(), session->lastMessage());
                emit sessionsChanged();
            });

    m_sessions.append(session);
    session->start();

    emit sessionsChanged();
    return sessionId;
}

void SessionController::close(const QString &sessionId)
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i)->id() == sessionId) {
            SshSession *session = m_sessions.takeAt(i);
            session->requestStop();
            session->deleteLater();
            emit sessionsChanged();
            return;
        }
    }
}

void SessionController::sendInput(const QString &sessionId, const QByteArray &data)
{
    if (auto *session = findSession(sessionId)) {
        session->sendInput(data);
    }
}

void SessionController::requestResize(const QString &sessionId, int cols, int rows)
{
    if (auto *session = findSession(sessionId)) {
        session->requestResize(cols, rows);
    }
}

QVariantList SessionController::sessionsAsVariantList() const
{
    QVariantList list;
    list.reserve(m_sessions.size());
    for (const SshSession *session : m_sessions) {
        list.append(toVariantMap(session));
    }
    return list;
}

QString SessionController::sessionBuffer(const QString &sessionId) const
{
    if (auto *session = findSession(sessionId)) {
        return session->buffer();
    }
    return QString();
}

bool SessionController::contains(const QString &sessionId) const
{
    return findSession(sessionId) != nullptr;
}

QVariantMap SessionController::toVariantMap(const SshSession *session) const
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), session->id());
    map.insert(QStringLiteral("connectionId"), session->connectionId());
    map.insert(QStringLiteral("title"), session->title());
    map.insert(QStringLiteral("status"), session->status());
    map.insert(QStringLiteral("lastMessage"), session->lastMessage());
    return map;
}

SshSession *SessionController::findSession(const QString &sessionId) const
{
    for (SshSession *session : m_sessions) {
        if (session->id() == sessionId) {
            return session;
        }
    }
    return nullptr;
}
