#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QDebug>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QStyleHints>

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

TerminalScreenItem::TerminalScreenItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);

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

void TerminalScreenItem::setCursorColor(const QColor &c)
{
    if (m_cursorColor == c) {
        return;
    }
    m_cursorColor = c;
    update();
    emit cursorColorChanged();
}

QString TerminalScreenItem::selectedText() const
{
    if (!m_screen || !m_selectionActive) {
        return QString();
    }

    const auto bounds = normalizedSelection();
    const QPoint start = bounds.first;
    const QPoint end = bounds.second;
    QString text;
    for (int row = start.y(); row <= end.y(); ++row) {
        QString line;
        const int firstCol = row == start.y() ? start.x() : 0;
        const int lastCol = row == end.y() ? end.x() : m_cols - 1;
        for (int col = firstCol; col <= lastCol; ++col) {
            const VtCell cell = m_screen->cellAtAbsolute(row, col);
            if (cell.placeholder) {
                continue;
            }
            if (cell.text.isEmpty()) {
                line.append(QLatin1Char(' '));
            } else {
                line.append(cell.text);
            }
        }
        while (!line.isEmpty() && line.endsWith(QLatin1Char(' '))) {
            line.chop(1);
        }
        text.append(line);
        if (row < end.y()) {
            text.append(QLatin1Char('\n'));
        }
    }
    return text;
}

bool TerminalScreenItem::hasSelection() const
{
    return m_screen && m_selectionActive && !selectedText().isEmpty();
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
}

void TerminalScreenItem::selectAll()
{
    if (!m_screen) {
        return;
    }
    const int sbSize = m_screen->scrollbackSize();
    m_selecting = false;
    m_selectionAnchor = QPoint(0, -sbSize);
    setSelectionRange(QPoint(0, -sbSize),
                      QPoint(qMax(0, m_cols - 1), qMax(0, m_rows - 1)));
    copySelectionIfActive();
}

void TerminalScreenItem::clearSelection()
{
    m_autoScrollTimer.stop();
    m_autoScrollDir = 0;
    if (!m_selectionActive && !m_selecting) {
        return;
    }
    m_selectionActive = false;
    m_selecting = false;
    update();
    emit selectionChanged();
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
}

void TerminalScreenItem::keyPressEvent(QKeyEvent *event)
{
    // 用户输入时回到实时屏幕底部，看见自己敲的字。
    if (m_scrollOffset != 0) {
        setScrollOffset(0);
    }

    QString logMsg = QString("KEY EVENT: key=%1 modifiers=%2 text=[%3] focus=%4")
        .arg(event->key()).arg((int)event->modifiers()).arg(event->text()).arg(hasActiveFocus());

    QFile logFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/openshell_keyboard.log");
    logFile.open(QIODevice::Append | QIODevice::Text);
    QTextStream out(&logFile);
    out << logMsg << "\n";
    logFile.close();

    if (!m_screen) {
        QQuickPaintedItem::keyPressEvent(event);
        return;
    }
    const bool handled = m_screen->sendKey(event->key(), event->modifiers(), event->text());
    logFile.open(QIODevice::Append | QIODevice::Text);
    out.setDevice(&logFile);
    out << "  -> handled: " << handled << "\n";
    logFile.close();

    if (handled) {
        event->accept();
    } else {
        QQuickPaintedItem::keyPressEvent(event);
    }
}

void TerminalScreenItem::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus(Qt::MouseFocusReason);
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

void TerminalScreenItem::focusInEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusInEvent(event);
    m_cursorOn = true;
    m_cursorTimer.start();
    update();
}

void TerminalScreenItem::focusOutEvent(QFocusEvent *event)
{
    QQuickPaintedItem::focusOutEvent(event);
    m_cursorTimer.stop();
    m_cursorOn = true;
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
    update();
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

QPair<QPoint, QPoint> TerminalScreenItem::normalizedSelection() const
{
    QPoint start = m_selectionStart;
    QPoint end = m_selectionEnd;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x())) {
        std::swap(start, end);
    }
    return qMakePair(start, end);
}

bool TerminalScreenItem::isCellSelected(int absRow, int col) const
{
    if (!m_selectionActive) {
        return false;
    }
    const auto bounds = normalizedSelection();
    const QPoint start = bounds.first;
    const QPoint end = bounds.second;
    if (absRow < start.y() || absRow > end.y()) {
        return false;
    }
    if (absRow == start.y() && col < start.x()) {
        return false;
    }
    if (absRow == end.y() && col > end.x()) {
        return false;
    }
    return true;
}

QString TerminalScreenItem::lineText(int absRow) const
{
    QString line;
    if (!m_screen) {
        return line;
    }
    line.reserve(m_cols);
    for (int col = 0; col < m_cols; ++col) {
        const VtCell cell = m_screen->cellAtAbsolute(absRow, col);
        if (cell.placeholder) {
            continue;
        }
        if (cell.text.isEmpty()) {
            line.append(QLatin1Char(' '));
        } else {
            line.append(cell.text);
        }
    }
    return line;
}

QString TerminalScreenItem::cellText(int absRow, int col) const
{
    if (!m_screen) {
        return QString();
    }
    return m_screen->cellAtAbsolute(absRow, col).text;
}

bool TerminalScreenItem::isWordCharacter(const QString &text) const
{
    if (text.isEmpty()) {
        return false;
    }
    const QChar ch = text.at(0);
    return ch.isLetterOrNumber()
           || ch == QLatin1Char('_')
           || ch == QLatin1Char('-')
           || ch == QLatin1Char('.')
           || ch == QLatin1Char('/')
           || ch == QLatin1Char(':')
           || ch == QLatin1Char('@')
           || ch == QLatin1Char('~');
}

void TerminalScreenItem::setSelectionRange(const QPoint &start, const QPoint &end, bool active)
{
    const bool changed = m_selectionStart != start
                         || m_selectionEnd != end
                         || m_selectionActive != active;
    m_selectionStart = start;
    m_selectionEnd = end;
    m_selectionActive = active;
    if (changed) {
        update();
        emit selectionChanged();
    }
}

void TerminalScreenItem::selectWordAt(const QPoint &cell)
{
    if (!m_screen) {
        return;
    }
    if (!isWordCharacter(cellText(cell.y(), cell.x()))) {
        setSelectionRange(cell, cell, false);
        return;
    }
    int startCol = cell.x();
    int endCol = cell.x();
    while (startCol > 0 && isWordCharacter(cellText(cell.y(), startCol - 1))) {
        --startCol;
    }
    while (endCol + 1 < m_cols && isWordCharacter(cellText(cell.y(), endCol + 1))) {
        ++endCol;
    }
    m_selectionAnchor = QPoint(startCol, cell.y());
    setSelectionRange(m_selectionAnchor, QPoint(endCol, cell.y()));
    copySelectionIfActive();
}

void TerminalScreenItem::selectLineAt(int absRow)
{
    QString line = lineText(absRow);
    while (!line.isEmpty() && line.endsWith(QLatin1Char(' '))) {
        line.chop(1);
    }
    if (line.isEmpty()) {
        setSelectionRange(QPoint(0, absRow), QPoint(0, absRow), false);
        return;
    }
    m_selectionAnchor = QPoint(0, absRow);
    setSelectionRange(m_selectionAnchor, QPoint(qMin(m_cols - 1, line.size() - 1), absRow));
    copySelectionIfActive();
}

void TerminalScreenItem::copySelectionIfActive()
{
    const QString text = selectedText();
    if (!text.isEmpty()) {
        emit copySelectionRequested(text);
    }
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

            if (cell.bg != m_background) {
                painter->fillRect(x, y, w, h, cell.bg);
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
                painter->setPen(cell.fg);
                painter->drawText(QRectF(x, y, w, h),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  cell.text);
            }

            if (cell.underline) {
                painter->setPen(cell.fg);
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
