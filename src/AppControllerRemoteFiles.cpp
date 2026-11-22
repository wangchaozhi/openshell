#include "AppController.h"

#include "AppControllerTransferProgressReporter.h"
#include "ConnectionCatalog.h"
#include "SystemMonitorController.h"
#include "ssh/SftpDirectoryLister.h"

#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QSettings>
#include <QtConcurrent>
#include <optional>

QString stagePickedUpload(const QUrl &source, bool folder, const QTemporaryDir &sandbox);

namespace {
constexpr auto transferConflictSetting = "transfers/conflictPolicy";

SftpDirectoryLister::Conflict conflictPolicy(const QString &value)
{
    if (value == QStringLiteral("overwrite")) {
        return SftpDirectoryLister::Conflict::Overwrite;
    }
    if (value == QStringLiteral("skip")) {
        return SftpDirectoryLister::Conflict::Skip;
    }
    return SftpDirectoryLister::Conflict::Rename;
}
}

QString AppController::transferConflictPolicy() const
{
    return QSettings().value(transferConflictSetting, QStringLiteral("rename")).toString();
}

void AppController::setTransferConflictPolicy(const QString &policy)
{
    if (policy == QStringLiteral("rename") || policy == QStringLiteral("skip")
        || policy == QStringLiteral("overwrite")) {
        QSettings().setValue(transferConflictSetting, policy);
    }
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
    const qsizetype slash = clean.lastIndexOf(QLatin1Char('/'));
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
                releaseUnusedSftpProfile(connectionId);
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
    return requestUpload(connectionId, localPath, remoteDirectory, false, false);
}

QString AppController::requestUploadPickedPath(const QString &connectionId, const QString &url,
                                               const QString &remoteDirectory, bool folder)
{
    return requestUpload(connectionId, url, remoteDirectory, true, folder);
}

QString AppController::requestUpload(const QString &connectionId, const QString &source,
                                     const QString &remoteDirectory, bool fromPicker, bool folder)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto cancelFlag = QSharedPointer<std::atomic_bool>::create(false);
    m_transferCancelFlags.insert(requestId, cancelFlag);
    m_transferConnectionIds.insert(requestId, connectionId);
    const auto conflict = conflictPolicy(transferConflictPolicy());
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    if (profile.id.isEmpty()) {
        m_transferCancelFlags.remove(requestId);
        m_transferConnectionIds.remove(requestId);
        const QString error = tr("Unknown connection");
        setLastError(error);
        QTimer::singleShot(0, this, [this, requestId, connectionId, source, error]() {
            emit remoteOperationFinished(requestId, connectionId, QStringLiteral("upload"),
                                         source, false, error);
        });
        return requestId;
    }

    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, source]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                m_transferCancelFlags.remove(requestId);
                m_transferConnectionIds.remove(requestId);
                releaseUnusedSftpProfile(connectionId);
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId,
                                             connectionId,
                                             QStringLiteral("upload"),
                                             source,
                                             ok,
                                             message);
                watcher->deleteLater();
            });

    watcher->setFuture(QtConcurrent::run([this, requestId, connectionId, profile, source,
                                         remoteDirectory, cancelFlag, conflict, fromPicker, folder]() {
        QString localPath = source;
        std::optional<QTemporaryDir> sandbox;
        if (fromPicker) {
            sandbox.emplace();
            localPath = stagePickedUpload(QUrl(source), folder, *sandbox);
            if (localPath.isEmpty()) {
                return QVariantMap{{QStringLiteral("ok"), false},
                                   {QStringLiteral("message"), folder
                                       ? tr("Cannot read the selected upload folder")
                                       : tr("Cannot read picked content URL: %1").arg(source)}};
            }
        }
        QString error;
        AppControllerTransferProgressReporter progress(this,
                                          requestId,
                                          connectionId,
                                          QStringLiteral("upload"),
                                          source);
        const bool ok = SftpDirectoryLister::upload(profile,
                                                    localPath,
                                                    remoteDirectory,
                                                    &error,
                                                    [&progress](qint64 done, qint64 total) {
                                                        progress.report(done, total, done == 0 || done == total);
                                                    },
                                                    [cancelFlag]() {
                                                        return cancelFlag->load();
                                                    }, conflict);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"),
                      ok && error.isEmpty() ? QObject::tr("Upload completed") : error);
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
    const auto cancelFlag = QSharedPointer<std::atomic_bool>::create(false);
    m_transferCancelFlags.insert(requestId, cancelFlag);
    m_transferConnectionIds.insert(requestId, connectionId);
    const auto conflict = conflictPolicy(transferConflictPolicy());
    const ConnectionProfile profile = m_catalog->profileById(connectionId);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId, remotePath]() {
                const QVariantMap result = watcher->result();
                const bool ok = result.value(QStringLiteral("ok")).toBool();
                const QString message = result.value(QStringLiteral("message")).toString();
                m_transferCancelFlags.remove(requestId);
                m_transferConnectionIds.remove(requestId);
                releaseUnusedSftpProfile(connectionId);
                setLastError(ok ? QString() : message);
                emit remoteOperationFinished(requestId, connectionId, QStringLiteral("download"),
                                             remotePath, ok, message);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([this, requestId, connectionId, profile, remotePath,
                                         localDirectory, cancelFlag, conflict]() {
        QString error;
        QString downloadedPath;
        AppControllerTransferProgressReporter progress(this,
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
                                                         },
                                                         [cancelFlag]() {
                                                             return cancelFlag->load();
                                                         }, conflict);
        QVariantMap result;
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("message"), ok ? downloadedPath : (error.isEmpty() ? QObject::tr("Download failed") : error));
        return result;
    }));
    return requestId;
}

void AppController::cancelRemoteOperation(const QString &requestId)
{
    const auto it = m_transferCancelFlags.constFind(requestId);
    if (it == m_transferCancelFlags.constEnd()) {
        return;
    }
    (*it)->store(true);
}

QString AppController::requestSystemMonitorSnapshot(const QString &connectionId, bool includeDetails)
{
    return m_monitor->requestSnapshot(connectionId, m_catalog->profileById(connectionId), includeDetails);
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
