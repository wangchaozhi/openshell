import QtQuick

// 流动虚线边框：长按某个面板进入“可拖出”状态时，沿四周绘制一圈
// 不断流动的虚线（俗称“蚂蚁线”），提示用户现在可以把它拖成独立窗口。
Item {
    id: ants

    // 外部把它绑定到“正在准备拖出”的状态。
    property bool active: false
    property color strokeColor: "#38bdf8"
    property color fillTint: "#1438bdf8"
    property real lineWidth: 2
    property real cornerRadius: 6
    property real dashLength: 7
    property real gapLength: 5

    readonly property real dashCycle: dashLength + gapLength

    // 不拦截鼠标事件，仅作视觉提示，事件照常落到下方的内容上。
    visible: opacity > 0
    opacity: active ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    // 进入状态时四周透出一点淡淡的高亮，强化“框在变化”的感觉。
    Rectangle {
        anchors.fill: parent
        radius: ants.cornerRadius
        color: ants.fillTint
        border.width: 0
    }

    // 虚线相位，循环推进即形成流动效果。
    property real phase: 0
    NumberAnimation on phase {
        running: ants.visible
        from: 0
        to: ants.dashCycle
        duration: 420
        loops: Animation.Infinite
    }
    onPhaseChanged: borderCanvas.requestPaint()
    onWidthChanged: borderCanvas.requestPaint()
    onHeightChanged: borderCanvas.requestPaint()
    onVisibleChanged: if (visible) borderCanvas.requestPaint()

    Canvas {
        id: borderCanvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (width <= 0 || height <= 0) {
                return
            }
            const inset = ants.lineWidth / 2 + 0.5
            const r = Math.max(0, Math.min(ants.cornerRadius,
                                           (Math.min(width, height) - 2 * inset) / 2))
            const right = width - inset
            const bottom = height - inset

            ctx.strokeStyle = ants.strokeColor
            ctx.lineWidth = ants.lineWidth
            ctx.lineCap = "butt"
            ctx.setLineDash([ants.dashLength, ants.gapLength])
            ctx.lineDashOffset = -ants.phase

            ctx.beginPath()
            ctx.moveTo(inset + r, inset)
            ctx.lineTo(right - r, inset)
            ctx.arcTo(right, inset, right, inset + r, r)
            ctx.lineTo(right, bottom - r)
            ctx.arcTo(right, bottom, right - r, bottom, r)
            ctx.lineTo(inset + r, bottom)
            ctx.arcTo(inset, bottom, inset, bottom - r, r)
            ctx.lineTo(inset, inset + r)
            ctx.arcTo(inset, inset, inset + r, inset, r)
            ctx.closePath()
            ctx.stroke()
        }
    }
}
