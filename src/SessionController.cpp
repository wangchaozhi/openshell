#include "SessionController.h"

#include <QUuid>
#include <QVariantMap>

SessionController::SessionController(QObject *parent)
    : QObject(parent)
{
}

bool SessionController::open(const ConnectionProfile &profile, QString *error)
{
    if (profile.id.isEmpty()) {
        if (error) {
            *error = tr("Cannot open session without a connection");
        }
        return false;
    }

    // Placeholder: a real implementation would spin up a libssh2/QSsh worker on
    // a background thread, emit sessionOutput on data arrival, and update status
    // via a thread-safe channel. For now the skeleton records a stub session.

    ActiveSession session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.connectionId = profile.id;
    session.title = profile.name.isEmpty()
            ? QStringLiteral("%1@%2").arg(profile.username, profile.host)
            : profile.name;
    session.status = QStringLiteral("connecting");
    session.lastMessage = tr("Stub session — wire up an SSH backend to drive output.");
    m_sessions.append(session);

    emit sessionsChanged();
    return true;
}

void SessionController::close(const QString &sessionId)
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).id == sessionId) {
            m_sessions.removeAt(i);
            emit sessionsChanged();
            return;
        }
    }
}

QVector<ActiveSession> SessionController::sessions() const
{
    return m_sessions;
}

QVariantList SessionController::sessionsAsVariantList() const
{
    QVariantList list;
    for (const ActiveSession &s : m_sessions) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), s.id);
        map.insert(QStringLiteral("connectionId"), s.connectionId);
        map.insert(QStringLiteral("title"), s.title);
        map.insert(QStringLiteral("status"), s.status);
        map.insert(QStringLiteral("lastMessage"), s.lastMessage);
        list.append(map);
    }
    return list;
}
