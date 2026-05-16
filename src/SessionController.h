#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "ConnectionCatalog.h"

class SshSession;

class SessionController : public QObject
{
    Q_OBJECT

public:
    explicit SessionController(QObject *parent = nullptr);
    ~SessionController() override;

    QString open(const ConnectionProfile &profile, QString *error = nullptr);
    void close(const QString &sessionId);
    void sendInput(const QString &sessionId, const QByteArray &data);
    void requestResize(const QString &sessionId, int cols, int rows);

    QVariantList sessionsAsVariantList() const;
    QString sessionBuffer(const QString &sessionId) const;
    bool contains(const QString &sessionId) const;

signals:
    void sessionsChanged();
    void sessionOutput(const QString &sessionId, const QByteArray &chunk);
    void sessionStatusChanged(const QString &sessionId, const QString &status, const QString &message);

private:
    QVariantMap toVariantMap(const SshSession *session) const;
    SshSession *findSession(const QString &sessionId) const;

    QVector<SshSession *> m_sessions;
};
