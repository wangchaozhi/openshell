#pragma once

#include <QObject>
#include <QRect>
#include <QString>

class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &language);

    bool minimizeToTray() const;
    void setMinimizeToTray(bool enabled);

    QString remoteFileOpenMode() const;
    void setRemoteFileOpenMode(const QString &mode);

    QString externalTextEditorPath() const;
    void setExternalTextEditorPath(const QString &path);

    bool autoUploadRemoteEdits() const;
    void setAutoUploadRemoteEdits(bool enabled);

    QRect mainWindowGeometry() const;
    void setMainWindowGeometry(const QRect &geometry);

    QString connectionsRoot() const;
};
