#include "AppController.h"

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "SettingsStore.h"
#include "ssh/SftpDirectoryLister.h"
#include "TrayController.h"
#include "TranslationManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
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
        // Windows 简化权限：映射成 Unix 数字供 QML 统一转换
        QString perm;
        if (item.isDir()) {
            perm = item.isWritable() ? QStringLiteral("755") : QStringLiteral("555");
        } else {
            perm = item.isWritable() ? QStringLiteral("644") : QStringLiteral("444");
        }
        row.insert(QStringLiteral("permissions"), perm);
        entries.append(row);
    }
    return entries;
}

QString AppController::localPathFromUrl(const QString &url) const
{
    const QUrl parsed(url);
    if (parsed.isLocalFile()) {
        return parsed.toLocalFile();
    }
    return url;
}

QString AppController::chooseLocalFile()
{
    return QFileDialog::getOpenFileName(nullptr, tr("Select file to upload"), QDir::homePath());
}

QString AppController::chooseLocalFolder()
{
    return QFileDialog::getExistingDirectory(nullptr, tr("Select folder to upload"), QDir::homePath());
}

QString AppController::chooseDownloadFolder()
{
    return QFileDialog::getExistingDirectory(nullptr, tr("Select download folder"), QDir::homePath());
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

QString AppController::remoteSiblingPath(const QString &path, const QString &name) const
{
    const QString parent = remoteParentPath(path);
    if (parent == QStringLiteral("/")) {
        return QStringLiteral("/") + name;
    }
    return parent + QStringLiteral("/") + name;
}

void AppController::copyTextToClipboard(const QString &text) const
{
    QApplication::clipboard()->setText(text);
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

QString AppController::requestUploadLocalPath(const QString &connectionId,
                                              const QString &localPath,
                                              const QString &remoteDirectory)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    if (profile.id.isEmpty()) {
        const QString error = tr("Unknown connection");
        setLastError(error);
        QTimer::singleShot(0, this, [this, requestId, connectionId, localPath, error]() {
            emit remoteOperationFinished(requestId, connectionId, QStringLiteral("upload"),
                                         localPath, false, error);
        });
        return requestId;
    }

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, localPath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId,
                                             connectionId,
                                             QStringLiteral("upload"),
                                             localPath,
                                             ok,
                                             message);
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run([profile, localPath, remoteDirectory]() {
        QString error;
        const bool ok = SftpDirectoryLister::upload(profile, localPath, remoteDirectory, &error);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"),
                      ok ? QObject::tr("Upload completed") : error);
        return result;
    }));

    return requestId;
}

QString AppController::requestRemoteChmod(const QString &connectionId,
                                          const QString &remotePath,
                                          const QString &octalPermissions)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    bool parseOk = false;
    const int permissions = octalPermissions.toInt(&parseOk, 8);
    if (profile.id.isEmpty() || !parseOk) {
        const QString error = profile.id.isEmpty()
                                  ? tr("Unknown connection")
                                  : tr("Invalid permission value");
        setLastError(error);
        QTimer::singleShot(0, this, [this, requestId, connectionId, remotePath, error]() {
            emit remoteOperationFinished(requestId, connectionId, QStringLiteral("chmod"),
                                         remotePath, false, error);
        });
        return requestId;
    }

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, remotePath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId,
                                             connectionId,
                                             QStringLiteral("chmod"),
                                             remotePath,
                                             ok,
                                             message);
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run([profile, remotePath, permissions]() {
        QString error;
        const bool ok = SftpDirectoryLister::chmod(profile, remotePath, permissions, &error);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"),
                      ok ? QObject::tr("Permissions updated") : error);
        return result;
    }));

    return requestId;
}

QString AppController::requestRemoteDownload(const QString &connectionId,
                                             const QString &remotePath,
                                             const QString &localDirectory)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, remotePath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("download"),
                                             remotePath, ok, message);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([profile, remotePath, localDirectory]() {
        QString error;
        QString downloadedPath;
        const bool ok = !profile.id.isEmpty()
                        && SftpDirectoryLister::download(profile, remotePath, localDirectory, &downloadedPath, &error);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? downloadedPath : (error.isEmpty() ? QObject::tr("Download failed") : error));
        return result;
    }));
    return requestId;
}

QString AppController::requestOpenRemotePath(const QString &connectionId,
                                             const QString &remotePath)
{
    const QString tempRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("OpenShell/remote-open"));
    QDir().mkpath(tempRoot);
    const QString requestId = requestRemoteDownload(connectionId, remotePath, tempRoot);
    connect(this, &AppController::remoteOperationFinished, this,
            [requestId](const QString &finishedId,
                        const QString &,
                        const QString &operation,
                        const QString &,
                        bool ok,
                        const QString &message) {
                if (finishedId == requestId && operation == QStringLiteral("download") && ok) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(message));
                }
            }, Qt::SingleShotConnection);
    return requestId;
}

QString AppController::requestCreateRemotePath(const QString &connectionId,
                                               const QString &remoteDirectory,
                                               const QString &name,
                                               bool directory)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    const QString path = remoteDirectory == QStringLiteral("/")
                             ? QStringLiteral("/") + name
                             : remoteDirectory + QStringLiteral("/") + name;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, path]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("create"),
                                             path, ok, message);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([profile, path, directory]() {
        QString error;
        const bool ok = !profile.id.isEmpty()
                        && (directory
                                ? SftpDirectoryLister::createDirectory(profile, path, &error)
                                : SftpDirectoryLister::createFile(profile, path, &error));
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? QObject::tr("Created") : (error.isEmpty() ? QObject::tr("Create failed") : error));
        return result;
    }));
    return requestId;
}

QString AppController::requestRenameRemotePath(const QString &connectionId,
                                               const QString &oldPath,
                                               const QString &newName)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    const QString newPath = remoteSiblingPath(oldPath, newName);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, oldPath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("rename"),
                                             oldPath, ok, message);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([profile, oldPath, newPath]() {
        QString error;
        const bool ok = !profile.id.isEmpty()
                        && SftpDirectoryLister::renamePath(profile, oldPath, newPath, &error);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? QObject::tr("Renamed") : (error.isEmpty() ? QObject::tr("Rename failed") : error));
        return result;
    }));
    return requestId;
}

QString AppController::requestDeleteRemotePath(const QString &connectionId,
                                               const QString &remotePath,
                                               bool recursive)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, remotePath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("delete"),
                                             remotePath, ok, message);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([profile, remotePath, recursive]() {
        QString error;
        const bool ok = !profile.id.isEmpty()
                        && SftpDirectoryLister::removePath(profile, remotePath, recursive, &error);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? QObject::tr("Deleted") : (error.isEmpty() ? QObject::tr("Delete failed") : error));
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
