#include "AppController.h"

#include "SessionController.h"

QString AppController::openSession(const QString &connectionId)
{
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    if (profile.id.isEmpty()) {
        setLastError(tr("Unknown connection"));
        return QString();
    }

    QString error;
    const QString sessionId = m_sessions->open(profile, &error);
    if (sessionId.isEmpty()) {
        setLastError(error);
        return QString();
    }

    setCurrentConnectionId(connectionId);
    setLastError(QString());
    return sessionId;
}

void AppController::closeSession(const QString &sessionId)
{
    m_sessions->close(sessionId);
}

QVariantList AppController::sessions() const
{
    return m_sessions->sessionsAsVariantList();
}

QString AppController::sessionBuffer(const QString &sessionId) const
{
    return m_sessions->sessionBuffer(sessionId);
}

void AppController::sendSessionInput(const QString &sessionId, const QString &text)
{
    m_sessions->sendInput(sessionId, text.toUtf8());
}

void AppController::sendSessionBytes(const QString &sessionId, const QByteArray &data)
{
    m_sessions->sendInput(sessionId, data);
}

void AppController::sendSessionCtrlC(const QString &sessionId)
{
    m_sessions->sendInput(sessionId, QByteArray(1, '\x03'));
}

QObject *AppController::sessionScreen(const QString &sessionId) const
{
    return m_sessions->sessionScreen(sessionId);
}

void AppController::resizeSession(const QString &sessionId, int cols, int rows)
{
    m_sessions->requestResize(sessionId, cols, rows);
}

void AppController::clearSessionBuffer(const QString &sessionId)
{
    m_sessions->clearBuffer(sessionId);
}

