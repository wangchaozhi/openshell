#include "EchoChannelWorker.h"

#include <QDateTime>

EchoChannelWorker::EchoChannelWorker(const ConnectionProfile &profile, QObject *parent)
    : SshChannelWorker(profile, parent)
{
}

void EchoChannelWorker::start()
{
    if (m_running) {
        return;
    }
    m_running = true;

    const QString banner = QStringLiteral(
            "OpenShell echo backend (offline)\r\n"
            "Target: %1@%2:%3 (%4)\r\n"
            "Started: %5\r\n"
            "Type 'exit' to close. Real SSH backend not wired yet.\r\n")
            .arg(m_profile.username.isEmpty() ? QStringLiteral("user") : m_profile.username,
                 m_profile.host.isEmpty() ? QStringLiteral("localhost") : m_profile.host,
                 QString::number(m_profile.port > 0 ? m_profile.port : 22),
                 m_profile.protocol.isEmpty() ? QStringLiteral("ssh") : m_profile.protocol,
                 QDateTime::currentDateTime().toString(Qt::ISODate));

    emit connected();
    emit output(banner.toUtf8());
    emitPrompt();
}

void EchoChannelWorker::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    emit disconnected(tr("Echo backend stopped."));
}

void EchoChannelWorker::sendInput(const QByteArray &data)
{
    if (!m_running) {
        return;
    }

    emit output(data);
    m_lineBuffer.append(data);

    while (true) {
        const int newlineIndex = m_lineBuffer.indexOf('\n');
        if (newlineIndex < 0) {
            break;
        }
        QByteArray line = m_lineBuffer.left(newlineIndex);
        m_lineBuffer.remove(0, newlineIndex + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        const QString trimmed = QString::fromUtf8(line).trimmed();
        if (trimmed.compare(QStringLiteral("exit"), Qt::CaseInsensitive) == 0) {
            emit output(QByteArrayLiteral("Goodbye.\r\n"));
            stop();
            return;
        }

        if (!trimmed.isEmpty()) {
            const QString reply = QStringLiteral("echo: %1\r\n").arg(trimmed);
            emit output(reply.toUtf8());
        }
        emitPrompt();
    }
}

void EchoChannelWorker::emitPrompt()
{
    const QString prompt = QStringLiteral("%1@%2:~$ ")
            .arg(m_profile.username.isEmpty() ? QStringLiteral("user") : m_profile.username,
                 m_profile.host.isEmpty() ? QStringLiteral("localhost") : m_profile.host);
    emit output(prompt.toUtf8());
}
