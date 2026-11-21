#include <QtTest>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include "ConnectionCatalog.h"

class TestConnectionCatalog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void upsertRequiresNameAndHost();
    void upsertAssignsIdWhenMissing();
    void upsertRoundTripsAllFields();
    void invalidSocks5ProfileIsRejected();
    void removeDeletesProfile();

private:
    QTemporaryDir m_tempDir;
};

void TestConnectionCatalog::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("OpenShellTest"));
    QCoreApplication::setApplicationName(QStringLiteral("OpenShellTest"));
    // Redirect AppDataLocation reads to the temp directory.
    QStandardPaths::setTestModeEnabled(true);
}

void TestConnectionCatalog::upsertRequiresNameAndHost()
{
    ConnectionCatalog catalog;
    ConnectionProfile p;
    QString error;
    QVERIFY(!catalog.upsert(p, &error));
    QVERIFY(!error.isEmpty());

    p.name = QStringLiteral("Test");
    error.clear();
    QVERIFY(!catalog.upsert(p, &error));
    QVERIFY(!error.isEmpty());
}

void TestConnectionCatalog::upsertAssignsIdWhenMissing()
{
    ConnectionCatalog catalog;
    ConnectionProfile p;
    p.name = QStringLiteral("Auto Id");
    p.host = QStringLiteral("example.com");
    QString error;
    QVERIFY2(catalog.upsert(p, &error), qPrintable(error));

    bool found = false;
    for (const ConnectionProfile &profile : catalog.profiles()) {
        if (profile.name == QStringLiteral("Auto Id")) {
            QVERIFY(!profile.id.isEmpty());
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestConnectionCatalog::upsertRoundTripsAllFields()
{
    ConnectionCatalog catalog;
    ConnectionProfile p;
    p.name = QStringLiteral("Round Trip %1")
                 .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    p.host = QStringLiteral("rt.example");
    p.port = 2222;
    p.username = QStringLiteral("admin");
    p.protocol = QStringLiteral("sftp");
    p.authType = QStringLiteral("key");
    p.privateKeyPath = QStringLiteral("/keys/id_rsa");
    p.telnetAutoLogin = false;
    p.telnetTerminalType = QStringLiteral("vt100");
    p.socks5 = {QStringLiteral("proxy.example"), 10808,
                QStringLiteral("proxy-user"), QStringLiteral("test-password")};
    p.group = QStringLiteral("Production");
    p.notes = QStringLiteral("Datacenter A");

    QString error;
    QVERIFY2(catalog.upsert(p, &error), qPrintable(error));

    catalog.reload();
    bool found = false;
    for (const ConnectionProfile &profile : catalog.profiles()) {
        if (profile.name == p.name) {
            QCOMPARE(profile.host, QStringLiteral("rt.example"));
            QCOMPARE(profile.port, 2222);
            QCOMPARE(profile.username, QStringLiteral("admin"));
            QCOMPARE(profile.protocol, QStringLiteral("sftp"));
            QCOMPARE(profile.authType, QStringLiteral("key"));
            QCOMPARE(profile.privateKeyPath, QStringLiteral("/keys/id_rsa"));
            QCOMPARE(profile.telnetAutoLogin, false);
            QCOMPARE(profile.telnetTerminalType, QStringLiteral("vt100"));
            QVERIFY(profile.socks5 == p.socks5);
            QCOMPARE(profile.group, QStringLiteral("Production"));
            QCOMPARE(profile.notes, QStringLiteral("Datacenter A"));
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestConnectionCatalog::invalidSocks5ProfileIsRejected()
{
    const ConnectionProfile legacy = ConnectionProfile::fromVariantMap({});
    QVERIFY(!legacy.socks5.enabled());
    QCOMPARE(legacy.socks5.port, 1080);

    ConnectionCatalog catalog;
    ConnectionProfile p;
    p.name = QStringLiteral("Invalid proxy");
    p.host = QStringLiteral("example.test");
    p.socks5.host = QStringLiteral("proxy.test");
    p.socks5.port = 65536;
    QString error;
    QVERIFY(!catalog.upsert(p, &error));
    QVERIFY(!error.isEmpty());

    p.socks5.port = 1080;
    p.socks5.username = QStringLiteral("user");
    error.clear();
    QVERIFY(!catalog.upsert(p, &error));
    QVERIFY(!error.isEmpty());
}

void TestConnectionCatalog::removeDeletesProfile()
{
    ConnectionCatalog catalog;
    ConnectionProfile p;
    p.name = QStringLiteral("To Delete");
    p.host = QStringLiteral("delete.me");
    QString error;
    QVERIFY2(catalog.upsert(p, &error), qPrintable(error));

    QString id;
    for (const ConnectionProfile &profile : catalog.profiles()) {
        if (profile.name == QStringLiteral("To Delete")) {
            id = profile.id;
            break;
        }
    }
    QVERIFY(!id.isEmpty());

    QVERIFY(catalog.remove(id, &error));
    QVERIFY(!catalog.contains(id));
}

QTEST_GUILESS_MAIN(TestConnectionCatalog)
#include "test_connection_catalog.moc"
