#include <QtTest>
#include <QCoreApplication>
#include <QHostAddress>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

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

bool waitForBytes(const QByteArray &buffer, const QByteArray &needle, int timeoutMs = 2000)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        if (buffer.contains(needle)) {
            return true;
        }
        QTest::qWait(20);
    }
    return buffer.contains(needle);
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
    void telnetProtocolUsesTcpWorker();
    void telnetUsesSocks5_data();
    void telnetUsesSocks5();
    void telnetSocks5Failure_data();
    void telnetSocks5Failure();
    void telnetAutoLoginRespondsToPrompts();
    void telnetAutoLoginCanBeDisabled();
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

void TestSessionController::telnetProtocolUsesTcpWorker()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QPointer<QTcpSocket> peer;
    QByteArray receivedFromClient;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        peer = server.nextPendingConnection();
        peer->setParent(&server);
        connect(peer, &QTcpSocket::readyRead, &server, [&]() {
            receivedFromClient.append(peer->readAll());
        });
        QByteArray greeting;
        greeting.append(static_cast<char>(255)); // IAC
        greeting.append(static_cast<char>(253)); // DO
        greeting.append(static_cast<char>(24));  // TERMINAL-TYPE
        greeting.append(static_cast<char>(255)); // IAC
        greeting.append(static_cast<char>(253)); // DO
        greeting.append(static_cast<char>(31));  // NAWS
        greeting.append(static_cast<char>(255)); // IAC
        greeting.append(static_cast<char>(251)); // WILL
        greeting.append(static_cast<char>(1));   // ECHO
        greeting.append(QByteArrayLiteral("login: "));
        peer->write(greeting);
        peer->flush();
    });

    ConnectionProfile profile = makeProfile(QStringLiteral("Telnet"));
    profile.protocol = QStringLiteral("telnet");
    profile.host = server.serverAddress().toString();
    profile.port = server.serverPort();
    profile.username.clear();
    profile.telnetTerminalType = QStringLiteral("vt100");

    SessionController controller;
    QString error;
    const QString id = controller.open(profile, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    QVERIFY2(waitForBuffer(controller, id, QStringLiteral("login:")),
             qPrintable(QStringLiteral("buffer was: %1").arg(controller.sessionBuffer(id))));
    QVERIFY(!controller.sessionBuffer(id).contains(QChar(255)));

    const QByteArray willTerminalType = QByteArray()
                                            + static_cast<char>(255)
                                            + static_cast<char>(251)
                                            + static_cast<char>(24);
    const QByteArray willNaws = QByteArray()
                                + static_cast<char>(255)
                                + static_cast<char>(251)
                                + static_cast<char>(31);
    QVERIFY(waitForBytes(receivedFromClient, willTerminalType));
    QVERIFY(waitForBytes(receivedFromClient, willNaws));

    receivedFromClient.clear();
    QVERIFY(peer);
    QByteArray ttypeRequest;
    ttypeRequest.append(static_cast<char>(255)); // IAC
    ttypeRequest.append(static_cast<char>(250)); // SB
    ttypeRequest.append(static_cast<char>(24));  // TERMINAL-TYPE
    ttypeRequest.append(static_cast<char>(1));   // SEND
    ttypeRequest.append(static_cast<char>(255)); // IAC
    ttypeRequest.append(static_cast<char>(240)); // SE
    peer->write(ttypeRequest);
    peer->flush();

    const QByteArray ttypeReply = QByteArray()
                                  + static_cast<char>(255)
                                  + static_cast<char>(250)
                                  + static_cast<char>(24)
                                  + static_cast<char>(0)
                                  + QByteArrayLiteral("vt100")
                                  + static_cast<char>(255)
                                  + static_cast<char>(240);
    QVERIFY(waitForBytes(receivedFromClient, ttypeReply));

    receivedFromClient.clear();
    controller.requestResize(id, 132, 43);
    const QByteArray nawsReply = QByteArray()
                                 + static_cast<char>(255)
                                 + static_cast<char>(250)
                                 + static_cast<char>(31)
                                 + static_cast<char>(0)
                                 + static_cast<char>(132)
                                 + static_cast<char>(0)
                                 + static_cast<char>(43)
                                 + static_cast<char>(255)
                                 + static_cast<char>(240);
    QVERIFY(waitForBytes(receivedFromClient, nawsReply));

    receivedFromClient.clear();
    controller.sendInput(id, QByteArray(1, static_cast<char>(255)));
    const QByteArray escapedIac = QByteArray()
                                  + static_cast<char>(255)
                                  + static_cast<char>(255);
    QVERIFY(waitForBytes(receivedFromClient, escapedIac));

    controller.close(id);
    QTest::qWait(100);
}

void TestSessionController::telnetUsesSocks5_data()
{
    using namespace Qt::Literals::StringLiterals;
    QTest::addColumn<bool>("authenticate");
    QTest::addColumn<QString>("destination");
    QTest::addColumn<QByteArray>("connectRequest");
    QTest::newRow("domain-no-auth")
        << false << QStringLiteral("proxy-only.invalid")
        << ("\x05\x01\x00\x03\x12"_ba + "proxy-only.invalid"_ba + "\x00\x17"_ba);
    QTest::newRow("loopback-with-auth")
        << true << QStringLiteral("127.0.0.1")
        << "\x05\x01\x00\x01\x7f\x00\x00\x01\x00\x17"_ba;
    QTest::newRow("ipv6")
        << false << QStringLiteral("::1")
        << "\x05\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00"
           "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x17"_ba;
}

void TestSessionController::telnetUsesSocks5()
{
    using namespace Qt::Literals::StringLiterals;
    QFETCH(bool, authenticate);
    QFETCH(QString, destination);
    QFETCH(QByteArray, connectRequest);

    QTcpServer proxy;
    QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
    const QList<QByteArray> requests = authenticate
        ? QList<QByteArray>{"\x05\x01\x02"_ba, "\x01\x04user\x04pass"_ba, connectRequest}
        : QList<QByteArray>{"\x05\x01\x00"_ba, connectRequest};
    const QByteArray accepted = "\x05\x00\x00\x03\x05proxy\x00\x00SOCKS5 telnet ready\r\n"_ba;
    const QList<QByteArray> replies = authenticate
        ? QList<QByteArray>{"\x05\x02"_ba, "\x01\x00"_ba, accepted}
        : QList<QByteArray>{"\x05\x00"_ba, accepted};

    qsizetype step = 0;
    QByteArray clientInput;
    connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
        auto *peer = proxy.nextPendingConnection();
        connect(peer, &QTcpSocket::readyRead, &proxy, [&, peer] {
            if (step == requests.size()) {
                clientInput.append(peer->readAll());
            } else if (peer->bytesAvailable() >= requests[step].size()) {
                QCOMPARE(peer->read(requests[step].size()), requests[step]);
                peer->write(replies[step]);
                ++step;
            }
        });
    });

    SessionController controller;
    ConnectionProfile profile = makeProfile(QStringLiteral("Proxied Telnet"));
    profile.protocol = QStringLiteral("telnet");
    profile.host = destination;
    profile.port = 23;
    profile.telnetAutoLogin = false;
    profile.socks5.host = proxy.serverAddress().toString();
    profile.socks5.port = proxy.serverPort();
    if (authenticate) {
        profile.socks5.username = QStringLiteral("user");
        profile.socks5.password = QStringLiteral("pass");
    }
    QString error;
    const QString id = controller.open(profile, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));
    QVERIFY(waitForBuffer(controller, id, QStringLiteral("SOCKS5 telnet ready")));
    QCOMPARE(step, requests.size());

    controller.sendInput(id, "hello\r\n"_ba);
    QVERIFY(waitForBytes(clientInput, "hello\r\n"_ba));
    controller.close(id);
}

void TestSessionController::telnetSocks5Failure_data()
{
    QTest::addColumn<bool>("reply");
    QTest::addColumn<QString>("message");
    QTest::newRow("rejected") << true << QStringLiteral("SOCKS5 proxy rejected the authentication method");
    QTest::newRow("timeout") << false << QStringLiteral("SOCKS5 handshake timed out");
}

void TestSessionController::telnetSocks5Failure()
{
    QFETCH(bool, reply);
    QFETCH(QString, message);

    QTcpServer target;
    QVERIFY(target.listen(QHostAddress::LocalHost, 0));
    QSignalSpy directConnection(&target, &QTcpServer::newConnection);

    QTcpServer proxy;
    QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
    connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
        auto *peer = proxy.nextPendingConnection();
        if (reply) {
            peer->write(QByteArray("\x05\xff", 2));
        }
    });

    SessionController controller;
    ConnectionProfile profile = makeProfile();
    profile.protocol = QStringLiteral("telnet");
    profile.host = target.serverAddress().toString();
    profile.port = target.serverPort();
    profile.connectTimeoutSec = 1;
    profile.socks5.host = proxy.serverAddress().toString();
    profile.socks5.port = proxy.serverPort();
    QString error;
    const QString id = controller.open(profile, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    const auto current = [&] { return controller.sessionsAsVariantList().first().toMap(); };
    QTRY_COMPARE(current().value(QStringLiteral("status")).toString(), QStringLiteral("disconnected"));
    QCOMPARE(current().value(QStringLiteral("lastMessage")).toString(), message);
    QVERIFY(directConnection.isEmpty());
    controller.close(id);
}

void TestSessionController::telnetAutoLoginRespondsToPrompts()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QPointer<QTcpSocket> peer;
    QByteArray receivedFromClient;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        peer = server.nextPendingConnection();
        peer->setParent(&server);
        connect(peer, &QTcpSocket::readyRead, &server, [&]() {
            receivedFromClient.append(peer->readAll());
        });
        peer->write(QByteArrayLiteral("login: "));
        peer->flush();
    });

    ConnectionProfile profile = makeProfile(QStringLiteral("Telnet Login"));
    profile.protocol = QStringLiteral("telnet");
    profile.host = server.serverAddress().toString();
    profile.port = server.serverPort();
    profile.username = QStringLiteral("alice");
    profile.password = QStringLiteral("secret");

    SessionController controller;
    QString error;
    const QString id = controller.open(profile, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    QVERIFY(waitForBytes(receivedFromClient, QByteArrayLiteral("alice\r\n")));

    receivedFromClient.clear();
    QVERIFY(peer);
    peer->write(QByteArrayLiteral("Password: "));
    peer->flush();
    QVERIFY(waitForBytes(receivedFromClient, QByteArrayLiteral("secret\r\n")));

    receivedFromClient.clear();
    peer->write(QByteArrayLiteral("Password: "));
    peer->flush();
    QTest::qWait(100);
    QVERIFY(receivedFromClient.isEmpty());

    controller.close(id);
    QTest::qWait(100);
}

void TestSessionController::telnetAutoLoginCanBeDisabled()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    QPointer<QTcpSocket> peer;
    QByteArray receivedFromClient;
    connect(&server, &QTcpServer::newConnection, &server, [&]() {
        peer = server.nextPendingConnection();
        peer->setParent(&server);
        connect(peer, &QTcpSocket::readyRead, &server, [&]() {
            receivedFromClient.append(peer->readAll());
        });
        peer->write(QByteArrayLiteral("Username: "));
        peer->flush();
    });

    ConnectionProfile profile = makeProfile(QStringLiteral("Telnet Manual Login"));
    profile.protocol = QStringLiteral("telnet");
    profile.host = server.serverAddress().toString();
    profile.port = server.serverPort();
    profile.username = QStringLiteral("alice");
    profile.password = QStringLiteral("secret");
    profile.telnetAutoLogin = false;

    SessionController controller;
    QString error;
    const QString id = controller.open(profile, &error);
    QVERIFY2(!id.isEmpty(), qPrintable(error));

    QVERIFY(waitForBuffer(controller, id, QStringLiteral("Username:")));
    QTest::qWait(150);
    QVERIFY(receivedFromClient.isEmpty());

    controller.close(id);
    QTest::qWait(100);
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
