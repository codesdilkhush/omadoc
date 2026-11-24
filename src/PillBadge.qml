import QtQuick

Rectangle {
    id: root
    property alias text: label.text
    property bool isAccent: false

    implicitWidth: label.implicitWidth + 14
    implicitHeight: 24
    radius: 12
    color: isAccent ? theme.accent : "#20222a"

    Text {
        id: label
        anchors.centerIn: parent
        font.pixelSize: 11
        font.weight: Font.Medium
        color: root.isAccent ? theme.accentForeground : "#b8bcc8"
    }
}
