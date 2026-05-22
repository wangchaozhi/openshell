#include <QtTest>

#include "terminal/VtScreen.h"

// VtScreen 把 libvterm 包成 QObject。这里覆盖纯逻辑部分：尺寸、文本喂入、
// 光标推进、清屏、文本属性、键盘编码、滚动回放区。
class TestVtScreen : public QObject
{
    Q_OBJECT

private slots:
    void defaultDimensions();
    void resizeChangesDimensions();
    void feedPlainText();
    void newlineAdvancesRow();
    void cursorAdvancesWithText();
    void clearResetsScreen();
    void boldAttributeParsed();
    void sendReturnProducesCarriageReturn();
    void sendCtrlCProducesEtx();
    void sendTextProducesUnichar();
    void enqueueRawAppendsOutput();
    void scrollbackAccumulatesOnScroll();
};

void TestVtScreen::defaultDimensions()
{
    VtScreen screen;
    QCOMPARE(screen.cols(), 80);
    QCOMPARE(screen.rows(), 24);
}

void TestVtScreen::resizeChangesDimensions()
{
    VtScreen screen;
    screen.resize(100, 40);
    QCOMPARE(screen.cols(), 100);
    QCOMPARE(screen.rows(), 40);

    // 0 / 负数应被忽略，尺寸保持不变。
    screen.resize(0, -5);
    QCOMPARE(screen.cols(), 100);
    QCOMPARE(screen.rows(), 40);
}

void TestVtScreen::feedPlainText()
{
    VtScreen screen;
    screen.feed(QByteArrayLiteral("hello"));
    QCOMPARE(screen.cellAt(0, 0).text, QStringLiteral("h"));
    QCOMPARE(screen.cellAt(0, 4).text, QStringLiteral("o"));
    QVERIFY(screen.plainTextSnapshot().startsWith(QStringLiteral("hello")));
}

void TestVtScreen::newlineAdvancesRow()
{
    VtScreen screen;
    screen.feed(QByteArrayLiteral("ab\r\ncd"));
    const QStringList lines = screen.plainTextSnapshot().split(QLatin1Char('\n'));
    QVERIFY(lines.size() >= 2);
    QCOMPARE(lines.at(0), QStringLiteral("ab"));
    QCOMPARE(lines.at(1), QStringLiteral("cd"));
}

void TestVtScreen::cursorAdvancesWithText()
{
    VtScreen screen;
    screen.feed(QByteArrayLiteral("abc"));
    QCOMPARE(screen.cursorPosition(), QPoint(3, 0));
}

void TestVtScreen::clearResetsScreen()
{
    VtScreen screen;
    // clear() 会刻意保留光标所在行（活动提示行）。这里换行到一个空行后再
    // clear，整屏内容都应被清掉。
    screen.feed(QByteArrayLiteral("old line one\r\nold line two\r\n"));
    screen.clear();
    QVERIFY(screen.cellAt(0, 0).text.isEmpty());
    QVERIFY(screen.plainTextSnapshot().trimmed().isEmpty());
}

void TestVtScreen::boldAttributeParsed()
{
    VtScreen screen;
    // ESC[1m 打开加粗，随后的 X 应带 bold 属性。
    screen.feed(QByteArrayLiteral("\x1b[1mX"));
    const VtCell cell = screen.cellAt(0, 0);
    QCOMPARE(cell.text, QStringLiteral("X"));
    QVERIFY(cell.bold);
}

void TestVtScreen::sendReturnProducesCarriageReturn()
{
    VtScreen screen;
    screen.takePendingOutput(); // 清掉初始化期间可能产生的字节
    QVERIFY(screen.sendKey(Qt::Key_Return, Qt::NoModifier, QString()));
    QCOMPARE(screen.takePendingOutput(), QByteArrayLiteral("\r"));
}

void TestVtScreen::sendCtrlCProducesEtx()
{
    VtScreen screen;
    screen.takePendingOutput();
    QVERIFY(screen.sendKey(Qt::Key_C, Qt::ControlModifier, QString()));
    const QByteArray out = screen.takePendingOutput();
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.at(0), char(0x03));
}

void TestVtScreen::sendTextProducesUnichar()
{
    VtScreen screen;
    screen.takePendingOutput();
    QVERIFY(screen.sendKey(Qt::Key_A, Qt::NoModifier, QStringLiteral("a")));
    QCOMPARE(screen.takePendingOutput(), QByteArrayLiteral("a"));
}

void TestVtScreen::enqueueRawAppendsOutput()
{
    VtScreen screen;
    screen.takePendingOutput();
    screen.enqueueRaw(QByteArrayLiteral("xyz"));
    QCOMPARE(screen.takePendingOutput(), QByteArrayLiteral("xyz"));
    // 取走后应清空。
    QVERIFY(screen.takePendingOutput().isEmpty());
}

void TestVtScreen::scrollbackAccumulatesOnScroll()
{
    VtScreen screen;
    screen.resize(20, 3);
    QCOMPARE(screen.scrollbackSize(), 0);
    // 喂入超过屏幕高度的行数，顶部的行会被推进回滚区。
    screen.feed(QByteArrayLiteral("0\r\n1\r\n2\r\n3\r\n4\r\n5\r\n"));
    QVERIFY(screen.scrollbackSize() > 0);

    screen.clearScrollback();
    QCOMPARE(screen.scrollbackSize(), 0);
}

QTEST_GUILESS_MAIN(TestVtScreen)
#include "test_vt_screen.moc"
