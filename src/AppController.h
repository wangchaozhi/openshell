#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QDateTime>
#include <QHash>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>

class ConnectionCatalog;
class QFileSystemWatcher;
class SessionController;
class SettingsStore;
class SystemMonitorController;
class TrayController;
class TranslationManager;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString currentConnectionId READ currentConnectionId WRITE setCurrentConnectionId NOTIFY currentConnectionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(QString uiTheme READ uiTheme WRITE setUiTheme NOTIFY uiThemeChanged)
    Q_PROPERTY(QString remoteFileOpenMode READ remoteFileOpenMode WRITE setRemoteFileOpenMode NOTIFY remoteFileOpenSettingsChanged)
    Q_PROPERTY(QString externalTextEditorPath READ externalTextEditorPath WRITE setExternalTextEditorPath NOTIFY remoteFileOpenSettingsChanged)
    Q_PROPERTY(bool autoUploadRemoteEdits READ autoUploadRemoteEdits WRITE setAutoUploadRemoteEdits NOTIFY remoteFileOpenSettingsChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    QString language() const;
    void setLanguage(const QString &language);

    QString currentConnectionId() const;
    void setCurrentConnectionId(const QString &id);

    QString lastError() const;

    bool minimizeToTray() const;
    void setMinimizeToTray(bool enabled);

    QString uiTheme() const;
    void setUiTheme(const QString &theme);

    QString remoteFileOpenMode() const;
    void setRemoteFileOpenMode(const QString &mode);
    QString externalTextEditorPath() const;
    void setExternalTextEditorPath(const QString &path);
    bool autoUploadRemoteEdits() const;
    void setAutoUploadRemoteEdits(bool enabled);

    Q_INVOKABLE QRect mainWindowGeometry() const;
    Q_INVOKABLE void saveMainWindowGeometry(int x, int y, int w, int h);

    Q_INVOKABLE QVariantList connectionProfiles() const;
    Q_INVOKABLE QVariantList reloadConnectionProfiles();
    Q_INVOKABLE bool saveConnectionProfile(const QVariantMap &profile);
    Q_INVOKABLE bool deleteConnection(const QString &id);

    Q_INVOKABLE QString openSession(const QString &connectionId);
    Q_INVOKABLE void closeSession(const QString &sessionId);
    Q_INVOKABLE QVariantList sessions() const;
    Q_INVOKABLE QString sessionBuffer(const QString &sessionId) const;
    Q_INVOKABLE QObject *sessionScreen(const QString &sessionId) const;
    Q_INVOKABLE void sendSessionInput(const QString &sessionId, const QString &text);
    Q_INVOKABLE void sendSessionBytes(const QString &sessionId, const QByteArray &data);
    Q_INVOKABLE void sendSessionCtrlC(const QString &sessionId);
    Q_INVOKABLE void resizeSession(const QString &sessionId, int cols, int rows);
    Q_INVOKABLE void clearSessionBuffer(const QString &sessionId);
    Q_INVOKABLE QString localHomePath() const;
    Q_INVOKABLE QString localParentPath(const QString &path) const;
    Q_INVOKABLE QVariantList localDirectoryEntries(const QString &path) const;
    Q_INVOKABLE QString localPathFromUrl(const QString &url) const;
    Q_INVOKABLE QString chooseLocalFile();
    Q_INVOKABLE QString chooseLocalFolder();
    Q_INVOKABLE QString chooseDownloadFolder();
    // For mobile FileDialog results: Android SAF hands back content:// URIs
    // that QFile can read but QFileInfo / QDir / the existing upload path can
    // not walk. Stage the bytes into the app sandbox and return the staged
    // local path; on iOS / desktop just resolves to the file:// path.
    // Returns an empty string and sets lastError on failure.
    Q_INVOKABLE QString stageUrlForUpload(const QString &url);
    Q_INVOKABLE bool openLocalFolderForPath(const QString &path) const;
    Q_INVOKABLE QVariantList transferHistory() const;
    Q_INVOKABLE void saveTransferHistory(const QVariantList &history) const;
    Q_INVOKABLE QString remoteHomePath(const QString &connectionId) const;
    Q_INVOKABLE QString remoteParentPath(const QString &path) const;
    Q_INVOKABLE QVariantList remoteDirectoryEntries(const QString &connectionId,
                                                    const QString &path);
    Q_INVOKABLE QString requestRemoteDirectoryEntries(const QString &connectionId,
                                                      const QString &path);
    Q_INVOKABLE QString requestUploadLocalPath(const QString &connectionId,
                                               const QString &localPath,
                                               const QString &remoteDirectory);
    Q_INVOKABLE QString requestRemoteChmod(const QString &connectionId,
                                           const QString &remotePath,
                                           const QString &octalPermissions);
    Q_INVOKABLE QString requestRemoteDownload(const QString &connectionId,
                                              const QString &remotePath,
                                              const QString &localDirectory);
    Q_INVOKABLE void cancelRemoteOperation(const QString &requestId);
    Q_INVOKABLE QString requestOpenRemotePath(const QString &connectionId,
                                              const QString &remotePath);
    Q_INVOKABLE QString requestUploadEditedRemoteFile(const QString &connectionId,
                                                      const QString &localPath,
                                                      const QString &remotePath);
    Q_INVOKABLE QString chooseExternalTextEditor();
    Q_INVOKABLE QString readTextFile(const QString &path) const;
    Q_INVOKABLE bool saveTextFile(const QString &path, const QString &text);
    Q_INVOKABLE QString requestCreateRemotePath(const QString &connectionId,
                                                const QString &remoteDirectory,
                                                const QString &name,
                                                bool directory);
    Q_INVOKABLE QString requestRenameRemotePath(const QString &connectionId,
                                                const QString &oldPath,
                                                const QString &newName);
    Q_INVOKABLE QString requestDeleteRemotePath(const QString &connectionId,
                                                const QString &remotePath,
                                                bool recursive);
    Q_INVOKABLE QString requestSystemMonitorSnapshot(const QString &connectionId);
    Q_INVOKABLE QString remoteSiblingPath(const QString &path, const QString &name) const;
    Q_INVOKABLE void copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void quit();
    // 把 app 任务发送到后台（等效于按 home），仅 Android 有意义；其他平台
    // 返回 false 不做事。给移动端拦截系统 BACK 用，避免 Qt 把唯一窗口关掉
    // 之后再切回来变白屏。
    Q_INVOKABLE bool moveAppToBackground();

signals:
    void languageChanged();
    void currentConnectionChanged();
    void lastErrorChanged();
    void minimizeToTrayChanged();
    void uiThemeChanged();
    void remoteFileOpenSettingsChanged();
    void showRequested();
    void hideRequested();
    void sessionsChanged();
    void sessionActivationRequested(const QString &sessionId);
    void sessionScreenUpdated(const QString &sessionId);
    void sessionStatusChanged(const QString &sessionId, const QString &status, const QString &message);
    void remoteDirectoryEntriesReady(const QString &requestId,
                                     const QString &connectionId,
                                     const QString &path,
                                     const QVariantList &entries,
                                     const QString &error);
    void remoteOperationFinished(const QString &requestId,
                                 const QString &connectionId,
                                 const QString &operation,
                                 const QString &path,
                                 bool ok,
                                 const QString &message);
    void remoteOperationProgress(const QString &requestId,
                                 const QString &connectionId,
                                 const QString &operation,
                                 const QString &path,
                                 qint64 bytesDone,
                                 qint64 bytesTotal,
                                 double speedBytesPerSec);
    void remoteFileReadyForInternalEditor(const QString &connectionId,
                                          const QString &remotePath,
                                          const QString &localPath,
                                          const QString &text,
                                          const QString &error);
    void systemMonitorSnapshotReady(const QString &requestId,
                                    const QString &connectionId,
                                    const QVariantMap &snapshot,
                                    const QString &error);

private:
    struct RemoteEditWatch
    {
        QString connectionId;
        QString remotePath;
        QDateTime lastKnownModified;
    };
    void setLastError(const QString &message);
    void openDownloadedRemoteFile(const QString &connectionId,
                                  const QString &remotePath,
                                  const QString &localPath);
    void watchRemoteEditFile(const QString &connectionId,
                             const QString &remotePath,
                             const QString &localPath);
    void handleWatchedRemoteEditChanged(const QString &localPath);

    QString m_currentConnectionId;
    QString m_lastError;
    SettingsStore *m_settings = nullptr;
    ConnectionCatalog *m_catalog = nullptr;
    SessionController *m_sessions = nullptr;
    SystemMonitorController *m_monitor = nullptr;
    TranslationManager *m_translations = nullptr;
    TrayController *m_tray = nullptr;
    QFileSystemWatcher *m_remoteEditWatcher = nullptr;
    QHash<QString, RemoteEditWatch> m_remoteEditWatches;
    QHash<QString, QSharedPointer<std::atomic_bool>> m_transferCancelFlags;
};
