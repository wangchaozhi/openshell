#include "AppController.h"

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "SettingsStore.h"
#include "SystemMonitorController.h"
#include "TranslationManager.h"

#include <QFileSystemWatcher>
#include <QRect>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include "TrayController.h"
#endif

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(new SettingsStore(this))
    , m_catalog(new ConnectionCatalog(this))
    , m_sessions(new SessionController(this))
    , m_monitor(new SystemMonitorController(this))
    , m_translations(new TranslationManager(this))
    , m_remoteEditWatcher(new QFileSystemWatcher(this))
{
    m_translations->installLanguage(m_settings->language());

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    m_tray = new TrayController(this);
    if (m_tray) {
        m_tray->setLanguage(m_translations->language());
        m_tray->setConnections(m_catalog->profiles());

        connect(m_tray, &TrayController::showRequested, this, &AppController::showWindow);
        connect(m_tray, &TrayController::hideRequested, this, &AppController::hideWindow);
        connect(m_tray, &TrayController::languageChanged, this, &AppController::setLanguage);
        connect(m_tray, &TrayController::connectionTriggered, this,
                [this](const QString &connectionId) {
                    const QString sessionId = openSession(connectionId);
                    if (sessionId.isEmpty()) {
                        return;
                    }
                    showWindow();
                    emit sessionActivationRequested(sessionId);
                });
        connect(m_tray, &TrayController::quitRequested, this, &AppController::quit);
    }
#endif

    connect(m_sessions, &SessionController::sessionsChanged, this, &AppController::sessionsChanged);
    connect(m_sessions, &SessionController::sessionScreenUpdated, this,
            &AppController::sessionScreenUpdated);
    connect(m_sessions, &SessionController::sessionStatusChanged, this,
            &AppController::sessionStatusChanged);
    connect(m_monitor, &SystemMonitorController::snapshotReady, this,
            &AppController::systemMonitorSnapshotReady);
    connect(m_remoteEditWatcher, &QFileSystemWatcher::fileChanged, this,
            &AppController::handleWatchedRemoteEditChanged);
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
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    if (m_tray) {
        m_tray->setLanguage(language);
    }
#endif
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

QString AppController::uiTheme() const
{
    const QString theme = m_settings->uiTheme();
    return (theme == QStringLiteral("classic") || theme == QStringLiteral("forest"))
               ? theme
               : QStringLiteral("dark");
}

void AppController::setUiTheme(const QString &theme)
{
    const QString normalized =
        (theme == QStringLiteral("classic") || theme == QStringLiteral("forest"))
            ? theme
            : QStringLiteral("dark");
    if (uiTheme() == normalized) {
        return;
    }
    m_settings->setUiTheme(normalized);
    emit uiThemeChanged();
}

QString AppController::remoteFileOpenMode() const
{
    const QString mode = m_settings->remoteFileOpenMode();
    if (mode == QStringLiteral("custom") || mode == QStringLiteral("internal")) {
        return mode;
    }
    return QStringLiteral("system");
}

void AppController::setRemoteFileOpenMode(const QString &mode)
{
    const QString normalized = (mode == QStringLiteral("custom") || mode == QStringLiteral("internal"))
                                   ? mode
                                   : QStringLiteral("system");
    if (remoteFileOpenMode() == normalized) {
        return;
    }
    m_settings->setRemoteFileOpenMode(normalized);
    emit remoteFileOpenSettingsChanged();
}

QString AppController::externalTextEditorPath() const
{
    return m_settings->externalTextEditorPath();
}

void AppController::setExternalTextEditorPath(const QString &path)
{
    if (m_settings->externalTextEditorPath() == path) {
        return;
    }
    m_settings->setExternalTextEditorPath(path);
    emit remoteFileOpenSettingsChanged();
}

bool AppController::autoUploadRemoteEdits() const
{
    return m_settings->autoUploadRemoteEdits();
}

void AppController::setAutoUploadRemoteEdits(bool enabled)
{
    if (m_settings->autoUploadRemoteEdits() == enabled) {
        return;
    }
    m_settings->setAutoUploadRemoteEdits(enabled);
    emit remoteFileOpenSettingsChanged();
}

QRect AppController::mainWindowGeometry() const
{
    return m_settings->mainWindowGeometry();
}

void AppController::saveMainWindowGeometry(int x, int y, int w, int h)
{
    m_settings->setMainWindowGeometry(QRect(x, y, w, h));
}
