import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property int currentPage: 0
    property int totalPages: 1
    signal jumpToPage(int page)

    implicitWidth: 320
    implicitHeight: 40
    radius: 20
    color: "#181920ee"
    border.color: "#282a35"
    border.width: 1

    Row {
        anchors.centerIn: parent
        spacing: 12

        Text {
            text: "1"
            font.pixelSize: 11
            color: "#6b7280"
            anchors.verticalCenter: parent.verticalCenter
        }

        Slider {
            id: slider
            from: 0
            to: Math.max(0, root.totalPages - 1)
            stepSize: 1
            value: root.currentPage
            implicitWidth: 200

            onMoved: {
                root.jumpToPage(Math.round(value))
            }

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: slider.availableWidth
                height: implicitHeight
                radius: 2
                color: "#282a36"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    color: theme.accent
                    radius: 2
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 16
                implicitHeight: 16
                radius: 8
                color: slider.pressed ? "#ffffff" : theme.accent
                border.color: "#101116"
                border.width: 2
            }
        }

        Text {
            text: root.totalPages.toString()
            font.pixelSize: 11
            color: "#6b7280"
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
