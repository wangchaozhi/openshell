#include "AppController.h"

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "SettingsStore.h"
#include "TrayController.h"
#include "TranslationManager.h"

#include <QApplication>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(new SettingsStore(this))
    , m_catalog(new ConnectionCatalog(this))
    , m_sessions(new SessionController(this))
    , m_translations(new TranslationManager(this))
    , m_tray(new TrayController(this))
{
    m_translations->installLanguage(m_settings->language());
    m_tray->setLanguage(m_translations->language());
    m_tray->setConnections(m_catalog->profiles());

    connect(m_tray, &TrayController::showRequested, this, &AppController::showWindow);
    connect(m_tray, &TrayController::hideRequested, this, &AppController::hideWindow);
    connect(m_tray, &TrayController::languageChanged, this, &AppController::setLanguage);
    connect(m_tray, &TrayController::connectionTriggered, this,
            [this](const QString &connectionId) { openSession(connectionId); });
    connect(m_tray, &TrayController::quitRequested, this, &AppController::quit);

    connect(m_sessions, &SessionController::sessionsChanged, this, &AppController::sessionsChanged);
    connect(m_sessions, &SessionController::sessionOutput, this,
            [this](const QString &sessionId, const QByteArray &chunk) {
                emit sessionOutput(sessionId, QString::fromUtf8(chunk));
            });
    connect(m_sessions, &SessionController::sessionStatusChanged, this,
            &AppController::sessionStatusChanged);
}

AppController::~AppController() = default;

QString AppController::language() const
{
    return m_translations->language();
}

void AppController::setLanguage(const QString &language)
{
    if (m_translations->language() == language) {
        return;
    }

    m_settings->setLanguage(language);
    m_translations->installLanguage(language);
    m_tray->setLanguage(language);
    emit languageChanged();
}

QString AppController::currentConnectionId() const
{
    return m_currentConnectionId;
}

void AppController::setCurrentConnectionId(const QString &id)
{
    if (m_currentConnectionId == id) {
        return;
    }
    m_currentConnectionId = id;
    emit currentConnectionChanged();
}

QString AppController::lastError() const
{
    return m_lastError;
}

void AppController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

bool AppController::minimizeToTray() const
{
    return m_settings->minimizeToTray();
}

void AppController::setMinimizeToTray(bool enabled)
{
    if (m_settings->minimizeToTray() == enabled) {
        return;
    }
    m_settings->setMinimizeToTray(enabled);
    emit minimizeToTrayChanged();
}

QRect AppController::mainWindowGeometry() const
{
    return m_settings->mainWindowGeometry();
}

void AppController::saveMainWindowGeometry(int x, int y, int w, int h)
{
    m_settings->setMainWindowGeometry(QRect(x, y, w, h));
}

QVariantList AppController::connectionProfiles() const
{
    QVariantList list;
    for (const ConnectionProfile &profile : m_catalog->profiles()) {
        list.append(profile.toVariantMap());
    }
    return list;
}

QVariantList AppController::reloadConnectionProfiles()
{
    m_catalog->reload();
    m_tray->setConnections(m_catalog->profiles());
    return connectionProfiles();
}

bool AppController::saveConnectionProfile(const QVariantMap &profile)
{
    QString error;
    if (!m_catalog->upsert(ConnectionProfile::fromVariantMap(profile), &error)) {
        setLastError(error);
        return false;
    }

    m_tray->setConnections(m_catalog->profiles());
    setLastError(QString());
    return true;
}

bool AppController::deleteConnection(const QString &id)
{
    QString error;
    if (!m_catalog->remove(id, &error)) {
        setLastError(error);
        return false;
    }

    m_tray->setConnections(m_catalog->profiles());
    if (m_currentConnectionId == id) {
        setCurrentConnectionId(QString());
    }
    setLastError(QString());
    return true;
}

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

void AppController::resizeSession(const QString &sessionId, int cols, int rows)
{
    m_sessions->requestResize(sessionId, cols, rows);
}

void AppController::showWindow()
{
    emit showRequested();
}

void AppController::hideWindow()
{
    emit hideRequested();
}

void AppController::quit()
{
    QApplication::quit();
}
