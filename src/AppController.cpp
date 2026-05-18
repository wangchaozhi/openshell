#include "AppController.h"

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "SettingsStore.h"
#include "SystemMonitorController.h"
#include "ssh/SftpDirectoryLister.h"
#include "TrayController.h"
#include "TranslationManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>

#include <utility>

namespace {
class TransferProgressReporter
{
public:
    TransferProgressReporter(AppController *controller,
                             QString requestId,
                             QString connectionId,
                             QString operation,
                             QString path)
        : m_controller(controller)
        , m_requestId(std::move(requestId))
        , m_connectionId(std::move(connectionId))
        , m_operation(std::move(operation))
        , m_path(std::move(path))
    {
        m_timer.start();
    }

    void report(qint64 bytesDone, qint64 bytesTotal, bool force = false)
    {
        const qint64 elapsed = qMax<qint64>(1, m_timer.elapsed());
        if (!force && bytesDone != 0 && bytesDone != bytesTotal && elapsed - m_lastEmitMs < 200) {
            return;
        }
        m_lastEmitMs = elapsed;
        const double speed = elapsed > 0
                                 ? (static_cast<double>(bytesDone) * 1000.0 / static_cast<double>(elapsed))
                                 : 0.0;
        QMetaObject::invokeMethod(m_controller,
                                  [controller = m_controller,
                                   requestId = m_requestId,
                                   connectionId = m_connectionId,
                                   operation = m_operation,
                                   path = m_path,
                                   bytesDone,
                                   bytesTotal,
                                   speed]() {
                                      emit controller->remoteOperationProgress(requestId,
                                                                               connectionId,
                                                                               operation,
                                                                               path,
                                                                               bytesDone,
                                                                               bytesTotal,
                                                                               speed);
                                  },
                                  Qt::QueuedConnection);
    }

private:
    AppController *m_controller = nullptr;
    QString m_requestId;
    QString m_connectionId;
    QString m_operation;
    QString m_path;
    QElapsedTimer m_timer;
    qint64 m_lastEmitMs = -1000;
};
}

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings(new SettingsStore(this))
    , m_catalog(new ConnectionCatalog(this))
    , m_sessions(new SessionController(this))
    , m_monitor(new SystemMonitorController(this))
    , m_translations(new TranslationManager(this))
    , m_tray(new TrayController(this))
    , m_remoteEditWatcher(new QFileSystemWatcher(this))
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

QString AppController::chooseExternalTextEditor()
{
    return QFileDialog::getOpenFileName(nullptr,
                                        tr("Select text editor"),
                                        QDir::homePath(),
                                        tr("Applications (*.exe);;All files (*)"));
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

QString AppController::clipboardText() const
{
    return QApplication::clipboard()->text();
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

    watcher->setFuture(QtConcurrent::run([this, requestId, connectionId, profile, localPath, remoteDirectory]() {
        QString error;
        TransferProgressReporter progress(this,
                                          requestId,
                                          connectionId,
                                          QStringLiteral("upload"),
                                          localPath);
        const bool ok = SftpDirectoryLister::upload(profile,
                                                    localPath,
                                                    remoteDirectory,
                                                    &error,
                                                    [&progress](qint64 done, qint64 total) {
                                                        progress.report(done, total, done == 0 || done == total);
                                                    });
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
    watcher->setFuture(QtConcurrent::run([this, requestId, connectionId, profile, remotePath, localDirectory]() {
        QString error;
        QString downloadedPath;
        TransferProgressReporter progress(this,
                                          requestId,
                                          connectionId,
                                          QStringLiteral("download"),
                                          remotePath);
        const bool ok = !profile.id.isEmpty()
                        && SftpDirectoryLister::download(profile,
                                                         remotePath,
                                                         localDirectory,
                                                         &downloadedPath,
                                                         &error,
                                                         [&progress](qint64 done, qint64 total) {
                                                             progress.report(done, total, done == 0 || done == total);
                                                         });
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? downloadedPath : (error.isEmpty() ? QObject::tr("Download failed") : error));
        return result;
    }));
    return requestId;
}

QString AppController::requestSystemMonitorSnapshot(const QString &connectionId)
{
    return m_monitor->requestSnapshot(connectionId, m_catalog->profileById(connectionId));
}

QString AppController::requestOpenRemotePath(const QString &connectionId,
                                             const QString &remotePath)
{
    const QString tempRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("OpenShell/remote-open/%1")
                                               .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    QDir().mkpath(tempRoot);
    const QString requestId = requestRemoteDownload(connectionId, remotePath, tempRoot);
    connect(this, &AppController::remoteOperationFinished, this,
            [this, requestId](const QString &finishedId,
                        const QString &finishedConnectionId,
                        const QString &operation,
                        const QString &finishedRemotePath,
                        bool ok,
                        const QString &message) {
                if (finishedId == requestId && operation == QStringLiteral("download") && ok) {
                    openDownloadedRemoteFile(finishedConnectionId, finishedRemotePath, message);
                }
            }, Qt::SingleShotConnection);
    return requestId;
}

QString AppController::requestUploadEditedRemoteFile(const QString &connectionId,
                                                     const QString &localPath,
                                                     const QString &remotePath)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    const QString remoteDirectory = remoteParentPath(remotePath);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, remotePath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                setLastError(ok ? QString() : message);
                if (ok && m_remoteEditWatches.contains(result.value(QStringLiteral("localPath")).toString())) {
                    auto edit = m_remoteEditWatches.value(result.value(QStringLiteral("localPath")).toString());
                    edit.lastKnownModified = QFileInfo(result.value(QStringLiteral("localPath")).toString()).lastModified();
                    m_remoteEditWatches.insert(result.value(QStringLiteral("localPath")).toString(), edit);
                }
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("upload"),
                                             remotePath, ok, message);
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run([this, requestId, connectionId, profile, localPath, remotePath, remoteDirectory]() {
        QString error;
        TransferProgressReporter progress(this,
                                          requestId,
                                          connectionId,
                                          QStringLiteral("upload"),
                                          remotePath);
        const QFileInfo info(localPath);
        const bool ok = !profile.id.isEmpty()
                        && info.exists()
                        && info.fileName() == QFileInfo(remotePath).fileName()
                        && SftpDirectoryLister::upload(profile,
                                                       localPath,
                                                       remoteDirectory,
                                                       &error,
                                                       [&progress](qint64 done, qint64 total) {
                                                           progress.report(done, total, done == 0 || done == total);
                                                       });
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("localPath"), localPath);
        result.insert(QStringLiteral("message"),
                      ok ? QObject::tr("Remote file saved") : (error.isEmpty() ? QObject::tr("Upload failed") : error));
        return result;
    }));
    return requestId;
}

QString AppController::readTextFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

bool AppController::saveTextFile(const QString &path, const QString &text)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setLastError(tr("Cannot write %1").arg(path));
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    if (!file.commit()) {
        setLastError(tr("Cannot save %1").arg(path));
        return false;
    }
    setLastError(QString());
    return true;
}

void AppController::openDownloadedRemoteFile(const QString &connectionId,
                                             const QString &remotePath,
                                             const QString &localPath)
{
    const QString mode = remoteFileOpenMode();
    if (mode == QStringLiteral("internal")) {
        emit remoteFileReadyForInternalEditor(connectionId,
                                              remotePath,
                                              localPath,
                                              readTextFile(localPath),
                                              QString());
        return;
    }

    if (autoUploadRemoteEdits()) {
        watchRemoteEditFile(connectionId, remotePath, localPath);
    }

    bool opened = false;
    if (mode == QStringLiteral("custom") && !externalTextEditorPath().isEmpty()) {
        opened = QProcess::startDetached(externalTextEditorPath(), QStringList{localPath});
    }
    if (!opened) {
        opened = QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    }
    setLastError(opened ? QString() : tr("Cannot open %1").arg(localPath));
}

void AppController::watchRemoteEditFile(const QString &connectionId,
                                        const QString &remotePath,
                                        const QString &localPath)
{
    const QFileInfo info(localPath);
    if (!info.exists()) {
        return;
    }
    RemoteEditWatch edit;
    edit.connectionId = connectionId;
    edit.remotePath = remotePath;
    edit.lastKnownModified = info.lastModified();
    m_remoteEditWatches.insert(localPath, edit);
    if (!m_remoteEditWatcher->files().contains(localPath)) {
        m_remoteEditWatcher->addPath(localPath);
    }
}

void AppController::handleWatchedRemoteEditChanged(const QString &localPath)
{
    if (!m_remoteEditWatches.contains(localPath)) {
        return;
    }
    if (QFileInfo::exists(localPath) && !m_remoteEditWatcher->files().contains(localPath)) {
        m_remoteEditWatcher->addPath(localPath);
    }
    QTimer::singleShot(900, this, [this, localPath]() {
        if (!autoUploadRemoteEdits() || !m_remoteEditWatches.contains(localPath)) {
            return;
        }
        const QFileInfo info(localPath);
        if (!info.exists()) {
            m_remoteEditWatches.remove(localPath);
            return;
        }
        const RemoteEditWatch edit = m_remoteEditWatches.value(localPath);
        if (info.lastModified() <= edit.lastKnownModified) {
            return;
        }
        requestUploadEditedRemoteFile(edit.connectionId, localPath, edit.remotePath);
    });
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
