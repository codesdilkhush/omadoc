import QtQuick

Rectangle {
    id: root
    property alias text: label.text
    property bool isAccent: false

    implicitWidth: Math.max(22, label.implicitWidth + 10)
    implicitHeight: 22
    radius: 4
    color: isAccent ? theme.accent : "#222533"
    border.color: isAccent ? theme.accent : "#33384c"
    border.width: 1

    Text {
        id: label
        anchors.centerIn: parent
        font.pixelSize: 11
        font.weight: Font.DemiBold
        color: root.isAccent ? theme.accentForeground : "#d1d5db"
    }
}
