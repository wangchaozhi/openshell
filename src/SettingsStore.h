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

    QRect mainWindowGeometry() const;
    void setMainWindowGeometry(const QRect &geometry);

    QString connectionsRoot() const;
};
