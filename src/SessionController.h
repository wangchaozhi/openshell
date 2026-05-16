#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "ConnectionCatalog.h"

struct ActiveSession
{
    QString id;
    QString connectionId;
    QString title;
    QString status; // connecting, connected, disconnected, error
    QString lastMessage;
};

class SessionController : public QObject
{
    Q_OBJECT

public:
    explicit SessionController(QObject *parent = nullptr);

    bool open(const ConnectionProfile &profile, QString *error = nullptr);
    void close(const QString &sessionId);

    QVector<ActiveSession> sessions() const;
    QVariantList sessionsAsVariantList() const;

signals:
    void sessionsChanged();
    void sessionOutput(const QString &sessionId, const QString &chunk);

private:
    QVector<ActiveSession> m_sessions;
};
