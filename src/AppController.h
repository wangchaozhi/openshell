#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ConnectionCatalog;
class SessionController;
class SettingsStore;
class TrayController;
class TranslationManager;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString currentConnectionId READ currentConnectionId WRITE setCurrentConnectionId NOTIFY currentConnectionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)

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
    Q_INVOKABLE void sendSessionInput(const QString &sessionId, const QString &text);
    Q_INVOKABLE void resizeSession(const QString &sessionId, int cols, int rows);
    Q_INVOKABLE QString localHomePath() const;
    Q_INVOKABLE QString localParentPath(const QString &path) const;
    Q_INVOKABLE QVariantList localDirectoryEntries(const QString &path) const;
    Q_INVOKABLE QString remoteHomePath(const QString &connectionId) const;
    Q_INVOKABLE QString remoteParentPath(const QString &path) const;
    Q_INVOKABLE QVariantList remoteDirectoryEntries(const QString &connectionId,
                                                    const QString &path);
    Q_INVOKABLE QString requestRemoteDirectoryEntries(const QString &connectionId,
                                                      const QString &path);

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void quit();

signals:
    void languageChanged();
    void currentConnectionChanged();
    void lastErrorChanged();
    void minimizeToTrayChanged();
    void showRequested();
    void hideRequested();
    void sessionsChanged();
    void sessionOutput(const QString &sessionId, const QString &chunk);
    void sessionStatusChanged(const QString &sessionId, const QString &status, const QString &message);
    void remoteDirectoryEntriesReady(const QString &requestId,
                                     const QString &connectionId,
                                     const QString &path,
                                     const QVariantList &entries,
                                     const QString &error);

private:
    void setLastError(const QString &message);

    QString m_currentConnectionId;
    QString m_lastError;
    SettingsStore *m_settings = nullptr;
    ConnectionCatalog *m_catalog = nullptr;
    SessionController *m_sessions = nullptr;
    TranslationManager *m_translations = nullptr;
    TrayController *m_tray = nullptr;
};
