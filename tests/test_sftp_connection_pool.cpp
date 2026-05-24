#include <QtTest>
#include <QCoreApplication>

#include "ssh/SftpConnectionPool.h"

class TestSftpConnectionPool : public QObject
{
    Q_OBJECT

private slots:
    void ensureLibssh2IsIdempotent();
    void leaseMoveConstructorTransfersOwnership();
    void leaseMoveAssignmentTransfersOwnership();
    void acquireProducesUsableLeaseForFreshProfile();
    void acquireSeparatesLanes();
    void acquireReusesConnectionAfterRelease();
};

void TestSftpConnectionPool::ensureLibssh2IsIdempotent()
{
    QString err;
    QVERIFY(SftpConnectionPool::ensureLibssh2(&err));
    QVERIFY(err.isEmpty());
    QVERIFY(SftpConnectionPool::ensureLibssh2(&err));
    QVERIFY(err.isEmpty());
}

void TestSftpConnectionPool::leaseMoveConstructorTransfersOwnership()
{
    ConnectionProfile profile;
    profile.id = QStringLiteral("move-ctor");
    profile.host = QStringLiteral("127.0.0.1");
    profile.port = 22;
    profile.username = QStringLiteral("nobody");

    SftpConnectionPool::Lease a = SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Browse);
    auto *cached = a.get();
    QVERIFY(cached != nullptr);

    SftpConnectionPool::Lease b(std::move(a));
    QCOMPARE(b.get(), cached);
    QVERIFY(a.get() == nullptr);
    // b's destructor must release the lock; if not, the next acquire below would deadlock.
}

void TestSftpConnectionPool::leaseMoveAssignmentTransfersOwnership()
{
    ConnectionProfile profile;
    profile.id = QStringLiteral("move-assign");
    profile.host = QStringLiteral("127.0.0.1");
    profile.username = QStringLiteral("nobody");

    SftpConnectionPool::Lease lease;
    {
        SftpConnectionPool::Lease tmp =
            SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Browse);
        auto *cached = tmp.get();
        lease = std::move(tmp);
        QCOMPARE(lease.get(), cached);
        QVERIFY(tmp.get() == nullptr);
    }
    // Reassigning over an active lease must release the previous lock so we don't
    // deadlock when the lease goes out of scope.
}

void TestSftpConnectionPool::acquireProducesUsableLeaseForFreshProfile()
{
    ConnectionProfile profile;
    profile.id = QStringLiteral("fresh");
    profile.host = QStringLiteral("example.test");
    profile.username = QStringLiteral("u");

    SftpConnectionPool::Lease lease =
        SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Browse);
    QVERIFY(lease.get() != nullptr);
    // No SFTP session yet because we never called ensureConnected.
    QVERIFY(SftpConnectionPool::session(lease.get()) == nullptr);
    QVERIFY(SftpConnectionPool::sftp(lease.get()) == nullptr);
}

void TestSftpConnectionPool::acquireSeparatesLanes()
{
    ConnectionProfile profile;
    profile.id = QStringLiteral("lane-split");
    profile.host = QStringLiteral("example.test");
    profile.username = QStringLiteral("u");

    SftpConnectionPool::Lease browse =
        SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Browse);
    SftpConnectionPool::Lease transfer =
        SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Transfer);

    QVERIFY(browse.get() != nullptr);
    QVERIFY(transfer.get() != nullptr);
    // Different lanes hand out distinct cached connections; otherwise long transfers
    // would block directory browsing.
    QVERIFY(browse.get() != transfer.get());
}

void TestSftpConnectionPool::acquireReusesConnectionAfterRelease()
{
    ConnectionProfile profile;
    profile.id = QStringLiteral("reuse");
    profile.host = QStringLiteral("example.test");
    profile.username = QStringLiteral("u");

    void *firstAddr = nullptr;
    {
        SftpConnectionPool::Lease lease =
            SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Exec);
        firstAddr = lease.get();
        QVERIFY(firstAddr != nullptr);
    }
    SftpConnectionPool::Lease again =
        SftpConnectionPool::acquire(profile, SftpConnectionPool::Lane::Exec);
    // Exec lane has pool size 1, so the same CachedConnection should come back.
    QCOMPARE(static_cast<void *>(again.get()), firstAddr);
}

QTEST_GUILESS_MAIN(TestSftpConnectionPool)
#include "test_sftp_connection_pool.moc"
