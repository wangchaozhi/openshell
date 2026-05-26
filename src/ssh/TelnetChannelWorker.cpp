#include "TelnetChannelWorker.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QtGlobal>

namespace {
constexpr unsigned char kSe = 240;
constexpr unsigned char kSb = 250;
constexpr unsigned char kWill = 251;
constexpr unsigned char kWont = 252;
constexpr unsigned char kDo = 253;
constexpr unsigned char kDont = 254;
constexpr unsigned char kIac = 255;

constexpr unsigned char kOptBinary = 0;
constexpr unsigned char kOptEcho = 1;
constexpr unsigned char kOptSuppressGoAhead = 3;
constexpr unsigned char kOptTerminalType = 24;
constexpr unsigned char kOptNaws = 31;

constexpr unsigned char kTerminalTypeIs = 0;
constexpr unsigned char kTerminalTypeSend = 1;
}

TelnetChannelWorker::TelnetChannelWorker(const ConnectionProfile &profile, QObject *parent)
    : SshChannelWorker(profile, parent)
{
}

TelnetChannelWorker::~TelnetChannelWorker()
{
    cleanupSocket();
}

void TelnetChannelWorker::start()
{
    if (m_socket) {
        return;
    }

    m_running = false;
    m_stopping = false;
    m_nawsEnabled = false;
    m_disconnectedEmitted = false;
    m_usernameSent = false;
    m_passwordSent = false;
    m_loginPromptBuffer.clear();
    resetParser();

    if (!m_connectTimer) {
        m_connectTimer = new QTimer(this);
        m_connectTimer->setSingleShot(true);
        connect(m_connectTimer, &QTimer::timeout,
                this, &TelnetChannelWorker::handleConnectTimeout);
    }

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,
            this, &TelnetChannelWorker::handleConnected);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &TelnetChannelWorker::handleReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &TelnetChannelWorker::handleDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &TelnetChannelWorker::handleSocketError);

    m_connectTimer->start(m_profile.connectTimeoutSec > 0
                              ? m_profile.connectTimeoutSec * 1000
                              : 10000);
    m_socket->connectToHost(m_profile.host, port());
}

void TelnetChannelWorker::stop()
{
    m_stopping = true;
    if (m_connectTimer) {
        m_connectTimer->stop();
    }
    if (!m_socket) {
        finishDisconnected(tr("Disconnected"));
        return;
    }

    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        finishDisconnected(tr("Disconnected"));
        return;
    }

    m_socket->disconnectFromHost();
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    QTimer::singleShot(0, this, [this]() {
        if (m_socket) {
            finishDisconnected(tr("Disconnected"));
        }
    });
}

void TelnetChannelWorker::sendInput(const QByteArray &data)
{
    if (!m_socket || !m_running || data.isEmpty()) {
        return;
    }

    QByteArray escaped;
    escaped.reserve(data.size());
    for (char c : data) {
        escaped.append(c);
        if (static_cast<unsigned char>(c) == kIac) {
            escaped.append(static_cast<char>(kIac));
        }
    }
    m_socket->write(escaped);
}

void TelnetChannelWorker::resizePty(int cols, int rows)
{
    if (cols <= 0 || rows <= 0) {
        return;
    }
    m_cols = cols;
    m_rows = rows;
    sendWindowSize();
}

void TelnetChannelWorker::handleConnected()
{
    if (m_connectTimer) {
        m_connectTimer->stop();
    }
    m_running = true;
    emit connected();
    sendWindowSize();
}

void TelnetChannelWorker::handleReadyRead()
{
    if (!m_socket) {
        return;
    }
    processIncoming(m_socket->readAll());
}

void TelnetChannelWorker::handleDisconnected()
{
    finishDisconnected(m_stopping ? tr("Disconnected") : tr("Telnet connection closed"));
}

void TelnetChannelWorker::handleSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_stopping || !m_socket) {
        return;
    }
    const QString message = m_socket->errorString();
    emit errorOccurred(message);
    finishDisconnected(message);
}

void TelnetChannelWorker::handleConnectTimeout()
{
    const QString message = tr("connect() to %1:%2 timed out after %3 seconds")
                                .arg(m_profile.host,
                                     QString::number(port()),
                                     QString::number(m_profile.connectTimeoutSec > 0
                                                         ? m_profile.connectTimeoutSec
                                                         : 10));
    emit errorOccurred(message);
    if (m_socket) {
        m_socket->abort();
    }
    finishDisconnected(message);
}

void TelnetChannelWorker::resetParser()
{
    m_parseState = ParseState::Data;
    m_pendingCommand = 0;
    m_subnegotiation.clear();
}

void TelnetChannelWorker::processIncoming(const QByteArray &bytes)
{
    QByteArray plain;
    plain.reserve(bytes.size());

    for (char raw : bytes) {
        const auto ch = static_cast<unsigned char>(raw);
        switch (m_parseState) {
        case ParseState::Data:
            if (ch == kIac) {
                m_parseState = ParseState::Command;
            } else {
                plain.append(raw);
            }
            break;
        case ParseState::Command:
            if (ch == kIac) {
                plain.append(raw);
                m_parseState = ParseState::Data;
            } else if (ch == kDo || ch == kDont || ch == kWill || ch == kWont) {
                m_pendingCommand = ch;
                m_parseState = ParseState::Option;
            } else if (ch == kSb) {
                m_subnegotiation.clear();
                m_parseState = ParseState::Subnegotiation;
            } else {
                m_parseState = ParseState::Data;
            }
            break;
        case ParseState::Option:
            handleOption(m_pendingCommand, ch);
            m_parseState = ParseState::Data;
            break;
        case ParseState::Subnegotiation:
            if (ch == kIac) {
                m_parseState = ParseState::SubnegotiationIac;
            } else {
                m_subnegotiation.append(raw);
            }
            break;
        case ParseState::SubnegotiationIac:
            if (ch == kSe) {
                handleSubnegotiation(m_subnegotiation);
                m_subnegotiation.clear();
                m_parseState = ParseState::Data;
            } else if (ch == kIac) {
                m_subnegotiation.append(raw);
                m_parseState = ParseState::Subnegotiation;
            } else {
                m_parseState = ParseState::Subnegotiation;
            }
            break;
        }
    }

    if (!plain.isEmpty()) {
        emit output(plain);
        handlePlainTextForLogin(plain);
    }
}

void TelnetChannelWorker::handleOption(unsigned char command, unsigned char option)
{
    switch (command) {
    case kDo:
        if (option == kOptBinary || option == kOptSuppressGoAhead
            || option == kOptTerminalType || option == kOptNaws) {
            sendCommand(kWill, option);
            if (option == kOptNaws) {
                m_nawsEnabled = true;
                sendWindowSize();
            }
        } else {
            sendCommand(kWont, option);
        }
        break;
    case kDont:
        if (option == kOptNaws) {
            m_nawsEnabled = false;
        }
        sendCommand(kWont, option);
        break;
    case kWill:
        if (option == kOptBinary || option == kOptEcho || option == kOptSuppressGoAhead) {
            sendCommand(kDo, option);
        } else {
            sendCommand(kDont, option);
        }
        break;
    case kWont:
        sendCommand(kDont, option);
        break;
    default:
        break;
    }
}

void TelnetChannelWorker::handleSubnegotiation(const QByteArray &payload)
{
    if (payload.size() < 2) {
        return;
    }
    if (static_cast<unsigned char>(payload.at(0)) == kOptTerminalType
        && static_cast<unsigned char>(payload.at(1)) == kTerminalTypeSend) {
        QByteArray response;
        response.append(static_cast<char>(kOptTerminalType));
        response.append(static_cast<char>(kTerminalTypeIs));
        const QString terminalType = m_profile.telnetTerminalType.trimmed().isEmpty()
                                         ? QStringLiteral("xterm-256color")
                                         : m_profile.telnetTerminalType.trimmed();
        response.append(terminalType.toUtf8());
        sendSubnegotiation(response);
    }
}

void TelnetChannelWorker::sendCommand(unsigned char command, unsigned char option)
{
    if (!m_socket) {
        return;
    }
    const char bytes[] = {
        static_cast<char>(kIac),
        static_cast<char>(command),
        static_cast<char>(option),
    };
    m_socket->write(bytes, sizeof(bytes));
}

void TelnetChannelWorker::sendSubnegotiation(const QByteArray &payload)
{
    if (!m_socket) {
        return;
    }
    QByteArray bytes;
    bytes.reserve(payload.size() + 4);
    bytes.append(static_cast<char>(kIac));
    bytes.append(static_cast<char>(kSb));
    for (char c : payload) {
        bytes.append(c);
        if (static_cast<unsigned char>(c) == kIac) {
            bytes.append(static_cast<char>(kIac));
        }
    }
    bytes.append(static_cast<char>(kIac));
    bytes.append(static_cast<char>(kSe));
    m_socket->write(bytes);
}

void TelnetChannelWorker::sendWindowSize()
{
    if (!m_socket || !m_running || !m_nawsEnabled) {
        return;
    }

    const int cols = qBound(1, m_cols, 65535);
    const int rows = qBound(1, m_rows, 65535);
    QByteArray payload;
    payload.append(static_cast<char>(kOptNaws));
    payload.append(static_cast<char>((cols >> 8) & 0xff));
    payload.append(static_cast<char>(cols & 0xff));
    payload.append(static_cast<char>((rows >> 8) & 0xff));
    payload.append(static_cast<char>(rows & 0xff));
    sendSubnegotiation(payload);
}

void TelnetChannelWorker::handlePlainTextForLogin(const QByteArray &plain)
{
    if (plain.isEmpty()
        || !m_profile.telnetAutoLogin
        || (m_usernameSent && m_passwordSent)
        || (!m_socket || !m_running)) {
        return;
    }

    m_loginPromptBuffer.append(plain);
    constexpr qsizetype maxPromptBuffer = 512;
    if (m_loginPromptBuffer.size() > maxPromptBuffer) {
        m_loginPromptBuffer = m_loginPromptBuffer.right(maxPromptBuffer);
    }

    const QString text = QString::fromLatin1(m_loginPromptBuffer).toLower();
    if (!m_usernameSent && !m_profile.username.isEmpty()
        && (text.contains(QStringLiteral("login:"))
            || text.contains(QStringLiteral("username:"))
            || text.contains(QStringLiteral("user name:")))) {
        m_usernameSent = true;
        m_loginPromptBuffer.clear();
        sendLoginLine(m_profile.username);
        return;
    }

    if (!m_passwordSent && !m_profile.password.isEmpty()
        && text.contains(QStringLiteral("password:"))) {
        m_passwordSent = true;
        m_loginPromptBuffer.clear();
        sendLoginLine(m_profile.password);
    }
}

void TelnetChannelWorker::sendLoginLine(const QString &text)
{
    if (!m_socket || !m_running) {
        return;
    }
    QByteArray line = text.toUtf8();
    line.append("\r\n");
    sendInput(line);
}

void TelnetChannelWorker::finishDisconnected(const QString &reason)
{
    if (m_connectTimer) {
        m_connectTimer->stop();
    }
    m_running = false;
    cleanupSocket();
    resetParser();
    m_loginPromptBuffer.clear();

    if (m_disconnectedEmitted) {
        return;
    }
    m_disconnectedEmitted = true;
    emit disconnected(reason);
}

void TelnetChannelWorker::cleanupSocket()
{
    if (!m_socket) {
        return;
    }
    QTcpSocket *socket = m_socket;
    m_socket = nullptr;
    socket->disconnect(this);
    socket->deleteLater();
}

int TelnetChannelWorker::port() const
{
    return m_profile.port > 0 ? m_profile.port : 23;
}
