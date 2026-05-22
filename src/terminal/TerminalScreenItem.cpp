#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>

// TerminalScreenItem 的实现按职责拆到三个文件，沿用 AppController*.cpp 的做法：
//   - TerminalScreenItem.cpp           本文件：构造、屏幕绑定、度量、绘制
//   - TerminalScreenItemInput.cpp      键鼠 / 滚轮 / 输入法 / 自动滚动
//   - TerminalScreenItemSelection.cpp  选区与复制

namespace {
constexpr QRgb kDefaultTerminalBackground = 0xff020617;
constexpr QRgb kDefaultTerminalForeground = 0xffe2e8f0;
} // namespace

TerminalScreenItem::TerminalScreenItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);

#if defined(Q_OS_MACOS)
    m_font.setFamily(QStringLiteral("Menlo"));
#endif
    if (m_font.pixelSize() <= 0) {
        m_font.setPixelSize(14);
    }
    m_font.setStyleHint(QFont::Monospace);
    m_font.setKerning(false);
    m_font.setFixedPitch(true);

    m_cursorTimer.setInterval(530);
    m_cursorTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_cursorTimer, &QTimer::timeout, this, &TerminalScreenItem::onCursorBlink);

    m_autoScrollTimer.setInterval(40);
    m_autoScrollTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_autoScrollTimer, &QTimer::timeout, this, &TerminalScreenItem::onAutoScrollTick);

    recomputeMetrics();
}

TerminalScreenItem::~TerminalScreenItem() = default;

QObject *TerminalScreenItem::screenObject() const
{
    return m_screen.data();
}

void TerminalScreenItem::setScreenObject(QObject *obj)
{
    auto *next = qobject_cast<VtScreen *>(obj);
    if (next == m_screen.data()) {
        return;
    }
    clearSelection();
    disconnectScreen();
    m_screen = next;
    m_scrollOffset = 0;
    m_wheelPixelAccum = 0;
    m_wheelAngleAccum = 0;
    connectScreen();
    emit screenChanged();
    if (m_screen) {
        m_cols = m_screen->cols();
        m_rows = m_screen->rows();
        emit metricsChanged();
        emitDesiredGrid();
    }
    update();
}

void TerminalScreenItem::connectScreen()
{
    if (!m_screen) {
        return;
    }
    connect(m_screen.data(), &VtScreen::damaged,
            this, &TerminalScreenItem::onScreenDamaged);
    connect(m_screen.data(), &VtScreen::cursorMoved,
            this, &TerminalScreenItem::onScreenCursorMoved);
    connect(m_screen.data(), &VtScreen::sizeChanged,
            this, &TerminalScreenItem::onScreenSizeChanged);
    connect(m_screen.data(), &VtScreen::scrollbackPushed,
            this, &TerminalScreenItem::onScrollbackPushed);
    connect(m_screen.data(), &VtScreen::scrollbackCleared,
            this, &TerminalScreenItem::onScrollbackCleared);
}

void TerminalScreenItem::disconnectScreen()
{
    if (!m_screen) {
        return;
    }
    disconnect(m_screen.data(), nullptr, this, nullptr);
}

void TerminalScreenItem::setFontFamily(const QString &family)
{
    if (m_font.family() == family) {
        return;
    }
    m_font.setFamily(family);
    recomputeMetrics();
    emitDesiredGrid();
    update();
    emit fontChanged();
}

void TerminalScreenItem::setFontPixelSize(int px)
{
    if (px <= 0 || m_font.pixelSize() == px) {
        return;
    }
    m_font.setPixelSize(px);
    recomputeMetrics();
    emitDesiredGrid();
    update();
    emit fontChanged();
}

void TerminalScreenItem::setBackground(const QColor &c)
{
    if (m_background == c) {
        return;
    }
    m_background = c;
    update();
    emit backgroundChanged();
}

void TerminalScreenItem::setForeground(const QColor &c)
{
    if (m_foreground == c) {
        return;
    }
    m_foreground = c;
    update();
    emit foregroundChanged();
}

void TerminalScreenItem::setCursorColor(const QColor &c)
{
    if (m_cursorColor == c) {
        return;
    }
    m_cursorColor = c;
    update();
    emit cursorColorChanged();
}

void TerminalScreenItem::setCursorBlinking(bool enabled)
{
    if (m_cursorBlinking == enabled) {
        return;
    }
    m_cursorBlinking = enabled;
    m_cursorOn = true;
    updateCursorTimer();
    update();
    emit cursorBlinkingChanged();
}

void TerminalScreenItem::sendText(const QString &text)
{
    if (m_screen && !text.isEmpty()) {
        if (m_scrollOffset != 0) {
            setScrollOffset(0);
        }
        m_screen->enqueueRaw(text.toUtf8());
    }
}

void TerminalScreenItem::requestFocus()
{
    forceActiveFocus(Qt::OtherFocusReason);
    updateInputMethod(Qt::ImEnabled | Qt::ImHints | Qt::ImCursorRectangle);
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    showSoftKeyboard();
#endif
}

void TerminalScreenItem::scrollByLines(int delta)
{
    setScrollOffset(m_scrollOffset + delta);
}

void TerminalScreenItem::scrollToBottom()
{
    setScrollOffset(0);
}

void TerminalScreenItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    emitDesiredGrid();
    updateInputMethod(Qt::ImCursorRectangle | Qt::ImInputItemClipRectangle);
}

void TerminalScreenItem::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event);
    m_cursorOn = true;
    updateCursorTimer();
    updateInputMethod(Qt::ImEnabled | Qt::ImHints | Qt::ImCursorRectangle);
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // 移动端：拿到焦点就主动调起软键盘。覆盖切到 Terminal 页时（QML 调
    // requestFocus）这条路径，那次没有 mouse press 事件兜底。
    if (auto *im = QGuiApplication::inputMethod()) {
        im->show();
    }
#endif
    update();
}

void TerminalScreenItem::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event);
    m_cursorOn = true;
    updateCursorTimer();
    update();
}

void TerminalScreenItem::onScreenDamaged(const QRect &cellRect)
{
    if (!m_cellW || !m_cellH) {
        update();
        return;
    }
    const QRect px(cellRect.x() * m_cellW,
                   (cellRect.y() + m_scrollOffset) * m_cellH,
                   cellRect.width() * m_cellW,
                   cellRect.height() * m_cellH);
    update(px);
}

void TerminalScreenItem::onScreenCursorMoved()
{
    m_cursorOn = true;
    updateInputMethod(Qt::ImCursorRectangle);
    update();
}

void TerminalScreenItem::onScreenSizeChanged()
{
    if (!m_screen) {
        return;
    }
    if (m_cols != m_screen->cols() || m_rows != m_screen->rows()) {
        m_cols = m_screen->cols();
        m_rows = m_screen->rows();
        emit metricsChanged();
        update();
    }
}

void TerminalScreenItem::onCursorBlink()
{
    m_cursorOn = !m_cursorOn;
    update();
}

void TerminalScreenItem::updateCursorTimer()
{
    if (m_cursorBlinking && hasActiveFocus()) {
        if (!m_cursorTimer.isActive()) {
            m_cursorTimer.start();
        }
    } else {
        m_cursorTimer.stop();
    }
}

void TerminalScreenItem::recomputeMetrics()
{
    const QFontMetricsF fm(m_font);
    const qreal w = fm.horizontalAdvance(QLatin1Char('W'));
    const qreal h = fm.height();
    const int newW = qMax(1, qRound(w));
    const int newH = qMax(1, qRound(h));
    if (newW == m_cellW && newH == m_cellH) {
        return;
    }
    m_cellW = newW;
    m_cellH = newH;
    m_baseline = qMax(1, qRound(fm.ascent()));
    emit metricsChanged();
}

void TerminalScreenItem::emitDesiredGrid()
{
    if (m_cellW <= 0 || m_cellH <= 0 || width() <= 0 || height() <= 0) {
        return;
    }
    const int desiredCols = qMax(20, int(width()) / m_cellW);
    const int desiredRows = qMax(4, int(height()) / m_cellH);
    emit cellSizeRequested(desiredCols, desiredRows);
}

int TerminalScreenItem::maxScrollOffset() const
{
    return m_screen ? m_screen->scrollbackSize() : 0;
}

void TerminalScreenItem::setScrollOffset(int offset)
{
    const int clamped = qBound(0, offset, maxScrollOffset());
    if (clamped == m_scrollOffset) {
        return;
    }
    m_scrollOffset = clamped;
    updateInputMethod(Qt::ImCursorRectangle);
    update();
}

void TerminalScreenItem::onScrollbackPushed(int count)
{
    if (count <= 0) {
        return;
    }
    const int sbSize = m_screen ? m_screen->scrollbackSize() : 0;

    // 视口贴底时 (scrollOffset==0) 跟随新内容；否则锚定到原来那一行历史内容上。
    if (m_scrollOffset > 0) {
        m_scrollOffset = qMin(m_scrollOffset + count, sbSize);
    }

    if (m_selectionActive || m_selecting) {
        m_selectionStart.ry() -= count;
        m_selectionEnd.ry() -= count;
        m_selectionAnchor.ry() -= count;
        if (m_selectionStart.y() < -sbSize && m_selectionEnd.y() < -sbSize) {
            clearSelection();
        }
    }
    update();
}

void TerminalScreenItem::onScrollbackCleared()
{
    m_scrollOffset = 0;
    if (m_selectionActive || m_selecting) {
        // 选区可能引用了已经丢弃的历史，统一清掉
        clearSelection();
    }
    update();
}

void TerminalScreenItem::paint(QPainter *painter)
{
    painter->fillRect(QRectF(0, 0, width(), height()), m_background);
    if (!m_screen) {
        return;
    }

    painter->setFont(m_font);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const int viewRows = qMin(m_rows, int(height() / m_cellH) + 1);
    const int cols = qMin(m_screen->cols(), int(width() / m_cellW) + 1);
    const int sbSize = m_screen->scrollbackSize();

    QFont boldFont = m_font;
    boldFont.setBold(true);
    QFont italicFont = m_font;
    italicFont.setItalic(true);
    QFont boldItalicFont = boldFont;
    boldItalicFont.setItalic(true);

    for (int r = 0; r < viewRows; ++r) {
        const int absRow = r - m_scrollOffset;
        if (absRow < -sbSize || absRow > m_rows - 1) {
            continue;
        }
        for (int c = 0; c < cols; ++c) {
            const VtCell cell = m_screen->cellAtAbsolute(absRow, c);
            if (cell.placeholder) {
                continue;
            }
            const int x = c * m_cellW;
            const int y = r * m_cellH;
            const int w = m_cellW * cell.width;
            const int h = m_cellH;
            const QColor cellBg = cell.bg.rgba() == kDefaultTerminalBackground ? m_background : cell.bg;
            const QColor cellFg = cell.fg.rgba() == kDefaultTerminalForeground ? m_foreground : cell.fg;

            if (cellBg != m_background) {
                painter->fillRect(x, y, w, h, cellBg);
            }

            if (isCellSelected(absRow, c)) {
                QColor selection = m_cursorColor;
                selection.setAlphaF(0.28);
                painter->fillRect(x, y, w, h, selection);
            }

            if (!cell.text.isEmpty()) {
                if (cell.bold && cell.italic) {
                    painter->setFont(boldItalicFont);
                } else if (cell.bold) {
                    painter->setFont(boldFont);
                } else if (cell.italic) {
                    painter->setFont(italicFont);
                } else {
                    painter->setFont(m_font);
                }
                painter->setPen(cellFg);
                painter->drawText(QRectF(x, y, w, h),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  cell.text);
            }

            if (cell.underline) {
                painter->setPen(cellFg);
                painter->drawLine(x, y + h - 1, x + w, y + h - 1);
            }
        }
    }

    if (m_screen->cursorVisible() && m_scrollOffset == 0 && (m_cursorOn || !hasActiveFocus())) {
        const QPoint cp = m_screen->cursorPosition();
        const int x = cp.x() * m_cellW;
        const int y = cp.y() * m_cellH;
        QColor fill = m_cursorColor;
        fill.setAlphaF(hasActiveFocus() ? 0.75 : 0.35);
        painter->fillRect(x, y, m_cellW, m_cellH, fill);
        const VtCell cursorCell = m_screen->cellAt(cp.y(), cp.x());
        if (!cursorCell.text.isEmpty()) {
            painter->setFont(m_font);
            painter->setPen(m_background);
            painter->drawText(QRectF(x, y, m_cellW, m_cellH),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              cursorCell.text);
        }
    }
}
