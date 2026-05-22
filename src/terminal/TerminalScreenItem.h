#pragma once

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QRect>
#include <QString>
#include <QTimer>

class VtScreen;

// TerminalScreenItem 是 QML 端的终端控件。它订阅一个 VtScreen 的 damage/cursor/
// resize 信号，按单元格网格自绘屏幕；键盘事件直接转发给 VtScreen 让 libvterm
// 编码。VtScreen 由外部（SessionController）创建并通过 screen 属性注入。
class TerminalScreenItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *screen READ screenObject WRITE setScreenObject NOTIFY screenChanged)
    Q_PROPERTY(int cols READ cols NOTIFY metricsChanged)
    Q_PROPERTY(int rows READ rows NOTIFY metricsChanged)
    Q_PROPERTY(int cellWidth READ cellWidth NOTIFY metricsChanged)
    Q_PROPERTY(int cellHeight READ cellHeight NOTIFY metricsChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(QColor foreground READ foreground WRITE setForeground NOTIFY foregroundChanged)
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY cursorColorChanged)
    Q_PROPERTY(bool cursorBlinking READ cursorBlinking WRITE setCursorBlinking NOTIFY cursorBlinkingChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

public:
    explicit TerminalScreenItem(QQuickItem *parent = nullptr);
    ~TerminalScreenItem() override;

    void paint(QPainter *painter) override;

    QObject *screenObject() const;
    void setScreenObject(QObject *obj);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    int cellWidth() const { return m_cellW; }
    int cellHeight() const { return m_cellH; }

    QString fontFamily() const { return m_font.family(); }
    void setFontFamily(const QString &family);

    int fontPixelSize() const { return m_font.pixelSize(); }
    void setFontPixelSize(int px);

    QColor background() const { return m_background; }
    void setBackground(const QColor &c);

    QColor foreground() const { return m_foreground; }
    void setForeground(const QColor &c);

    QColor cursorColor() const { return m_cursorColor; }
    void setCursorColor(const QColor &c);
    bool cursorBlinking() const { return m_cursorBlinking; }
    void setCursorBlinking(bool enabled);
    QString selectedText() const;
    bool hasSelection() const;

    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void requestFocus();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void scrollByLines(int delta);
    Q_INVOKABLE void scrollToBottom();
    // 移动端调起软键盘。仅 Android / iOS 有实际效果，桌面是 no-op。
    Q_INVOKABLE void showSoftKeyboard();

signals:
    void screenChanged();
    void metricsChanged();
    void fontChanged();
    void backgroundChanged();
    void foregroundChanged();
    void cursorColorChanged();
    void cursorBlinkingChanged();
    void selectionChanged();
    void copySelectionRequested(const QString &text);
    void cellSizeRequested(int cols, int rows);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    // 软键盘 commit 字符走这里——Android/iOS 上软键盘大多走 IME，
    // 不直接发 keyPressEvent。这里把 commitString 转发给 VtScreen。
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private slots:
    void onScreenDamaged(const QRect &cellRect);
    void onScreenCursorMoved();
    void onScreenSizeChanged();
    void onCursorBlink();
    void onScrollbackPushed(int count);
    void onScrollbackCleared();
    void onAutoScrollTick();

private:
    void connectScreen();
    void disconnectScreen();
    void recomputeMetrics();
    void emitDesiredGrid();
    QPoint cellAtPosition(const QPointF &pos) const;
    QPair<QPoint, QPoint> normalizedSelection() const;
    bool isCellSelected(int absRow, int col) const;
    QString lineText(int absRow) const;
    QString cellText(int absRow, int col) const;
    bool isWordCharacter(const QString &text) const;
    void setSelectionRange(const QPoint &start, const QPoint &end, bool active = true);
    void selectWordAt(const QPoint &cell);
    void selectLineAt(int absRow);
    void copySelectionIfActive();
    int maxScrollOffset() const;
    void setScrollOffset(int offset);
    void updateAutoScroll();
    void sendCommittedText(const QString &text);
    void sendImeDeletion(int replacementStart, int replacementLength);
    QRectF cursorRectangle() const;
    void updateInputMethod(Qt::InputMethodQueries queries) const;
    void updateCursorTimer();

    QPointer<VtScreen> m_screen;
    int m_cols = 0;
    int m_rows = 0;
    int m_cellW = 8;
    int m_cellH = 16;
    int m_baseline = 12;
    QFont m_font;
    QColor m_background{0x02, 0x06, 0x17};
    QColor m_foreground{0xe2, 0xe8, 0xf0};
    QColor m_cursorColor{0x38, 0xbd, 0xf8};
    bool m_selectionActive = false;
    bool m_selecting = false;
    QPoint m_selectionStart{0, 0};
    QPoint m_selectionEnd{0, 0};
    QPoint m_selectionAnchor{0, 0};
    QPoint m_lastClickCell{-1, -1};
    qint64 m_lastClickMs = 0;
    int m_clickCount = 0;
    bool m_cursorBlinking = true;
    bool m_cursorOn = true;
    QTimer m_cursorTimer;
    int m_scrollOffset = 0;            // 视口顶部相对实时屏幕顶行的偏移：0=贴底
    int m_wheelPixelAccum = 0;
    int m_wheelAngleAccum = 0;
    QPointF m_lastMousePos;            // 最近一次 mouseMove 的本地坐标，给 autoscroll 用
    int m_autoScrollDir = 0;           // +1=往上拉历史，-1=往下回到实时
    QTimer m_autoScrollTimer;
};
