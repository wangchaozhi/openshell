#pragma once

#include <QByteArray>
#include <QColor>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include <vterm.h>

// 一个单元格的渲染快照。Renderer 拷一份后即可放心绘制。
struct VtCell
{
    QString text;
    QColor fg{0xe2, 0xe8, 0xf0};
    QColor bg{0x02, 0x06, 0x17};
    int width = 1; // 1 = 普通，2 = CJK 双宽（第二格为占位）
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool reverse = false;
    bool placeholder = false; // 双宽字符的第二格
};

// VtScreen 把 libvterm 包成 QObject。
//
// 线程模型：所有公开方法都必须在创建它的线程上调用。worker 线程的输出 chunk
// 通过 queued connection 送到主线程，由主线程 feed 进来；键盘/调整大小也在
// 主线程触发；libvterm 通过回调要回发给远端的字节累积在 m_pendingOutput，由
// outputReady 信号通知监听者用 takePendingOutput() 取走。
class VtScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int cols READ cols NOTIFY sizeChanged)
    Q_PROPERTY(int rows READ rows NOTIFY sizeChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)

public:
    explicit VtScreen(QObject *parent = nullptr);
    ~VtScreen() override;

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    QString title() const { return m_title; }

    // 将屏幕重设为 (cols, rows)，0/负数会被忽略。
    void resize(int cols, int rows);

    // 喂入从远端读到的原始字节。
    void feed(const QByteArray &data);

    // 取走 libvterm 要发回远端的字节（键盘/鼠标响应等）。
    QByteArray takePendingOutput();

    // 直接把字节追加到 pending output（不经 libvterm），用于"粘贴文本"。
    void enqueueRaw(const QByteArray &data);

    // 编码键盘事件并触发 outputReady。返回是否消耗了事件。
    bool sendKey(int qtKey, Qt::KeyboardModifiers modifiers, const QString &text);

    // 把屏幕清零（发 ESC[2J ESC[H 给 libvterm，等价于 clear 命令）。
    void clear();

    // 读取一个 cell（越界返回空白格）。
    VtCell cellAt(int row, int col) const;

    QPoint cursorPosition() const { return m_cursorPos; }
    bool cursorVisible() const { return m_cursorVisible; }

    // 把整屏转成纯文本（行末空白裁掉），用于切 tab 重放 / 测试断言。
    QString plainTextSnapshot() const;

signals:
    void damaged(const QRect &cellRect); // cellRect.x=col, y=row, w=cols, h=rows
    void cursorMoved();
    void sizeChanged();
    void titleChanged(const QString &title);
    void bellRang();
    void outputReady();

private:
    static int sDamage(VTermRect rect, void *user);
    static int sMoveRect(VTermRect dest, VTermRect src, void *user);
    static int sMoveCursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
    static int sSetTermProp(VTermProp prop, VTermValue *val, void *user);
    static int sBell(void *user);
    static int sResize(int rows, int cols, void *user);
    static void sOutput(const char *s, size_t len, void *user);

    int handleDamage(int startRow, int startCol, int endRow, int endCol);
    int handleMoveCursor(int row, int col, int visible);
    int handleSetTermProp(VTermProp prop, VTermValue *val);
    int handleBell();
    int handleResize(int rows, int cols);
    void appendOutput(const char *s, size_t len);

    void initialiseVTerm();

    VTerm *m_vt = nullptr;
    VTermScreen *m_screen = nullptr;
    int m_cols = 80;
    int m_rows = 24;
    QPoint m_cursorPos{0, 0};
    bool m_cursorVisible = true;
    QString m_title;
    QByteArray m_pendingOutput;
};
