#include "AppController.h"

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "SettingsStore.h"
#include "ssh/SftpDirectoryLister.h"
#include "TrayController.h"
#include "TranslationManager.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

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

QString AppController::localHomePath() const
{
    return QDir::homePath();
}

QString AppController::localParentPath(const QString &path) const
{
    QDir dir(path.isEmpty() ? QDir::homePath() : path);
    dir.cdUp();
    return QDir::toNativeSeparators(dir.absolutePath());
}

QVariantList AppController::localDirectoryEntries(const QString &path) const
{
    const QString target = path.isEmpty() ? QDir::homePath() : path;
    const QDir dir(target);
    QVariantList entries;
    const QFileInfoList items = dir.entryInfoList(QDir::AllEntries
                                                      | QDir::NoDotAndDotDot
                                                      | QDir::Readable,
                                                  QDir::DirsFirst
                                                      | QDir::IgnoreCase
                                                      | QDir::Name);
    entries.reserve(items.size());
    for (const QFileInfo &item : items) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), item.fileName());
        row.insert(QStringLiteral("path"), QDir::toNativeSeparators(item.absoluteFilePath()));
        row.insert(QStringLiteral("isDir"), item.isDir());
        row.insert(QStringLiteral("size"), item.isDir() ? QStringLiteral("--")
                                                        : QString::number(item.size()));
        row.insert(QStringLiteral("modified"),
                   item.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        entries.append(row);
    }
    return entries;
}

QString AppController::remoteHomePath(const QString &connectionId) const
{
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    if (profile.username == QStringLiteral("root")) {
        return QStringLiteral("/root");
    }
    if (!profile.username.isEmpty()) {
        return QStringLiteral("/home/%1").arg(profile.username);
    }
    return QStringLiteral("/");
}

QString AppController::remoteParentPath(const QString &path) const
{
    if (path.isEmpty() || path == QStringLiteral("/")) {
        return QStringLiteral("/");
    }
    QString clean = path;
    while (clean.size() > 1 && clean.endsWith(QLatin1Char('/'))) {
        clean.chop(1);
    }
    const int slash = clean.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QStringLiteral("/");
    }
    return clean.left(slash);
}

QVariantList AppController::remoteDirectoryEntries(const QString &connectionId,
                                                   const QString &path)
{
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    if (profile.id.isEmpty()) {
        setLastError(tr("Unknown connection"));
        return {};
    }

    QString error;
    QVariantList rows = SftpDirectoryLister::list(profile,
                                                  path.isEmpty()
                                                      ? remoteHomePath(connectionId)
                                                      : path,
                                                  &error);
    if (!error.isEmpty()) {
        setLastError(error);
    } else {
        setLastError(QString());
    }
    return rows;
}

QString AppController::requestRemoteDirectoryEntries(const QString &connectionId,
                                                     const QString &path)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    const QString targetPath = path.isEmpty() ? remoteHomePath(connectionId) : path;

    if (profile.id.isEmpty()) {
        const QString error = tr("Unknown connection");
        setLastError(error);
        QTimer::singleShot(0, this, [this, requestId, connectionId, targetPath, error]() {
            emit remoteDirectoryEntriesReady(requestId, connectionId, targetPath, {}, error);
        });
        return requestId;
    }

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, targetPath]() {
                const QVariantMap result = watcher->result();
                const QVariantList entries = result.value(QStringLiteral("entries")).toList();
                const QString error = result.value(QStringLiteral("error")).toString();
                setLastError(error);
                emit remoteDirectoryEntriesReady(requestId,
                                                 connectionId,
                                                 targetPath,
                                                 entries,
                                                 error);
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run([profile, targetPath]() {
        QString error;
        QVariantList entries = SftpDirectoryLister::list(profile, targetPath, &error);
        QVariantMap result;
        result.insert(QStringLiteral("entries"), entries);
        result.insert(QStringLiteral("error"), error);
        return result;
    }));

    return requestId;
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
