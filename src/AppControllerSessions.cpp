#include "AppController.h"

#include "SessionController.h"
#include "SettingsStore.h"
#include "terminal/VtScreen.h"
#include "ssh/SftpConnectionPool.h"

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

    if (auto *screen = qobject_cast<VtScreen *>(m_sessions->sessionScreen(sessionId))) {
        screen->setScrollbackLimit(m_settings->terminalHistoryLines());
    }

    setCurrentConnectionId(connectionId);
    setLastError(QString());
    return sessionId;
}

int AppController::terminalHistoryLines() const
{
    return m_settings->terminalHistoryLines();
}

void AppController::setTerminalHistoryLines(int lines)
{
    m_settings->setTerminalHistoryLines(lines);
    const int limit = m_settings->terminalHistoryLines();
    for (const auto &entry : m_sessions->sessionsAsVariantList()) {
        const QString sessionId = entry.toMap().value(QStringLiteral("id")).toString();
        if (auto *screen = qobject_cast<VtScreen *>(m_sessions->sessionScreen(sessionId))) {
            screen->setScrollbackLimit(limit);
        }
    }
}

bool AppController::closeSession(const QString &sessionId, bool cancelTransfers)
{
    const auto active = m_sessions->sessionsAsVariantList();
    QString profileId;
    for (const QVariant &entry : active) {
        const auto session = entry.toMap();
        if (session.value(QStringLiteral("id")).toString() == sessionId) {
            profileId = session.value(QStringLiteral("connectionId")).toString();
            break;
        }
    }
    bool lastSession = !profileId.isEmpty();
    for (const QVariant &entry : active) {
        const auto session = entry.toMap();
        if (session.value(QStringLiteral("id")).toString() != sessionId
            && session.value(QStringLiteral("connectionId")).toString() == profileId) {
            lastSession = false;
            break;
        }
    }
    if (lastSession) {
        const auto requests = m_transferConnectionIds.keys(profileId);
        if (!requests.isEmpty() && !cancelTransfers) {
            return false;
        }
        for (const QString &request : requests) {
            cancelRemoteOperation(request);
        }
    }
    m_sessions->close(sessionId);
    if (lastSession) {
        SftpConnectionPool::releaseProfile(profileId);
    }
    return true;
}

void AppController::releaseUnusedSftpProfile(const QString &connectionId)
{
    if (connectionId.isEmpty() || m_transferConnectionIds.values().contains(connectionId)) {
        return;
    }
    const auto active = m_sessions->sessionsAsVariantList();
    for (const QVariant &entry : active) {
        if (entry.toMap().value(QStringLiteral("connectionId")).toString() == connectionId) {
            return;
        }
    }
    SftpConnectionPool::releaseProfile(connectionId);
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
