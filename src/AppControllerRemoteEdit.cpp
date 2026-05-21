#include "AppController.h"

#include "AppControllerTransferProgressReporter.h"
#include "ConnectionCatalog.h"
#include "SettingsStore.h"
#include "ssh/SftpDirectoryLister.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
#include <QFileDialog>
#endif

QString AppController::chooseExternalTextEditor()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    setLastError(tr("External editor selection is not available on mobile"));
    return {};
#else
    return QFileDialog::getOpenFileName(nullptr,
                                        tr("Select text editor"),
                                        QDir::homePath(),
                                        tr("Applications (*.exe);;All files (*)"));
#endif
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
        AppControllerTransferProgressReporter progress(this,
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
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
        opened = QProcess::startDetached(externalTextEditorPath(), QStringList{localPath});
#endif
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

