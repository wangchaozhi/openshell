#pragma once

#include "ConnectionCatalog.h"

#include <QDeadlineTimer>
#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantMap>

class SystemMonitorController : public QObject
{
    Q_OBJECT

public:
    explicit SystemMonitorController(QObject *parent = nullptr);

    QString requestSnapshot(const QString &connectionId, const ConnectionProfile &profile,
                            bool includeDetails);

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

    struct ConnectionSamples
    {
        QString requestId;
        CpuTicks previousCpu;
        QVariantMap staticInfo;
        bool hasDetailedStaticInfo = false;
        QVariantList processes;
        QVariantList filesystems;
        QDeadlineTimer nextProcessSample;
        QDeadlineTimer nextFilesystemSample;
    };

    static QVariantMap normalizeSnapshot(CpuTicks &previous, const QVariantMap &snapshot);

    QHash<QString, ConnectionSamples> m_samplesByConnection;
};
