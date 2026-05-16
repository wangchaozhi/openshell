#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QDebug>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

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

void TerminalScreenItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    emitDesiredGrid();
}

void TerminalScreenItem::keyPressEvent(QKeyEvent *event)
{
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
    QQuickPaintedItem::mousePressEvent(event);
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
