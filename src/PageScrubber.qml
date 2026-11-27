import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentPage: 0
    property int totalPages: 1
    readonly property bool inputActive: pageInput.activeFocus
    signal jumpToPage(int page)
    signal dismissFocusRequested()

    function commitPageInput() {
        pageInput.commit();
    }

    implicitWidth: contentRow.implicitWidth + 16
    implicitHeight: 40
    radius: 20
    color: "#ee181920"
    border.color: "#282a35"
    border.width: 1

    RowLayout {
        id: contentRow
        anchors.centerIn: parent
        spacing: 6

        // Minus (-) button -> previous page
        Rectangle {
            id: minusBtn
            readonly property bool canStep: root.currentPage > 0
            implicitWidth: 28
            implicitHeight: 28
            radius: 14
            color: !canStep ? "transparent"
                 : minusArea.containsPress ? "#2b2e3d"
                 : minusArea.containsMouse ? "#222533"
                 : "transparent"
            opacity: canStep ? 1.0 : 0.35

            Text {
                anchors.centerIn: parent
                text: "−"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: "#f3f4f6"
            }

            MouseArea {
                id: minusArea
                anchors.fill: parent
                hoverEnabled: minusBtn.canStep
                enabled: minusBtn.canStep
                cursorShape: minusBtn.canStep ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.jumpToPage(Math.max(0, root.currentPage - 1))
            }

            ToolTip.visible: minusArea.containsMouse && minusBtn.canStep
            ToolTip.delay: 400
            ToolTip.text: qsTr("Previous page")
        }

        // Writing section: Go-To page input + total pages
        Rectangle {
            id: inputBox
            Layout.preferredWidth: Math.max(46, pageInput.contentWidth + 18)
            Layout.preferredHeight: 26
            radius: 6
            color: pageInput.activeFocus ? "#12131b" : (inputHover.hovered ? "#202330" : "#151721")
            border.color: pageInput.activeFocus ? theme.accent : (inputHover.hovered ? "#34384c" : "#282b3b")
            border.width: 1

            HoverHandler {
                id: inputHover
            }

            TextInput {
                id: pageInput
                property bool dirty: false
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                text: (root.currentPage + 1).toString()
                font.pixelSize: 12
                font.weight: Font.Bold
                color: "#ffffff"
                selectionColor: theme.accent
                selectedTextColor: theme.accentForeground
                selectByMouse: true
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                clip: true

                function commit() {
                    dirty = false;
                    const clean = text.replace(/[^0-9]/g, "");
                    const val = parseInt(clean) - 1;
                    if (!isNaN(val) && root.totalPages > 0) {
                        root.jumpToPage(Math.max(0, Math.min(val, root.totalPages - 1)));
                    }
                    text = (root.currentPage + 1).toString();
                    root.dismissFocusRequested();
                }

                onTextEdited: dirty = true

                onTextChanged: {
                    if (/[^0-9]/.test(text)) {
                        const digits = text.replace(/[^0-9]/g, "");
                        if (digits !== text) text = digits;
                    }
                }

                onAccepted: commit()

                Keys.onEscapePressed: {
                    dirty = false;
                    text = (root.currentPage + 1).toString();
                    root.dismissFocusRequested();
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        dirty = false;
                        selectAll();
                    } else if (dirty) {
                        commit();
                    } else {
                        text = (root.currentPage + 1).toString();
                    }
                }
            }

            ToolTip.visible: inputHover.hovered && !pageInput.activeFocus
            ToolTip.delay: 400
            ToolTip.text: qsTr("Go to page")
        }

        Text {
            text: "/ " + root.totalPages
            font.pixelSize: 12
            font.weight: Font.Medium
            color: "#8b92a4"
            Layout.rightMargin: 2
        }

        // Plus (+) button -> next page
        Rectangle {
            id: plusBtn
            readonly property bool canStep: root.currentPage < root.totalPages - 1
            implicitWidth: 28
            implicitHeight: 28
            radius: 14
            color: !canStep ? "transparent"
                 : plusArea.containsPress ? "#2b2e3d"
                 : plusArea.containsMouse ? "#222533"
                 : "transparent"
            opacity: canStep ? 1.0 : 0.35

            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: "#f3f4f6"
            }

            MouseArea {
                id: plusArea
                anchors.fill: parent
                hoverEnabled: plusBtn.canStep
                enabled: plusBtn.canStep
                cursorShape: plusBtn.canStep ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.jumpToPage(Math.min(root.totalPages - 1, root.currentPage + 1))
            }

            ToolTip.visible: plusArea.containsMouse && plusBtn.canStep
            ToolTip.delay: 400
            ToolTip.text: qsTr("Next page")
        }
    }

    Connections {
        target: root
        function onCurrentPageChanged() {
            pageInput.dirty = false;
            if (!pageInput.activeFocus) {
                pageInput.text = (root.currentPage + 1).toString();
            }
        }
        function onTotalPagesChanged() {
            pageInput.dirty = false;
            if (!pageInput.activeFocus) {
                pageInput.text = (root.currentPage + 1).toString();
            }
        }
    }
}
