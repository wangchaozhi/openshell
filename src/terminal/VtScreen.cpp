#include "VtScreen.h"

#include <vterm.h>

#include <QString>

#include <algorithm>
#include <cstring>

namespace {

constexpr int kDefaultCols = 80;
constexpr int kDefaultRows = 24;

QColor toQColor(const VTermColor &c, const QColor &fallback)
{
    VTermColor copy = c;
    if (VTERM_COLOR_IS_DEFAULT_FG(&copy) || VTERM_COLOR_IS_DEFAULT_BG(&copy)) {
        return fallback;
    }
    if (VTERM_COLOR_IS_INDEXED(&copy)) {
        // libvterm 提供的索引色暂时不展开 256 色调色板，让 vterm_state_convert_color_to_rgb
        // 在 state 里转完更稳。如果 caller 没转 RGB，这里给出一个温和的近似。
        const uint8_t idx = copy.indexed.idx;
        static const QColor kAnsi[16] = {
            QColor("#1f2937"), QColor("#ef4444"), QColor("#22c55e"), QColor("#eab308"),
            QColor("#3b82f6"), QColor("#d946ef"), QColor("#06b6d4"), QColor("#e2e8f0"),
            QColor("#64748b"), QColor("#f87171"), QColor("#86efac"), QColor("#fde047"),
            QColor("#60a5fa"), QColor("#e879f9"), QColor("#67e8f9"), QColor("#ffffff"),
        };
        if (idx < 16) {
            return kAnsi[idx];
        }
        if (idx >= 232) {
            const int v = 8 + (idx - 232) * 10;
            return QColor(v, v, v);
        }
        if (idx >= 16) {
            const int n = idx - 16;
            const int r = (n / 36) % 6;
            const int g = (n / 6) % 6;
            const int b = n % 6;
            const auto step = [](int x) { return x == 0 ? 0 : 55 + x * 40; };
            return QColor(step(r), step(g), step(b));
        }
        return fallback;
    }
    return QColor(copy.rgb.red, copy.rgb.green, copy.rgb.blue);
}

VtCell makeCellFromRaw(const VTermScreenCell &raw)
{
    VtCell out;
    if (raw.chars[0] == 0) {
        out.text.clear();
    } else {
        out.text.reserve(2);
        for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && raw.chars[i] != 0; ++i) {
            out.text.append(QChar::fromUcs4(raw.chars[i]));
        }
    }
    out.width = qMax<int>(1, raw.width);
    out.bold = raw.attrs.bold;
    out.italic = raw.attrs.italic;
    out.underline = raw.attrs.underline != 0;
    out.reverse = raw.attrs.reverse;
    out.fg = toQColor(raw.fg, QColor(0xe2, 0xe8, 0xf0));
    out.bg = toQColor(raw.bg, QColor(0x02, 0x06, 0x17));
    if (out.reverse) {
        std::swap(out.fg, out.bg);
    }
    if (raw.chars[0] == static_cast<uint32_t>(-1)) {
        out.placeholder = true;
        out.text.clear();
    }
    return out;
}

} // namespace

VtScreen::VtScreen(QObject *parent)
    : QObject(parent)
{
    initialiseVTerm();
}

VtScreen::~VtScreen()
{
    if (m_vt) {
        vterm_free(m_vt);
        m_vt = nullptr;
    }
}

void VtScreen::initialiseVTerm()
{
    m_cols = kDefaultCols;
    m_rows = kDefaultRows;
    m_vt = vterm_new(m_rows, m_cols);
    vterm_set_utf8(m_vt, 1);

    m_screen = vterm_obtain_screen(m_vt);
    static const VTermScreenCallbacks kCallbacks = {
        &VtScreen::sDamage,
        &VtScreen::sMoveRect,
        &VtScreen::sMoveCursor,
        &VtScreen::sSetTermProp,
        &VtScreen::sBell,
        &VtScreen::sResize,
        &VtScreen::sScrollbackPushLine,
        &VtScreen::sScrollbackPopLine,
        &VtScreen::sScrollbackClear, // libvterm >= 0.3
    };
    vterm_screen_set_callbacks(m_screen, &kCallbacks, this);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_reset(m_screen, 1);

    vterm_output_set_callback(m_vt, &VtScreen::sOutput, this);
}

void VtScreen::resize(int cols, int rows)
{
    if (cols <= 0 || rows <= 0) {
        return;
    }
    if (cols == m_cols && rows == m_rows) {
        return;
    }
    m_cols = cols;
    m_rows = rows;
    vterm_set_size(m_vt, rows, cols);
    vterm_screen_flush_damage(m_screen);
    emit sizeChanged();
    emit damaged(QRect(0, 0, m_cols, m_rows));
}

void VtScreen::feed(const QByteArray &data)
{
    if (data.isEmpty() || !m_vt) {
        return;
    }
    vterm_input_write(m_vt, data.constData(), static_cast<size_t>(data.size()));
    vterm_screen_flush_damage(m_screen);
}

QByteArray VtScreen::takePendingOutput()
{
    QByteArray out;
    out.swap(m_pendingOutput);
    return out;
}

void VtScreen::enqueueRaw(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }
    m_pendingOutput.append(data);
    emit outputReady();
}

void VtScreen::clear()
{
    // Keep the active prompt/input line visible after clearing scrollback-like content.
    QString cursorLine;
    const int cursorRow = qBound(0, m_cursorPos.y(), qMax(0, m_rows - 1));
    const int cursorCol = qBound(0, m_cursorPos.x(), qMax(0, m_cols - 1));
    cursorLine.reserve(m_cols);
    for (int c = 0; c < m_cols; ++c) {
        const VtCell cell = cellAt(cursorRow, c);
        if (cell.placeholder) {
            continue;
        }
        if (cell.text.isEmpty()) {
            cursorLine.append(QLatin1Char(' '));
        } else {
            cursorLine.append(cell.text);
        }
    }
    while (cursorLine.size() > cursorCol && cursorLine.endsWith(QLatin1Char(' '))) {
        cursorLine.chop(1);
    }
    while (cursorLine.size() < cursorCol) {
        cursorLine.append(QLatin1Char(' '));
    }

    QByteArray reset = QByteArrayLiteral("\x1b[2J\x1b[H");
    reset.append(cursorLine.toUtf8());
    reset.append(QStringLiteral("\x1b[1;%1H").arg(cursorCol + 1).toUtf8());
    feed(reset);
}

bool VtScreen::sendKey(int qtKey, Qt::KeyboardModifiers modifiers, const QString &text)
{
    if (!m_vt) {
        return false;
    }

    VTermModifier mod = VTERM_MOD_NONE;
    if (modifiers & Qt::ShiftModifier) {
        mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    }
    if (modifiers & Qt::AltModifier) {
        mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);
    }
    if (modifiers & Qt::ControlModifier) {
        mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);
    }

    VTermKey vk = VTERM_KEY_NONE;
    switch (qtKey) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    vk = VTERM_KEY_ENTER; break;
    case Qt::Key_Tab:      vk = VTERM_KEY_TAB; break;
    case Qt::Key_Backspace:vk = VTERM_KEY_BACKSPACE; break;
    case Qt::Key_Escape:   vk = VTERM_KEY_ESCAPE; break;
    case Qt::Key_Up:       vk = VTERM_KEY_UP; break;
    case Qt::Key_Down:     vk = VTERM_KEY_DOWN; break;
    case Qt::Key_Left:     vk = VTERM_KEY_LEFT; break;
    case Qt::Key_Right:    vk = VTERM_KEY_RIGHT; break;
    case Qt::Key_Insert:   vk = VTERM_KEY_INS; break;
    case Qt::Key_Delete:   vk = VTERM_KEY_DEL; break;
    case Qt::Key_Home:     vk = VTERM_KEY_HOME; break;
    case Qt::Key_End:      vk = VTERM_KEY_END; break;
    case Qt::Key_PageUp:   vk = VTERM_KEY_PAGEUP; break;
    case Qt::Key_PageDown: vk = VTERM_KEY_PAGEDOWN; break;
    default:
        if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35) {
            vk = static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + 1 + (qtKey - Qt::Key_F1));
        }
        break;
    }

    if (vk != VTERM_KEY_NONE) {
        vterm_keyboard_key(m_vt, vk, mod);
        if (!m_pendingOutput.isEmpty()) {
            emit outputReady();
        }
        return true;
    }

    // Ctrl+letter / Ctrl+[/\]/^/_/Space：以 qtKey 为准，不能依赖 event->text()，
    // 因为 Qt 在很多平台上 Ctrl 组合的 text 是空的。libvterm 拿到字母 + CTRL
    // 修饰符后自己折叠成 0x01..0x1f 控制字符。
    if (modifiers & Qt::ControlModifier) {
        uint32_t cp = 0;
        if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
            cp = static_cast<uint32_t>('A' + (qtKey - Qt::Key_A));
        } else {
            switch (qtKey) {
            case Qt::Key_Space:        cp = '@'; break;
            case Qt::Key_At:           cp = '@'; break;
            case Qt::Key_BracketLeft:  cp = '['; break;
            case Qt::Key_Backslash:    cp = '\\'; break;
            case Qt::Key_BracketRight: cp = ']'; break;
            case Qt::Key_AsciiCircum:  cp = '^'; break;
            case Qt::Key_Underscore:   cp = '_'; break;
            case Qt::Key_Question:     cp = '?'; break;
            default: break;
            }
        }
        if (cp != 0) {
            // 控制字符需要直接编码成 0x01-0x1f，不通过 vterm_keyboard_unichar
            // Ctrl+A=0x01, Ctrl+B=0x02, ..., Ctrl+C=0x03, ...
            uint8_t ctrl_char = (cp >= 'A' && cp <= 'Z') ? (cp - 'A' + 1) : cp;
            m_pendingOutput.append((char)ctrl_char);
            emit outputReady();
            return true;
        }
    }

    if (!text.isEmpty()) {
        for (const QChar &ch : text) {
            const uint cp = ch.unicode();
            if (cp == 0) {
                continue;
            }
            vterm_keyboard_unichar(m_vt, cp, mod);
        }
        if (!m_pendingOutput.isEmpty()) {
            emit outputReady();
        }
        return true;
    }

    return false;
}

VtCell VtScreen::cellAt(int row, int col) const
{
    if (!m_screen || row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        return VtCell();
    }
    VTermPos pos{row, col};
    VTermScreenCell raw;
    if (vterm_screen_get_cell(m_screen, pos, &raw) == 0) {
        return VtCell();
    }
    return makeCellFromRaw(raw);
}

VtCell VtScreen::cellAtAbsolute(int absRow, int col) const
{
    if (absRow >= 0) {
        return cellAt(absRow, col);
    }
    const int sbSize = static_cast<int>(m_scrollback.size());
    const int sbIdx = sbSize + absRow; // absRow=-1 -> last, absRow=-sbSize -> first
    if (sbIdx < 0 || sbIdx >= sbSize) {
        return VtCell();
    }
    const QVector<VtCell> &row = m_scrollback[sbIdx];
    if (col < 0 || col >= row.size()) {
        return VtCell();
    }
    return row[col];
}

void VtScreen::setScrollbackLimit(int limit)
{
    m_scrollbackLimit = qMax(0, limit);
    bool changed = false;
    while (static_cast<int>(m_scrollback.size()) > m_scrollbackLimit) {
        m_scrollback.pop_front();
        changed = true;
    }
    if (changed) {
        emit scrollbackCleared();
    }
}

void VtScreen::clearScrollback()
{
    if (m_scrollback.empty()) {
        return;
    }
    m_scrollback.clear();
    emit scrollbackCleared();
}

QString VtScreen::plainTextSnapshot() const
{
    QString out;
    out.reserve(m_rows * (m_cols + 1));
    for (int r = 0; r < m_rows; ++r) {
        QString line;
        line.reserve(m_cols);
        for (int c = 0; c < m_cols; ++c) {
            const VtCell cell = cellAt(r, c);
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
        out.append(line);
        if (r + 1 < m_rows) {
            out.append(QLatin1Char('\n'));
        }
    }
    return out;
}

// ---------- libvterm callbacks ----------

int VtScreen::sDamage(VTermRect rect, void *user)
{
    return static_cast<VtScreen *>(user)->handleDamage(rect.start_row, rect.start_col,
                                                       rect.end_row, rect.end_col);
}

int VtScreen::sMoveRect(VTermRect dest, VTermRect /*src*/, void *user)
{
    // libvterm 已经把移动的内容写到 screen 缓冲，我们只需把目标 rect 当作脏区。
    return static_cast<VtScreen *>(user)->handleDamage(dest.start_row, dest.start_col,
                                                       dest.end_row, dest.end_col);
}

int VtScreen::sMoveCursor(VTermPos pos, VTermPos /*oldpos*/, int visible, void *user)
{
    return static_cast<VtScreen *>(user)->handleMoveCursor(pos.row, pos.col, visible);
}

int VtScreen::sSetTermProp(VTermProp prop, VTermValue *val, void *user)
{
    return static_cast<VtScreen *>(user)->handleSetTermProp(prop, val);
}

int VtScreen::handleSetTermProp(VTermProp prop, VTermValue *val)
{
    if (prop == VTERM_PROP_TITLE || prop == VTERM_PROP_ICONNAME) {
        const QString next = QString::fromUtf8(val->string.str, static_cast<int>(val->string.len));
        if (next != m_title) {
            m_title = next;
            emit titleChanged(m_title);
        }
        return 1;
    }
    if (prop == VTERM_PROP_CURSORVISIBLE) {
        const bool vis = val->boolean != 0;
        if (vis != m_cursorVisible) {
            m_cursorVisible = vis;
            emit cursorMoved();
        }
        return 1;
    }
    return 1;
}

int VtScreen::sBell(void *user)
{
    return static_cast<VtScreen *>(user)->handleBell();
}

int VtScreen::sResize(int rows, int cols, void *user)
{
    return static_cast<VtScreen *>(user)->handleResize(rows, cols);
}

void VtScreen::sOutput(const char *s, size_t len, void *user)
{
    static_cast<VtScreen *>(user)->appendOutput(s, len);
}

int VtScreen::sScrollbackPushLine(int cols, const VTermScreenCell *cells, void *user)
{
    return static_cast<VtScreen *>(user)->handleScrollbackPush(cols, cells);
}

int VtScreen::sScrollbackPopLine(int /*cols*/, VTermScreenCell * /*cells*/, void * /*user*/)
{
    // 终端变高时 libvterm 会通过该回调向回滚区借行回填顶部，由于 VtCell 是
    // 渲染快照、无法无损还原成 VTermScreenCell，这里直接返回 0 表示没有可借
    // 的行；libvterm 会用空白行填充新出现的顶部，效果可接受。
    return 0;
}

int VtScreen::sScrollbackClear(void *user)
{
    return static_cast<VtScreen *>(user)->handleScrollbackClear();
}

int VtScreen::handleScrollbackPush(int cols, const VTermScreenCell *cells)
{
    if (!cells || cols <= 0 || m_scrollbackLimit <= 0) {
        return 1;
    }
    QVector<VtCell> row;
    row.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        row.append(makeCellFromRaw(cells[c]));
    }
    m_scrollback.push_back(std::move(row));
    while (static_cast<int>(m_scrollback.size()) > m_scrollbackLimit) {
        m_scrollback.pop_front();
    }
    emit scrollbackPushed(1);
    return 1;
}

int VtScreen::handleScrollbackClear()
{
    if (m_scrollback.empty()) {
        return 1;
    }
    m_scrollback.clear();
    emit scrollbackCleared();
    return 1;
}

int VtScreen::handleDamage(int startRow, int startCol, int endRow, int endCol)
{
    const int x = qMax(0, startCol);
    const int y = qMax(0, startRow);
    const int w = qMax(0, endCol - startCol);
    const int h = qMax(0, endRow - startRow);
    if (w > 0 && h > 0) {
        emit damaged(QRect(x, y, w, h));
    }
    return 1;
}

int VtScreen::handleMoveCursor(int row, int col, int visible)
{
    const QPoint p(col, row);
    const bool vis = visible != 0;
    if (p != m_cursorPos || vis != m_cursorVisible) {
        m_cursorPos = p;
        m_cursorVisible = vis;
        emit cursorMoved();
    }
    return 1;
}

int VtScreen::handleBell()
{
    emit bellRang();
    return 1;
}

int VtScreen::handleResize(int rows, int cols)
{
    if (cols == m_cols && rows == m_rows) {
        return 1;
    }
    m_cols = cols;
    m_rows = rows;
    emit sizeChanged();
    emit damaged(QRect(0, 0, m_cols, m_rows));
    return 1;
}

void VtScreen::appendOutput(const char *s, size_t len)
{
    if (len == 0) {
        return;
    }
    m_pendingOutput.append(s, static_cast<int>(len));
    emit outputReady();
}
