#include "SystemMonitorController.h"

#include "ssh/SftpDirectoryLister.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>
#include <QtConcurrent>

namespace {
QString monitorCommand()
{
    return QStringLiteral(R"SH(
sh -c '
echo "__INFO__"
if [ -r /etc/os-release ]; then . /etc/os-release; echo "os=${PRETTY_NAME:-$NAME}"; else echo "os=$(uname -s)"; fi
echo "kernel=$(uname -s)"
echo "kernel_release=$(uname -r)"
echo "arch=$(uname -m)"
echo "hostname=$(hostname)"
echo "uptime=$(uptime -p 2>/dev/null || uptime)"
echo "__CPUINFO__"
awk -F: "/model name|Hardware|Processor/ {gsub(/^[ \t]+/,\"\",\$2); print \"model=\" \$2; exit}" /proc/cpuinfo 2>/dev/null
echo "cores=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)"
awk "/^cpu / {usr=\$2; nice=\$3; sys=\$4; idle=\$5; iowait=\$6; irq=\$7; softirq=\$8; steal=\$9; total=usr+nice+sys+idle+iowait+irq+softirq+steal; printf \"stat=%s %s %s %s %s %s %s %s %s\n\", usr,nice,sys,idle,iowait,irq,softirq,steal,total}" /proc/stat 2>/dev/null
echo "__CPUS__"
awk -F: "/^processor/ {if (seen) {printf \"cpu=%s|%s|%s|%s|%s\n\", model, cores, mhz, cache, bogo} seen=1; model=\"\"; cores=\"1\"; mhz=\"\"; cache=\"\"; bogo=\"\"} /^model name|^Hardware|^Processor/ {gsub(/^[ \t]+/,\"\",\$2); model=\$2} /^cpu cores/ {gsub(/^[ \t]+/,\"\",\$2); cores=\$2} /^cpu MHz/ {gsub(/^[ \t]+/,\"\",\$2); mhz=\$2} /^cache size/ {gsub(/^[ \t]+/,\"\",\$2); cache=\$2} /^bogomips|^BogoMIPS/ {gsub(/^[ \t]+/,\"\",\$2); bogo=\$2} END {if (seen) printf \"cpu=%s|%s|%s|%s|%s\n\", model, cores, mhz, cache, bogo}" /proc/cpuinfo 2>/dev/null
echo "__MEM__"
awk "/MemTotal|MemAvailable|SwapTotal|SwapFree/ {print \$1 \"=\" \$2}" /proc/meminfo 2>/dev/null
echo "__NET__"
cat /proc/net/dev 2>/dev/null | tail -n +3
echo "__DF__"
df -P -B1 2>/dev/null | tail -n +2
echo "__PS__"
ps -eo pid,comm,pcpu,pmem,rss --sort=-pcpu 2>/dev/null | head -n 8
'
)SH");
}

QString humanBytes(double bytes)
{
    static const QStringList units{QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB"), QStringLiteral("PB")};
    int unit = 0;
    while (bytes >= 1024.0 && unit + 1 < units.size()) {
        bytes /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(bytes, 0, bytes >= 10.0 || unit == 0 ? 'f' : 'f', unit == 0 ? 0 : 1)
        .arg(units.at(unit));
}

QVariantMap parseSystemMonitorOutput(const QString &output)
{
    QVariantMap snapshot;
    QVariantMap info;
    QVariantMap cpu;
    QVariantList cpuDetails;
    QVariantMap memory;
    QVariantList networks;
    QVariantList filesystems;
    QVariantList processes;
    QString section;

    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QStringLiteral("__")) && line.endsWith(QStringLiteral("__"))) {
            section = line;
            continue;
        }
        if (section == QStringLiteral("__INFO__") || section == QStringLiteral("__CPUINFO__")) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                continue;
            }
            const QString key = line.left(eq);
            const QString value = line.mid(eq + 1);
            if (section == QStringLiteral("__INFO__")) {
                info.insert(key, value);
            } else if (key == QStringLiteral("stat")) {
                const QStringList parts = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() >= 9) {
                    const double user = parts.at(0).toDouble();
                    const double nice = parts.at(1).toDouble();
                    const double system = parts.at(2).toDouble();
                    const double idle = parts.at(3).toDouble();
                    const double iowait = parts.at(4).toDouble();
                    const double irq = parts.at(5).toDouble();
                    const double softirq = parts.at(6).toDouble();
                    const double steal = parts.at(7).toDouble();
                    const double total = qMax(1.0, parts.at(8).toDouble());
                    const double busy = total - idle - iowait;
                    cpu.insert(QStringLiteral("userPercent"), user * 100.0 / total);
                    cpu.insert(QStringLiteral("systemPercent"), system * 100.0 / total);
                    cpu.insert(QStringLiteral("nicePercent"), nice * 100.0 / total);
                    cpu.insert(QStringLiteral("idlePercent"), idle * 100.0 / total);
                    cpu.insert(QStringLiteral("ioPercent"), iowait * 100.0 / total);
                    cpu.insert(QStringLiteral("irqPercent"), irq * 100.0 / total);
                    cpu.insert(QStringLiteral("softirqPercent"), softirq * 100.0 / total);
                    cpu.insert(QStringLiteral("stealPercent"), steal * 100.0 / total);
                    cpu.insert(QStringLiteral("busyPercent"), busy * 100.0 / total);
                    cpu.insert(QStringLiteral("userTicks"), user);
                    cpu.insert(QStringLiteral("niceTicks"), nice);
                    cpu.insert(QStringLiteral("systemTicks"), system);
                    cpu.insert(QStringLiteral("idleTicks"), idle);
                    cpu.insert(QStringLiteral("iowaitTicks"), iowait);
                    cpu.insert(QStringLiteral("irqTicks"), irq);
                    cpu.insert(QStringLiteral("softirqTicks"), softirq);
                    cpu.insert(QStringLiteral("stealTicks"), steal);
                    cpu.insert(QStringLiteral("totalTicks"), total);
                }
            } else {
                cpu.insert(key, value);
            }
            continue;
        }
        if (section == QStringLiteral("__CPUS__")) {
            if (line.startsWith(QStringLiteral("cpu="))) {
                const QStringList parts = line.mid(4).split(QLatin1Char('|'));
                QVariantMap row;
                row.insert(QStringLiteral("name"), parts.value(0).isEmpty() ? cpu.value(QStringLiteral("model")).toString() : parts.value(0));
                row.insert(QStringLiteral("cores"), parts.value(1, QStringLiteral("1")));
                row.insert(QStringLiteral("frequency"), parts.value(2).isEmpty() ? QStringLiteral("--") : QStringLiteral("%1 MHz").arg(parts.value(2)));
                row.insert(QStringLiteral("cache"), parts.value(3).isEmpty() ? QStringLiteral("--") : parts.value(3));
                row.insert(QStringLiteral("bogomips"), parts.value(4).isEmpty() ? QStringLiteral("--") : parts.value(4));
                cpuDetails.append(row);
            }
            continue;
        }
        if (section == QStringLiteral("__MEM__")) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0) {
                memory.insert(line.left(eq).remove(QLatin1Char(':')), line.mid(eq + 1).toLongLong() * 1024);
            }
            continue;
        }
        if (section == QStringLiteral("__NET__")) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                const QString name = line.left(colon).trimmed();
                const QStringList parts = line.mid(colon + 1).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (parts.size() >= 16) {
                    QVariantMap row;
                    row.insert(QStringLiteral("name"), name);
                    row.insert(QStringLiteral("rxBytes"), parts.at(0).toLongLong());
                    row.insert(QStringLiteral("txBytes"), parts.at(8).toLongLong());
                    row.insert(QStringLiteral("rx"), humanBytes(parts.at(0).toDouble()));
                    row.insert(QStringLiteral("tx"), humanBytes(parts.at(8).toDouble()));
                    networks.append(row);
                }
            }
            continue;
        }
        if (section == QStringLiteral("__DF__")) {
            const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                const qint64 size = parts.at(1).toLongLong();
                const qint64 used = parts.at(2).toLongLong();
                const qint64 available = parts.at(3).toLongLong();
                QVariantMap row;
                row.insert(QStringLiteral("name"), parts.at(0));
                row.insert(QStringLiteral("size"), humanBytes(size));
                row.insert(QStringLiteral("used"), humanBytes(used));
                row.insert(QStringLiteral("available"), humanBytes(available));
                row.insert(QStringLiteral("usedPercent"), size > 0 ? used * 100.0 / size : 0.0);
                row.insert(QStringLiteral("mount"), parts.mid(5).join(QLatin1Char(' ')));
                filesystems.append(row);
            }
            continue;
        }
        if (section == QStringLiteral("__PS__")) {
            if (line.startsWith(QStringLiteral("PID"))) {
                continue;
            }
            const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                QVariantMap row;
                row.insert(QStringLiteral("pid"), parts.at(0));
                row.insert(QStringLiteral("name"), parts.at(1));
                row.insert(QStringLiteral("cpu"), parts.at(2).toDouble());
                row.insert(QStringLiteral("memory"), parts.at(3).toDouble());
                row.insert(QStringLiteral("rss"), humanBytes(parts.at(4).toDouble() * 1024.0));
                processes.append(row);
            }
        }
    }

    const qint64 memTotal = memory.value(QStringLiteral("MemTotal")).toLongLong();
    const qint64 memAvailable = memory.value(QStringLiteral("MemAvailable")).toLongLong();
    const qint64 swapTotal = memory.value(QStringLiteral("SwapTotal")).toLongLong();
    const qint64 swapFree = memory.value(QStringLiteral("SwapFree")).toLongLong();
    memory.insert(QStringLiteral("memUsed"), qMax<qint64>(0, memTotal - memAvailable));
    memory.insert(QStringLiteral("memUsedPercent"), memTotal > 0 ? (memTotal - memAvailable) * 100.0 / memTotal : 0.0);
    memory.insert(QStringLiteral("swapUsed"), qMax<qint64>(0, swapTotal - swapFree));
    memory.insert(QStringLiteral("swapUsedPercent"), swapTotal > 0 ? (swapTotal - swapFree) * 100.0 / swapTotal : 0.0);
    memory.insert(QStringLiteral("memTotalText"), humanBytes(memTotal));
    memory.insert(QStringLiteral("memUsedText"), humanBytes(memory.value(QStringLiteral("memUsed")).toDouble()));
    memory.insert(QStringLiteral("memAvailableText"), humanBytes(memAvailable));
    memory.insert(QStringLiteral("swapTotalText"), humanBytes(swapTotal));
    memory.insert(QStringLiteral("swapUsedText"), humanBytes(memory.value(QStringLiteral("swapUsed")).toDouble()));

    snapshot.insert(QStringLiteral("info"), info);
    snapshot.insert(QStringLiteral("cpu"), cpu);
    snapshot.insert(QStringLiteral("cpuDetails"), cpuDetails);
    snapshot.insert(QStringLiteral("memory"), memory);
    snapshot.insert(QStringLiteral("networks"), networks);
    snapshot.insert(QStringLiteral("filesystems"), filesystems);
    snapshot.insert(QStringLiteral("processes"), processes);
    snapshot.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    return snapshot;
}
}

SystemMonitorController::SystemMonitorController(QObject *parent)
    : QObject(parent)
{
}

QString SystemMonitorController::requestSnapshot(const QString &connectionId, const ConnectionProfile &profile)
{
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this,
            [this, watcher, requestId, connectionId]() {
                const QVariantMap result = watcher->result();
                const QString error = result.value(QStringLiteral("error")).toString();
                const QVariantMap snapshot = normalizeSnapshot(connectionId,
                                                               result.value(QStringLiteral("snapshot")).toMap());
                emit snapshotReady(requestId, connectionId, snapshot, error);
                watcher->deleteLater();
            });
    watcher->setFuture(QtConcurrent::run([profile]() {
        QVariantMap result;
        if (profile.id.isEmpty()) {
            result.insert(QStringLiteral("error"), QObject::tr("Unknown connection"));
            return result;
        }
        QString error;
        const QString output = SftpDirectoryLister::execute(profile, monitorCommand(), &error);
        if (!error.isEmpty()) {
            result.insert(QStringLiteral("error"), error);
            return result;
        }
        result.insert(QStringLiteral("snapshot"), parseSystemMonitorOutput(output));
        result.insert(QStringLiteral("error"), QString());
        return result;
    }));
    return requestId;
}

QVariantMap SystemMonitorController::normalizeSnapshot(const QString &connectionId,
                                                       const QVariantMap &snapshot)
{
    QVariantMap normalized = snapshot;
    QVariantMap cpu = normalized.value(QStringLiteral("cpu")).toMap();
    CpuTicks current;
    current.user = cpu.value(QStringLiteral("userTicks")).toDouble();
    current.nice = cpu.value(QStringLiteral("niceTicks")).toDouble();
    current.system = cpu.value(QStringLiteral("systemTicks")).toDouble();
    current.idle = cpu.value(QStringLiteral("idleTicks")).toDouble();
    current.iowait = cpu.value(QStringLiteral("iowaitTicks")).toDouble();
    current.irq = cpu.value(QStringLiteral("irqTicks")).toDouble();
    current.softirq = cpu.value(QStringLiteral("softirqTicks")).toDouble();
    current.steal = cpu.value(QStringLiteral("stealTicks")).toDouble();
    current.total = cpu.value(QStringLiteral("totalTicks")).toDouble();
    current.valid = current.total > 0;

    const CpuTicks previous = m_lastCpuTicksByConnection.value(connectionId);
    if (current.valid && previous.valid && current.total > previous.total) {
        const double totalDelta = qMax(1.0, current.total - previous.total);
        const double userDelta = current.user - previous.user;
        const double niceDelta = current.nice - previous.nice;
        const double systemDelta = current.system - previous.system;
        const double idleDelta = current.idle - previous.idle;
        const double iowaitDelta = current.iowait - previous.iowait;
        const double irqDelta = current.irq - previous.irq;
        const double softirqDelta = current.softirq - previous.softirq;
        const double stealDelta = current.steal - previous.steal;
        const double busyDelta = qMax(0.0, totalDelta - idleDelta - iowaitDelta);
        cpu.insert(QStringLiteral("userPercent"), userDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("systemPercent"), systemDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("nicePercent"), niceDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("idlePercent"), idleDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("ioPercent"), iowaitDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("irqPercent"), irqDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("softirqPercent"), softirqDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("stealPercent"), stealDelta * 100.0 / totalDelta);
        cpu.insert(QStringLiteral("busyPercent"), busyDelta * 100.0 / totalDelta);
    }
    if (current.valid) {
        m_lastCpuTicksByConnection.insert(connectionId, current);
    }
    normalized.insert(QStringLiteral("cpu"), cpu);
    return normalized;
}
