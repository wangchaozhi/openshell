#include "TerminalScreenItem.h"

#include "VtScreen.h"

#include <QList>
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

bool TerminalScreenItem::findText(const QString &text, bool backwards, bool restart)
{
    if (!m_screen || text.isEmpty()) {
        return false;
    }
    const int firstRow = -m_screen->scrollbackSize();
    const int lastRow = m_rows - 1;
    const int rowCount = lastRow - firstRow + 1;
    if (rowCount <= 0) {
        return false;
    }
    const bool fromSelection = !restart && m_selectionActive;
    const QPoint start = fromSelection ? normalizedSelection().first
                                        : QPoint(0, backwards ? lastRow : -m_scrollOffset);
    const int direction = backwards ? -1 : 1;
    int row = qBound(firstRow, start.y(), lastRow);
    for (int visited = 0; visited < rowCount + (fromSelection ? 1 : 0); ++visited) {
        QString line;
        QList<int> columns;
        for (int col = 0; col < m_cols; ++col) {
            const VtCell cell = m_screen->cellAtAbsolute(row, col);
            if (cell.placeholder) {
                continue;
            }
            const QString value = cell.text.isEmpty() ? QStringLiteral(" ") : cell.text;
            line += value;
            for (qsizetype i = 0; i < value.size(); ++i) {
                columns.append(col);
            }
        }
        qsizetype from = backwards ? line.size() - 1 : 0;
        if (fromSelection && visited == 0) {
            if (backwards) {
                while (from >= 0 && columns.at(from) >= start.x()) {
                    --from;
                }
            } else {
                while (from < columns.size() && columns.at(from) <= start.x()) {
                    ++from;
                }
            }
        }
        if (from >= 0 && from < columns.size()) {
            const qsizetype match = backwards ? line.lastIndexOf(text, from, Qt::CaseInsensitive)
                                              : line.indexOf(text, from, Qt::CaseInsensitive);
            if (match >= 0) {
                const int left = columns.at(match);
                const int last = columns.at(match + text.size() - 1);
                const int right = qMin(m_cols - 1, last + m_screen->cellAtAbsolute(row, last).width - 1);
                m_selecting = false;
                setSelectionRange(QPoint(left, row), QPoint(right, row));
                if (row < -m_scrollOffset || row >= m_rows - m_scrollOffset) {
                    setScrollOffset(-row);
                }
                return true;
            }
        }
        row += direction;
        if (row < firstRow || row > lastRow) {
            row = backwards ? lastRow : firstRow;
        }
    }
    return false;
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
