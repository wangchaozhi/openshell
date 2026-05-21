.pragma library

// 纯展示/格式化工具函数：无状态、不依赖 QML 上下文,
// 从 FileBrowser.qml 抽出便于复用与测试。

// 字节数 -> 人类可读体积字符串。
function formatFileSize(bytes) {
    const strVal = String(bytes).trim()
    if (!strVal || strVal === "0" || strVal === "--" || strVal === "undefined") {
        return "--"
    }
    const size = Number(strVal)
    if (isNaN(size) || size < 0) {
        return strVal
    }
    if (size === 0) {
        return "0 B"
    }
    const units = ["B", "KB", "MB", "GB", "TB"]
    let unitIndex = 0
    let value = size
    while (value >= 1024 && unitIndex < units.length - 1) {
        value /= 1024
        unitIndex++
    }
    if (unitIndex === 0) {
        return Math.floor(value) + " " + units[unitIndex]
    } else {
        return value.toFixed(1) + " " + units[unitIndex]
    }
}

// 每秒字节数 -> 速度字符串。
function formatSpeed(bytesPerSecond) {
    const speed = Number(bytesPerSecond) || 0
    return formatFileSize(speed) + "/s"
}

// 取路径的最后一段(文件/目录名)。
function shortPath(path) {
    if (!path || path.length === 0) {
        return ""
    }
    const normalized = String(path).replace(/\\/g, "/")
    const parts = normalized.split("/")
    return parts.length > 0 && parts[parts.length - 1].length > 0
           ? parts[parts.length - 1]
           : normalized
}

// 八进制权限 -> 符号权限字符串("644" -> "-rw-r--r--")。
function permissionsToSymbolic(octal, isDir) {
    if (!octal || octal.length === 0) {
        return isDir ? "d---------" : "----------"
    }
    const normalized = octal.slice(-3)
    const owner = parseInt(normalized.charAt(0))
    const group = parseInt(normalized.charAt(1))
    const other = parseInt(normalized.charAt(2))

    const toRwx = (val) => (val & 4 ? "r" : "-") + (val & 2 ? "w" : "-") + (val & 1 ? "x" : "-")
    const prefix = isDir ? "d" : "-"
    return prefix + toRwx(owner) + toRwx(group) + toRwx(other)
}

// 传输任务完成百分比(0-100)。
function transferPercent(task) {
    if (!task || !task.total || task.total <= 0) {
        return 0
    }
    return Math.max(0, Math.min(100, Math.round(task.done * 100 / task.total)))
}

// 已完成下载任务可打开的本地路径,否则返回空串。
function downloadOpenPath(task) {
    if (!task || task.operation !== "download" || task.status !== "done") {
        return ""
    }
    return task.localPath && task.localPath.length > 0 ? task.localPath : (task.message || "")
}

// 按文件类型(目录/压缩包/图像/音频/可执行)返回名称配色。
function entryNameColor(entry) {
    if (!entry) {
        return "#d1d5db"
    }
    const name = String(entry.name || "")
    if (entry.isDir) {
        return "#3b82f6"
    }
    if (name.match(/\.(tar|tgz|arc|arj|taz|lha|lz4|lzh|lzma|tlz|txz|tzo|t7z|zip|z|dz|gz|lrz|lz|lzo|xz|zst|tzst|bz2|bz|tbz|tbz2|tz|deb|rpm|jar|war|ear|sar|rar|alz|ace|zoo|cpio|7z|rz|cab|wim|swm|dwm|esd)$/i)) {
        return "#ef4444"
    }
    if (name.match(/\.(jpg|jpeg|mjpg|mjpeg|gif|bmp|pbm|pgm|ppm|tga|xbm|xpm|tif|tiff|png|svg|svgz|mng|pcx|mov|mpg|mpeg|m2v|mkv|webm|webp|ogm|mp4|m4v|mp4v|vob|qt|nuv|wmv|asf|rm|rmvb|flc|avi|fli|flv|gl|dl|xcf|xwd|yuv|cgm|emf|ogv|ogx)$/i)) {
        return "#d946ef"
    }
    if (name.match(/\.(aac|au|flac|m4a|mid|midi|mka|mp3|mpc|ogg|ra|wav|oga|opus|spx|xspf)$/i)) {
        return "#06b6d4"
    }
    if (permissionsToSymbolic(String(entry.permissions || ""), false).indexOf("x") >= 0) {
        return "#22c55e"
    }
    return "#d1d5db"
}

// 列的固定像素宽度;0 表示可伸缩列。
function columnFixedWidth(colId) {
    switch (colId) {
    case "size": return 78
    case "owner": return 100
    case "permissions": return 78
    case "modified": return 118
    }
    return 0
}

// 该列内容是否右对齐。
function columnAlignsRight(colId) {
    return colId === "size" || colId === "permissions"
}

// 排序文件条目:目录始终排在前面,按指定列升/降序。
function sortEntries(entries, column, asc) {
    const arr = entries.slice()
    arr.sort(function(a, b) {
        // 目录始终排在前面
        if (a.isDir !== b.isDir) {
            return a.isDir ? -1 : 1
        }
        let va = a[column] || ""
        let vb = b[column] || ""
        if (column === "size") {
            const na = parseInt(va) || 0
            const nb = parseInt(vb) || 0
            return asc ? na - nb : nb - na
        }
        va = String(va).toLowerCase()
        vb = String(vb).toLowerCase()
        if (va < vb) return asc ? -1 : 1
        if (va > vb) return asc ? 1 : -1
        return 0
    })
    return arr
}

// 列标题旁的排序指示箭头。
function sortIcon(col, currentCol, asc) {
    if (col !== currentCol) return ""
    return asc ? " ^" : " v"
}
