#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "EchoChannelWorker.h"
#include "terminal/VtScreen.h"

namespace {

ConnectionProfile makeProfile(const QString &name = QStringLiteral("Echo"))
{
    ConnectionProfile p;
    p.id = QStringLiteral("conn-fixture");
    p.name = name;
    p.protocol = QStringLiteral("ssh");
    p.host = QStringLiteral("localhost");
    p.port = 22;
    p.username = QStringLiteral("tester");
    p.authType = QStringLiteral("password");
    // Fixture tests assert the disconnect → status transition; let the test
    // see the terminal state rather than entering the reconnect backoff loop.
    p.autoReconnect = false;
    return p;
}

// 全部用例统一注入 Echo 工厂，避开真实 libssh2 连接。
void installEchoFactory(SessionController &controller)
{
    controller.setWorkerFactory([](const ConnectionProfile &p) -> SshChannelWorker * {
        return new EchoChannelWorker(p);
    });
}

class SlowStartWorker final : public SshChannelWorker
{
    Q_OBJECT

public:
    explicit SlowStartWorker(const ConnectionProfile &profile)
        : SshChannelWorker(profile)
    {
    }

public slots:
    void start() override
    {
        QThread::msleep(500);
        emit disconnected(QStringLiteral("Slow start finished."));
    }

    void stop() override
    {
        emit disconnected(QStringLiteral("Slow start cancelled."));
    }

    void sendInput(const QByteArray &) override {}
};

void installSlowStartFactory(SessionController &controller)
{
    controller.setWorkerFactory([](const ConnectionProfile &p) -> SshChannelWorker * {
        return new SlowStartWorker(p);
    });
}

// 等待信号至少触发一次（默认 2s），或累计触发 minCount 次。
bool waitForCount(QSignalSpy &spy, int minCount, int timeoutMs = 2000)
{
    QElapsedTimer t;
    t.start();
    while (spy.count() < minCount && t.elapsed() < timeoutMs) {
        spy.wait(100);
    }
    return spy.count() >= minCount;
}

// 在 buffer() 里等到出现某段文字（带超时）。
bool waitForBuffer(SessionController &controller, const QString &id,
                   const QString &needle, int timeoutMs = 2000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (controller.sessionBuffer(id).contains(needle)) {
            return true;
        }
        QTest::qWait(50);
    }
    return controller.sessionBuffer(id).contains(needle);
}

} // namespace

class TestSessionController : public QObject
{
    Q_OBJECT

private slots:
    void rejectsEmptyConnectionId();
    void openProducesIdAndStreamsBanner();
    void inputIsEchoedBack();
    void closeReleasesSession();
    void closeDuringConnectReturnsImmediately();
    void exitCommandTransitionsToDisconnected();
    void clearScreenKeepsPromptLine();
    void redrawingDoesNotGrowBufferUnbounded();
};

void TestSessionController::rejectsEmptyConnectionId()
{
    SessionController controller;
    ConnectionProfile empty;
    QString error;
    const QString id = controller.open(empty, &error);
    QVERIFY(id.isEmpty());
    QVERIFY(!error.isEmpty());
    QCOMPARE(controller.sessionsAsVariantList().size(), 0);
}

void TestSessionController::openProducesIdAndStreamsBanner()
{
    SessionController controller;
    installEchoFactory(controller);
    QSignalSpy updateSpy(&controller, &SessionController::sessionScreenUpdated);

    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));
    QVERIFY(controller.contains(id));

    const QVariantList listed = controller.sessionsAsVariantList();
    QCOMPARE(listed.size(), 1);
    const QVariantMap row = listed.first().toMap();
    QCOMPARE(row.value(QStringLiteral("id")).toString(), id);
    QCOMPARE(row.value(QStringLiteral("connectionId")).toString(),
             QStringLiteral("conn-fixture"));

    QVERIFY2(waitForCount(updateSpy, 1),
             "VtScreen should fire screen updates as banner is fed in");

    QVERIFY2(waitForBuffer(controller, id, QStringLiteral("OpenShell echo backend")),
             qPrintable(QStringLiteral("buffer was: %1").arg(controller.sessionBuffer(id))));
}

void TestSessionController::inputIsEchoedBack()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    QVERIFY(waitForBuffer(controller, id, QStringLiteral("OpenShell echo backend")));

    controller.sendInput(id, QByteArrayLiteral("hello\n"));

    QVERIFY2(waitForBuffer(controller, id, QStringLiteral("echo: hello")),
             qPrintable(QStringLiteral("buffer was: %1").arg(controller.sessionBuffer(id))));
}

void TestSessionController::closeReleasesSession()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY(!id.isEmpty());
    QVERIFY(controller.contains(id));

    QSignalSpy changedSpy(&controller, &SessionController::sessionsChanged);
    controller.close(id);
    QVERIFY(!controller.contains(id));
    QCOMPARE(controller.sessionsAsVariantList().size(), 0);
    QVERIFY(changedSpy.count() >= 1);
}

void TestSessionController::closeDuringConnectReturnsImmediately()
{
    SessionController controller;
    installSlowStartFactory(controller);

    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));
    QVERIFY(controller.contains(id));

    QElapsedTimer timer;
    timer.start();
    controller.close(id);

    QVERIFY2(timer.elapsed() < 100,
             qPrintable(QStringLiteral("close() blocked for %1 ms").arg(timer.elapsed())));
    QVERIFY(!controller.contains(id));

    QTest::qWait(700);
}

void TestSessionController::exitCommandTransitionsToDisconnected()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY(!id.isEmpty());

    QSignalSpy statusSpy(&controller, &SessionController::sessionStatusChanged);

    waitForCount(statusSpy, 1);
    statusSpy.clear();

    controller.sendInput(id, QByteArrayLiteral("exit\n"));

    QVERIFY(waitForCount(statusSpy, 1, 3000));
    bool sawDisconnected = false;
    for (const QList<QVariant> &args : statusSpy) {
        if (args.value(0).toString() == id
                && args.value(1).toString() == QStringLiteral("disconnected")) {
            sawDisconnected = true;
            break;
        }
    }
    QVERIFY(sawDisconnected);
    QVERIFY2(waitForBuffer(controller, id, QStringLiteral("Connection disconnected")),
             qPrintable(QStringLiteral("buffer was: %1").arg(controller.sessionBuffer(id))));
}

void TestSessionController::clearScreenKeepsPromptLine()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY(!id.isEmpty());

    QVERIFY(waitForBuffer(controller, id, QStringLiteral("OpenShell echo backend")));
    QVERIFY(waitForBuffer(controller, id, QStringLiteral("tester@localhost:~$")));

    controller.clearBuffer(id);
    QTest::qWait(50);

    const QString buffer = controller.sessionBuffer(id);
    QVERIFY(buffer.contains(QStringLiteral("tester@localhost:~$")));

    // 清屏后 banner 不应再出现在屏幕快照里
    QVERIFY(!controller.sessionBuffer(id).contains(QStringLiteral("OpenShell echo backend")));
}

void TestSessionController::redrawingDoesNotGrowBufferUnbounded()
{
    // 模拟 top/less 这类全屏程序：反复"清屏+重画"应该让屏幕快照尺寸保持恒定。
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY(!id.isEmpty());

    auto *screen = qobject_cast<VtScreen *>(controller.sessionScreen(id));
    QVERIFY(screen);

    screen->resize(80, 24);

    const QByteArray frame = QByteArrayLiteral(
        "\x1b[2J\x1b[H"  // clear + home
        "redraw frame\r\n"
        "second line\r\n");

    QString initial;
    QString later;
    for (int i = 0; i < 200; ++i) {
        screen->feed(frame);
        if (i == 5) {
            initial = controller.sessionBuffer(id);
        }
        if (i == 199) {
            later = controller.sessionBuffer(id);
        }
    }

    QVERIFY(later.contains(QStringLiteral("redraw frame")));
    QVERIFY(later.contains(QStringLiteral("second line")));
    // 屏幕快照长度由 cols*rows 决定，不会随帧数线性增长
    QCOMPARE(initial.size(), later.size());
}

QTEST_GUILESS_MAIN(TestSessionController)
#include "test_session_controller.moc"
