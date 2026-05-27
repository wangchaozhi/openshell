#pragma once

#include "ConnectionCatalog.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantMap>

class SystemMonitorController : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitorController(QObject *parent = nullptr);

    QString requestSnapshot(const QString &connectionId, const ConnectionProfile &profile);

signals:
    void snapshotReady(const QString &requestId,
                       const QString &connectionId,
                       const QVariantMap &snapshot,
                       const QString &error);

private:
    struct CpuTicks
    {
        double user = 0;
        double nice = 0;
        double system = 0;
        double idle = 0;
        double iowait = 0;
        double irq = 0;
        double softirq = 0;
        double steal = 0;
        double total = 0;
        bool valid = false;
    };

    QVariantMap normalizeSnapshot(const QString &connectionId, const QVariantMap &snapshot);

    QHash<QString, CpuTicks> m_lastCpuTicksByConnection;
    // 静态机器信息按 connectionId 缓存（os/kernel/hostname/cpu 型号/cpuDetails
    // 等开机不会变的字段）。第一次抓快照时用 full 命令，之后只跑动态部分，
    // 静态字段直接从这里 merge 回去。
    QHash<QString, QVariantMap> m_staticInfoByConnection;
};
