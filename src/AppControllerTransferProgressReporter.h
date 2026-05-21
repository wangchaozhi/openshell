#pragma once

#include "AppController.h"

#include <QElapsedTimer>
#include <QMetaObject>
#include <QString>

#include <utility>

class AppControllerTransferProgressReporter
{
public:
    AppControllerTransferProgressReporter(AppController *controller,
                                          QString requestId,
                                          QString connectionId,
                                          QString operation,
                                          QString path)
        : m_controller(controller)
        , m_requestId(std::move(requestId))
        , m_connectionId(std::move(connectionId))
        , m_operation(std::move(operation))
        , m_path(std::move(path))
    {
        m_timer.start();
    }

    void report(qint64 bytesDone, qint64 bytesTotal, bool force = false)
    {
        const qint64 elapsed = qMax<qint64>(1, m_timer.elapsed());
        if (!force && bytesDone != 0 && bytesDone != bytesTotal && elapsed - m_lastEmitMs < 200) {
            return;
        }
        m_lastEmitMs = elapsed;
        const double speed = elapsed > 0
                                 ? (static_cast<double>(bytesDone) * 1000.0 / static_cast<double>(elapsed))
                                 : 0.0;
        QMetaObject::invokeMethod(m_controller,
                                  [controller = m_controller,
                                   requestId = m_requestId,
                                   connectionId = m_connectionId,
                                   operation = m_operation,
                                   path = m_path,
                                   bytesDone,
                                   bytesTotal,
                                   speed]() {
                                      emit controller->remoteOperationProgress(requestId,
                                                                               connectionId,
                                                                               operation,
                                                                               path,
                                                                               bytesDone,
                                                                               bytesTotal,
                                                                               speed);
                                  },
                                  Qt::QueuedConnection);
    }

private:
    AppController *m_controller = nullptr;
    QString m_requestId;
    QString m_connectionId;
    QString m_operation;
    QString m_path;
    QElapsedTimer m_timer;
    qint64 m_lastEmitMs = -1000;
};
