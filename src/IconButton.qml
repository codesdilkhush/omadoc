import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string iconText: ""
    property string tooltipText: ""
    property bool highlighted: false
    property bool active: false
    property color accentColor: theme.accent
    property int radius: Math.max(6, theme.radius)

    signal clicked()

    implicitWidth: 34
    implicitHeight: 34

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.radius
        color: root.highlighted ? root.accentColor
             : mouseArea.containsPress ? "#2e303c"
             : (mouseArea.containsMouse || root.active) ? "#22242d"
             : "transparent"

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Text {
            anchors.centerIn: parent
            text: root.iconText
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: root.highlighted ? theme.accentForeground
                 : (mouseArea.containsMouse || root.active) ? "#ffffff"
                 : "#a6abb8"
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: mouseArea.containsMouse && tooltipText.length > 0
    ToolTip.delay: 400
    ToolTip.text: root.tooltipText
}
