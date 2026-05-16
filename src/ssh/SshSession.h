#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QThread>

#include "ConnectionCatalog.h"

class SshChannelWorker;

// SshSession 把一个 worker thread + 一个 SshChannelWorker 打包起来。
// SessionController 持有它，QML 通过 sessionId 来引用它。
// 状态机：disconnected -> connecting -> connected -> disconnected/error。
class SshSession : public QObject
{
    Q_OBJECT

public:
    SshSession(const QString &id,
               const ConnectionProfile &profile,
               SshChannelWorker *worker,
               QObject *parent = nullptr);
    ~SshSession() override;

    QString id() const;
    QString connectionId() const;
    QString title() const;
    QString status() const; // connecting/connected/disconnected/error
    QString lastMessage() const;
    QString buffer() const; // 累积的原始输出，切 tab 时可重放
    qsizetype bufferSize() const;

    void start();
    void requestStop();
    void sendInput(const QByteArray &data);
    void requestResize(int cols, int rows);
    void clearBuffer();

signals:
    void statusChanged();
    void outputAppended(const QByteArray &chunk);

private slots:
    void handleConnected();
    void handleDisconnected(const QString &reason);
    void handleOutput(const QByteArray &chunk);
    void handleError(const QString &message);

private:
    void setStatus(const QString &status, const QString &message = QString());
    void appendBuffer(const QByteArray &chunk);

    QString m_id;
    QString m_connectionId;
    QString m_title;
    QString m_status;
    QString m_lastMessage;
    QByteArray m_buffer;

    QThread m_thread;
    SshChannelWorker *m_worker = nullptr; // lives on m_thread

    static constexpr qsizetype kBufferCap = 256 * 1024; // 256KB 滚动缓冲
};
