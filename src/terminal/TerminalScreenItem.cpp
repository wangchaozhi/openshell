#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QDebug>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
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
    if (!m_screen) {
        return QString();
    }
    if (m_selectAllActive) {
        QString text = m_screen->plainTextSnapshot();
        while (text.endsWith(QLatin1Char('\n')) || text.endsWith(QLatin1Char(' '))) {
            text.chop(1);
        }
        return text;
    }
    if (!m_selectionActive) {
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
            const VtCell cell = m_screen->cellAt(row, col);
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
    return m_screen && (m_selectAllActive || m_selectionActive) && !selectedText().isEmpty();
}

void TerminalScreenItem::sendText(const QString &text)
{
    if (m_screen && !text.isEmpty()) {
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
    m_selectionActive = false;
    m_selecting = false;
    m_selectAllActive = true;
    update();
    emit selectionChanged();
    copySelectionIfActive();
}

void TerminalScreenItem::clearSelection()
{
    if (!m_selectAllActive && !m_selectionActive && !m_selecting) {
        return;
    }
    m_selectAllActive = false;
    m_selectionActive = false;
    m_selecting = false;
    update();
    emit selectionChanged();
}

void TerminalScreenItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    emitDesiredGrid();
}

void TerminalScreenItem::keyPressEvent(QKeyEvent *event)
{
    if (m_selectAllActive && !(event->modifiers() & Qt::ControlModifier)) {
        clearSelection();
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
            if (!m_selectionActive && !m_selectAllActive) {
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
        const QPoint next = cellAtPosition(event->position());
        if (next != m_selectionEnd) {
            setSelectionRange(m_selectionAnchor, next, next != m_selectionAnchor);
        }
        event->accept();
        return;
    }
    QQuickPaintedItem::mouseMoveEvent(event);
}

void TerminalScreenItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
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
                   cellRect.y() * m_cellH,
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
    const int col = qBound(0, int(pos.x()) / qMax(1, m_cellW), qMax(0, m_cols - 1));
    const int row = qBound(0, int(pos.y()) / qMax(1, m_cellH), qMax(0, m_rows - 1));
    return QPoint(col, row);
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

bool TerminalScreenItem::isCellSelected(int row, int col) const
{
    if (m_selectAllActive) {
        return true;
    }
    if (!m_selectionActive) {
        return false;
    }
    const auto bounds = normalizedSelection();
    const QPoint start = bounds.first;
    const QPoint end = bounds.second;
    if (row < start.y() || row > end.y()) {
        return false;
    }
    if (row == start.y() && col < start.x()) {
        return false;
    }
    if (row == end.y() && col > end.x()) {
        return false;
    }
    return true;
}

QString TerminalScreenItem::lineText(int row) const
{
    QString line;
    if (!m_screen || row < 0 || row >= m_rows) {
        return line;
    }
    line.reserve(m_cols);
    for (int col = 0; col < m_cols; ++col) {
        const VtCell cell = m_screen->cellAt(row, col);
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
    const bool changed = m_selectAllActive
                         || m_selectionStart != start
                         || m_selectionEnd != end
                         || m_selectionActive != active;
    m_selectAllActive = false;
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
    if (!isWordCharacter(m_screen->cellAt(cell.y(), cell.x()).text)) {
        setSelectionRange(cell, cell, false);
        return;
    }
    int startCol = cell.x();
    int endCol = cell.x();
    while (startCol > 0 && isWordCharacter(m_screen->cellAt(cell.y(), startCol - 1).text)) {
        --startCol;
    }
    while (endCol + 1 < m_cols && isWordCharacter(m_screen->cellAt(cell.y(), endCol + 1).text)) {
        ++endCol;
    }
    m_selectionAnchor = QPoint(startCol, cell.y());
    setSelectionRange(m_selectionAnchor, QPoint(endCol, cell.y()));
    copySelectionIfActive();
}

void TerminalScreenItem::selectLineAt(int row)
{
    QString line = lineText(row);
    while (!line.isEmpty() && line.endsWith(QLatin1Char(' '))) {
        line.chop(1);
    }
    if (line.isEmpty()) {
        setSelectionRange(QPoint(0, row), QPoint(0, row), false);
        return;
    }
    m_selectionAnchor = QPoint(0, row);
    setSelectionRange(m_selectionAnchor, QPoint(qMin(m_cols - 1, line.size() - 1), row));
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

    const int rows = qMin(m_screen->rows(), int(height() / m_cellH) + 1);
    const int cols = qMin(m_screen->cols(), int(width() / m_cellW) + 1);

    QFont boldFont = m_font;
    boldFont.setBold(true);
    QFont italicFont = m_font;
    italicFont.setItalic(true);
    QFont boldItalicFont = boldFont;
    boldItalicFont.setItalic(true);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const VtCell cell = m_screen->cellAt(r, c);
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

            if (isCellSelected(r, c)) {
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

    if (m_screen->cursorVisible() && (m_cursorOn || !hasActiveFocus())) {
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
