#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "ConnectionCatalog.h"
#include "SessionController.h"
#include "EchoChannelWorker.h"

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
    return p;
}

// 全部用例统一注入 Echo 工厂，避开真实 libssh2 连接。
void installEchoFactory(SessionController &controller)
{
    controller.setWorkerFactory([](const ConnectionProfile &p) -> SshChannelWorker * {
        return new EchoChannelWorker(p);
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

} // namespace

class TestSessionController : public QObject
{
    Q_OBJECT

private slots:
    void rejectsEmptyConnectionId();
    void openProducesIdAndStreamsBanner();
    void inputIsEchoedBack();
    void closeReleasesSession();
    void exitCommandTransitionsToDisconnected();
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
    QSignalSpy outputSpy(&controller, &SessionController::sessionOutput);

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

    QVERIFY2(waitForCount(outputSpy, 1), "Echo worker should emit a banner chunk");

    bool sawSelf = false;
    for (const QList<QVariant> &args : outputSpy) {
        if (args.value(0).toString() == id) {
            sawSelf = true;
            break;
        }
    }
    QVERIFY(sawSelf);

    const QString buffered = controller.sessionBuffer(id);
    QVERIFY(buffered.contains(QStringLiteral("OpenShell echo backend")));
}

void TestSessionController::inputIsEchoedBack()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    QSignalSpy outputSpy(&controller, &SessionController::sessionOutput);
    QVERIFY(waitForCount(outputSpy, 1)); // banner first
    outputSpy.clear();

    controller.sendInput(id, QByteArrayLiteral("hello\n"));

    // 等到累积缓冲里看到 echo: hello。
    QElapsedTimer t;
    t.start();
    QString buffer;
    while (t.elapsed() < 2000) {
        buffer = controller.sessionBuffer(id);
        if (buffer.contains(QStringLiteral("echo: hello"))) {
            break;
        }
        QTest::qWait(50);
    }
    QVERIFY2(buffer.contains(QStringLiteral("echo: hello")),
             qPrintable(QStringLiteral("buffer was: %1").arg(buffer)));
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

void TestSessionController::exitCommandTransitionsToDisconnected()
{
    SessionController controller;
    installEchoFactory(controller);
    QString error;
    const QString id = controller.open(makeProfile(), &error);
    QVERIFY(!id.isEmpty());

    QSignalSpy statusSpy(&controller, &SessionController::sessionStatusChanged);

    // 先吃掉初始 connecting->connected 的事件，再发 exit。
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
}

QTEST_GUILESS_MAIN(TestSessionController)
#include "test_session_controller.moc"
