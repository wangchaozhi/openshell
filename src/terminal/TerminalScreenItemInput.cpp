#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyleHints>
#include <QWheelEvent>

// TerminalScreenItem 的输入部分：键盘 / 鼠标 / 滚轮 / 输入法 / 自动滚动。

namespace {
qint64 monotonicMs()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer.elapsed();
}
} // namespace

void TerminalScreenItem::showSoftKeyboard()
{
    forceActiveFocus(Qt::OtherFocusReason);
    updateInputMethod(Qt::ImEnabled | Qt::ImHints | Qt::ImCursorRectangle);
    if (auto *im = QGuiApplication::inputMethod()) {
        im->show();
    }
}

void TerminalScreenItem::inputMethodEvent(QInputMethodEvent *event)
{
    if (!m_screen) {
        QQuickPaintedItem::inputMethodEvent(event);
        return;
    }
    if (m_scrollOffset != 0) {
        setScrollOffset(0);
    }
    sendImeDeletion(event->replacementStart(), event->replacementLength());
    const QString commit = event->commitString();
    if (!commit.isEmpty()) {
        sendCommittedText(commit);
    }
    updateInputMethod(Qt::ImCursorRectangle | Qt::ImSurroundingText | Qt::ImCursorPosition);
    event->accept();
}

QVariant TerminalScreenItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImEnabled:
        return true;
    case Qt::ImHints:
        // 终端里不能让输入法自动大写、联想纠错或学习输入内容；否则命令、
        // 密码、路径都会被 IME 自作主张改写。
        return QVariant(int(Qt::ImhNoAutoUppercase
                            | Qt::ImhNoPredictiveText
                            | Qt::ImhSensitiveData));
    case Qt::ImCursorRectangle:
        return cursorRectangle();
    case Qt::ImFont:
        return QVariant::fromValue(m_font);
    case Qt::ImCursorPosition:
        return 0;
    case Qt::ImSurroundingText:
        return QString();
    case Qt::ImCurrentSelection:
        return QString();
    case Qt::ImMaximumTextLength:
        return -1;
    case Qt::ImAnchorPosition:
        return 0;
    case Qt::ImInputItemClipRectangle:
        return QRectF(0, 0, width(), height());
    default:
        return QQuickPaintedItem::inputMethodQuery(query);
    }
}

void TerminalScreenItem::keyPressEvent(QKeyEvent *event)
{
    // 用户输入时回到实时屏幕底部，看见自己敲的字。
    if (m_scrollOffset != 0) {
        setScrollOffset(0);
    }

    if (!m_screen) {
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }
    const bool handled = m_screen->sendKey(event->key(), event->modifiers(), event->text());

    if (handled) {
        event->accept();
    } else {
        QQuickPaintedItem::keyPressEvent(event);
    }
}

void TerminalScreenItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // 移动端：点 terminal 主动调起软键盘，否则光设 focus 也不弹。
    if (auto *im = QGuiApplication::inputMethod()) {
        im->show();
    }
#endif
    if (event->button() == Qt::LeftButton && m_screen) {
        const QPoint cell = cellAtPosition(event->position());
        const qint64 now = monotonicMs();
        const int doubleClickMs = QGuiApplication::styleHints()->mouseDoubleClickInterval();
        if (cell == m_lastClickCell && now - m_lastClickMs <= doubleClickMs) {
            ++m_clickCount;
        } else {
            m_clickCount = 1;
        }
        m_lastClickCell = cell;
        m_lastClickMs = now;

        if (event->modifiers() & Qt::ShiftModifier) {
            if (!m_selectionActive) {
                m_selectionAnchor = m_selectionStart;
            }
            setSelectionRange(m_selectionAnchor, cell);
        } else if (m_clickCount >= 3) {
            selectLineAt(cell.y());
        } else if (m_clickCount == 2) {
            selectWordAt(cell);
        } else {
            clearSelection();
            m_selectionAnchor = cell;
            m_selectionStart = cell;
            m_selectionEnd = cell;
        }
        m_selecting = true;
        m_lastMousePos = event->position();
        event->accept();
        return;
    }
    if (event->button() != Qt::RightButton) {
        clearSelection();
    }
    QQuickPaintedItem::mousePressEvent(event);
}

void TerminalScreenItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting && m_screen && (event->buttons() & Qt::LeftButton)) {
        m_lastMousePos = event->position();
        const QPoint next = cellAtPosition(event->position());
        if (next != m_selectionEnd) {
            setSelectionRange(m_selectionAnchor, next, next != m_selectionAnchor);
        }
        updateAutoScroll();
        event->accept();
        return;
    }
    QQuickPaintedItem::mouseMoveEvent(event);
}

void TerminalScreenItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_autoScrollTimer.stop();
        m_autoScrollDir = 0;
        if (m_clickCount < 2) {
            const QPoint next = cellAtPosition(event->position());
            setSelectionRange(m_selectionAnchor, next, next != m_selectionAnchor);
        }
        update();
        copySelectionIfActive();
        event->accept();
        return;
    }
    QQuickPaintedItem::mouseReleaseEvent(event);
}

void TerminalScreenItem::wheelEvent(QWheelEvent *event)
{
    if (!m_screen) {
        QQuickPaintedItem::wheelEvent(event);
        return;
    }
    const int maxOff = maxScrollOffset();
    if (maxOff <= 0 && m_scrollOffset == 0) {
        event->ignore();
        return;
    }

    int linesDelta = 0;
    const QPoint pixelDelta = event->pixelDelta();
    const QPoint angleDelta = event->angleDelta();
    if (!pixelDelta.isNull()) {
        m_wheelPixelAccum += pixelDelta.y();
        const int cellH = qMax(1, m_cellH);
        linesDelta = m_wheelPixelAccum / cellH;
        m_wheelPixelAccum -= linesDelta * cellH;
    } else if (!angleDelta.isNull()) {
        m_wheelAngleAccum += angleDelta.y();
        // 一次标准滚轮 tick = 120 units，按 3 行/格
        const int unitsPerLine = 40;
        linesDelta = m_wheelAngleAccum / unitsPerLine;
        m_wheelAngleAccum -= linesDelta * unitsPerLine;
    }
    if (linesDelta != 0) {
        setScrollOffset(m_scrollOffset + linesDelta);
    }
    event->accept();
}

QPoint TerminalScreenItem::cellAtPosition(const QPointF &pos) const
{
    // 鼠标 y 可能在 widget 外（autoscroll 时），用 floor 而不是 clamp 才能正确
    // 反映 "拉到了上方还是下方"。最终 clamp 到 [-scrollbackSize, m_rows-1]。
    const int cellH = qMax(1, m_cellH);
    int viewRow;
    if (pos.y() >= 0) {
        viewRow = int(pos.y()) / cellH;
    } else {
        viewRow = -1 - int(-pos.y() - 1) / cellH; // floor 除法
    }
    int absRow = viewRow - m_scrollOffset;
    const int sbSize = m_screen ? m_screen->scrollbackSize() : 0;
    absRow = qBound(-sbSize, absRow, qMax(0, m_rows - 1));

    const int col = qBound(0, int(pos.x()) / qMax(1, m_cellW), qMax(0, m_cols - 1));
    return QPoint(col, absRow);
}

void TerminalScreenItem::sendCommittedText(const QString &text)
{
    if (!m_screen || text.isEmpty()) {
        return;
    }

    QString printableRun;
    const auto flushPrintableRun = [this, &printableRun]() {
        if (!printableRun.isEmpty()) {
            m_screen->sendKey(0, Qt::NoModifier, printableRun);
            printableRun.clear();
        }
    };

    for (const QChar &ch : text) {
        switch (ch.unicode()) {
        case '\r':
        case '\n':
            flushPrintableRun();
            m_screen->sendKey(Qt::Key_Return, Qt::NoModifier, QString());
            break;
        case '\t':
            flushPrintableRun();
            m_screen->sendKey(Qt::Key_Tab, Qt::NoModifier, QString());
            break;
        case '\b':
        case 0x7f:
            flushPrintableRun();
            m_screen->sendKey(Qt::Key_Backspace, Qt::NoModifier, QString());
            break;
        case 0x1b:
            flushPrintableRun();
            m_screen->sendKey(Qt::Key_Escape, Qt::NoModifier, QString());
            break;
        default:
            printableRun.append(ch);
            break;
        }
    }
    flushPrintableRun();
}

void TerminalScreenItem::sendImeDeletion(int replacementStart, int replacementLength)
{
    if (!m_screen || replacementLength <= 0) {
        return;
    }

    const int key = replacementStart < 0 ? Qt::Key_Backspace : Qt::Key_Delete;
    for (int i = 0; i < replacementLength; ++i) {
        m_screen->sendKey(key, Qt::NoModifier, QString());
    }
}

QRectF TerminalScreenItem::cursorRectangle() const
{
    const int cellW = qMax(1, m_cellW);
    const int cellH = qMax(1, m_cellH);
    if (!m_screen) {
        return QRectF(0, 0, cellW, cellH);
    }

    const QPoint cp = m_screen->cursorPosition();
    const qreal maxX = qMax<qreal>(0, width() - cellW);
    const qreal maxY = qMax<qreal>(0, height() - cellH);
    const qreal x = qBound<qreal>(0, cp.x() * cellW, maxX);
    const qreal y = qBound<qreal>(0, (cp.y() + m_scrollOffset) * cellH, maxY);
    return QRectF(x, y, cellW, cellH);
}

void TerminalScreenItem::updateInputMethod(Qt::InputMethodQueries queries) const
{
    if (hasActiveFocus()) {
        if (auto *im = QGuiApplication::inputMethod()) {
            im->update(queries);
        }
    }
}

void TerminalScreenItem::updateAutoScroll()
{
    if (!m_selecting) {
        m_autoScrollTimer.stop();
        m_autoScrollDir = 0;
        return;
    }
    int dir = 0;
    if (m_lastMousePos.y() < 0) {
        dir = +1;
    } else if (m_lastMousePos.y() > height()) {
        dir = -1;
    }
    m_autoScrollDir = dir;
    if (dir == 0) {
        m_autoScrollTimer.stop();
    } else if (!m_autoScrollTimer.isActive()) {
        m_autoScrollTimer.start();
    }
}

void TerminalScreenItem::onAutoScrollTick()
{
    if (!m_selecting || m_autoScrollDir == 0 || !m_screen) {
        m_autoScrollTimer.stop();
        m_autoScrollDir = 0;
        return;
    }
    const int before = m_scrollOffset;
    setScrollOffset(m_scrollOffset + m_autoScrollDir);
    if (m_scrollOffset == before) {
        // 到底/到顶，停一下，避免空转
        m_autoScrollTimer.stop();
        return;
    }
    const QPoint next = cellAtPosition(m_lastMousePos);
    if (next != m_selectionEnd) {
        setSelectionRange(m_selectionAnchor, next, next != m_selectionAnchor);
    }
}
