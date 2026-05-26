#pragma once

#include "SshChannelWorker.h"

#include <QByteArray>
#include <QTcpSocket>
#include <QTimer>

class TelnetChannelWorker : public SshChannelWorker
{
    Q_OBJECT

public:
    explicit TelnetChannelWorker(const ConnectionProfile &profile, QObject *parent = nullptr);
    ~TelnetChannelWorker() override;

public slots:
    void start() override;
    void stop() override;
    void sendInput(const QByteArray &data) override;
    void resizePty(int cols, int rows) override;

private slots:
    void handleConnected();
    void handleReadyRead();
    void handleDisconnected();
    void handleSocketError(QAbstractSocket::SocketError error);
    void handleConnectTimeout();

private:
    enum class ParseState {
        Data,
        Command,
        Option,
        Subnegotiation,
        SubnegotiationIac,
    };

    void resetParser();
    void processIncoming(const QByteArray &bytes);
    void handleOption(unsigned char command, unsigned char option);
    void handleSubnegotiation(const QByteArray &payload);
    void sendCommand(unsigned char command, unsigned char option);
    void sendSubnegotiation(const QByteArray &payload);
    void sendWindowSize();
    void handlePlainTextForLogin(const QByteArray &plain);
    void sendLoginLine(const QString &text);
    void finishDisconnected(const QString &reason);
    void cleanupSocket();
    int port() const;

    QTcpSocket *m_socket = nullptr;
    QTimer *m_connectTimer = nullptr;
    ParseState m_parseState = ParseState::Data;
    unsigned char m_pendingCommand = 0;
    QByteArray m_subnegotiation;
    int m_cols = 80;
    int m_rows = 24;
    bool m_running = false;
    bool m_stopping = false;
    bool m_nawsEnabled = false;
    bool m_disconnectedEmitted = false;
    bool m_usernameSent = false;
    bool m_passwordSent = false;
    QByteArray m_loginPromptBuffer;
};
