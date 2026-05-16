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
    Q_PROPERTY(QColor cursorColor READ cursorColor WRITE setCursorColor NOTIFY cursorColorChanged)
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

    QColor cursorColor() const { return m_cursorColor; }
    void setCursorColor(const QColor &c);
    QString selectedText() const;
    bool hasSelection() const;

    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void requestFocus();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

signals:
    void screenChanged();
    void metricsChanged();
    void fontChanged();
    void backgroundChanged();
    void cursorColorChanged();
    void selectionChanged();
    void cellSizeRequested(int cols, int rows);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void onScreenDamaged(const QRect &cellRect);
    void onScreenCursorMoved();
    void onScreenSizeChanged();
    void onCursorBlink();

private:
    void connectScreen();
    void disconnectScreen();
    void recomputeMetrics();
    void emitDesiredGrid();
    QPoint cellAtPosition(const QPointF &pos) const;
    QPair<QPoint, QPoint> normalizedSelection() const;
    bool isCellSelected(int row, int col) const;

    QPointer<VtScreen> m_screen;
    int m_cols = 0;
    int m_rows = 0;
    int m_cellW = 8;
    int m_cellH = 16;
    int m_baseline = 12;
    QFont m_font{QStringLiteral("Consolas"), 0};
    QColor m_background{0x02, 0x06, 0x17};
    QColor m_cursorColor{0x38, 0xbd, 0xf8};
    bool m_selectAllActive = false;
    bool m_selectionActive = false;
    bool m_selecting = false;
    QPoint m_selectionStart{0, 0};
    QPoint m_selectionEnd{0, 0};
    bool m_cursorOn = true;
    QTimer m_cursorTimer;
};
