#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "ConnectionCatalog.h"
#include "ssh/SshWorkerFactory.h"

class SshSession;

class SessionController : public QObject
{
    Q_OBJECT

public:
    explicit SessionController(QObject *parent = nullptr);
    ~SessionController() override;

    // 默认工厂返回 Libssh2ChannelWorker（生产）。测试可注入 Echo 工厂。
    void setWorkerFactory(SshWorkerFactory factory);

    QString open(const ConnectionProfile &profile, QString *error = nullptr);
    void close(const QString &sessionId);
    void sendInput(const QString &sessionId, const QByteArray &data);
    void requestResize(const QString &sessionId, int cols, int rows);
    void clearBuffer(const QString &sessionId);

    QVariantList sessionsAsVariantList() const;
    QString sessionBuffer(const QString &sessionId) const;
    QObject *sessionScreen(const QString &sessionId) const;
    bool contains(const QString &sessionId) const;

signals:
    void sessionsChanged();
    void sessionScreenUpdated(const QString &sessionId);
    void sessionStatusChanged(const QString &sessionId, const QString &status, const QString &message);

private:
    QVariantMap toVariantMap(const SshSession *session) const;
    SshSession *findSession(const QString &sessionId) const;

    QVector<SshSession *> m_sessions;
    SshWorkerFactory m_workerFactory;
};
