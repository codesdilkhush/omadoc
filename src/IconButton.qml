import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string iconSource: ""
    property string text: ""
    property string tooltipText: ""
    property bool highlighted: false
    property bool active: false
    property bool enabled: true
    property int iconSize: 16
    property int customRadius: Math.max(6, theme.radius)

    signal clicked()

    implicitWidth: text.length > 0 ? (label.implicitWidth + (iconSource.length > 0 ? 30 : 18)) : 32
    implicitHeight: 32
    opacity: root.enabled ? 1.0 : 0.4

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.customRadius
        color: root.highlighted ? theme.accent
             : mouseArea.containsPress ? "#282a36"
             : (mouseArea.containsMouse || root.active) ? "#20232e"
             : "transparent"

        border.color: root.active && !root.highlighted ? theme.accent : "transparent"
        border.width: root.active && !root.highlighted ? 1 : 0

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Row {
            anchors.centerIn: parent
            spacing: 6

            Image {
                id: iconImg
                source: root.iconSource
                width: root.iconSize
                height: root.iconSize
                sourceSize.width: root.iconSize * 2
                sourceSize.height: root.iconSize * 2
                visible: root.iconSource.length > 0
                smooth: true
                mipmap: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                id: label
                text: root.text
                visible: text.length > 0
                font.pixelSize: 12
                font.weight: Font.SemiBold
                color: root.highlighted ? theme.accentForeground : "#f3f4f6"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (root.enabled) root.clicked()
        }
    }

    ToolTip.visible: mouseArea.containsMouse && tooltipText.length > 0
    ToolTip.delay: 400
    ToolTip.text: root.tooltipText
}
