#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <libssh2_sftp.h>

#include "ssh/SftpTransfer.h"

class TestSftpTransfer : public QObject
{
    Q_OBJECT

private slots:
    void joinRemotePathHandlesRoot();
    void joinRemotePathConcatenatesSubpaths();
    void permissionsToOctalReturnsEmptyWithoutFlag();
    void permissionsToOctalFormatsCommonModes();
    void localPathSizeReturnsZeroForMissing();
    void localPathSizeMatchesFileSize();
    void localPathSizeAggregatesDirectoryTree();
};

void TestSftpTransfer::joinRemotePathHandlesRoot()
{
    QCOMPARE(SftpTransfer::joinRemotePath(QStringLiteral("/"), QStringLiteral("etc")),
             QStringLiteral("/etc"));
}

void TestSftpTransfer::joinRemotePathConcatenatesSubpaths()
{
    QCOMPARE(SftpTransfer::joinRemotePath(QStringLiteral("/var/log"), QStringLiteral("syslog")),
             QStringLiteral("/var/log/syslog"));
    QCOMPARE(SftpTransfer::joinRemotePath(QStringLiteral("/home/u"), QStringLiteral(".bashrc")),
             QStringLiteral("/home/u/.bashrc"));
}

void TestSftpTransfer::permissionsToOctalReturnsEmptyWithoutFlag()
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    attrs.flags = 0;
    QVERIFY(SftpTransfer::permissionsToOctal(attrs).isEmpty());
}

void TestSftpTransfer::permissionsToOctalFormatsCommonModes()
{
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
    attrs.permissions = 0644;
    QCOMPARE(SftpTransfer::permissionsToOctal(attrs), QStringLiteral("644"));

    attrs.permissions = 0755;
    QCOMPARE(SftpTransfer::permissionsToOctal(attrs), QStringLiteral("755"));

    // Setuid bit + 0755 (octal 4755) should still be zero-padded.
    attrs.permissions = 04755;
    QCOMPARE(SftpTransfer::permissionsToOctal(attrs), QStringLiteral("4755"));

    // Only the low 12 bits matter — file-type bits in higher nibbles are masked out.
    attrs.permissions = 0100644;  // regular file + 0644
    QCOMPARE(SftpTransfer::permissionsToOctal(attrs), QStringLiteral("644"));
}

void TestSftpTransfer::localPathSizeReturnsZeroForMissing()
{
    QFileInfo info(QStringLiteral("/this/path/does/not/exist/openshell-test"));
    QCOMPARE(SftpTransfer::localPathSize(info), qint64(0));
}

void TestSftpTransfer::localPathSizeMatchesFileSize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString filePath = QDir(dir.path()).filePath(QStringLiteral("blob.bin"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray payload(1234, 'x');
    QCOMPARE(file.write(payload), qint64(payload.size()));
    file.close();

    QCOMPARE(SftpTransfer::localPathSize(QFileInfo(filePath)), qint64(payload.size()));
}

void TestSftpTransfer::localPathSizeAggregatesDirectoryTree()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    QVERIFY(root.mkpath(QStringLiteral("nested/deep")));

    auto writeBytes = [](const QString &path, int size) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(QByteArray(size, 'a')), qint64(size));
        f.close();
    };

    writeBytes(root.filePath(QStringLiteral("top.bin")), 100);
    writeBytes(root.filePath(QStringLiteral("nested/middle.bin")), 200);
    writeBytes(root.filePath(QStringLiteral("nested/deep/leaf.bin")), 300);

    QCOMPARE(SftpTransfer::localPathSize(QFileInfo(dir.path())), qint64(600));
}

QTEST_GUILESS_MAIN(TestSftpTransfer)
#include "test_sftp_transfer.moc"
