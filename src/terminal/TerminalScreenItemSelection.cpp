#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <utility>

// TerminalScreenItem 的选区部分：拖选 / 选词 / 选行 / 取选中文本 / 复制。

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
