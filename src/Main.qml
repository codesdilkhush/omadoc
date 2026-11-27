import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 780
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: backend.hasDocument ? backend.title + " — Omadoc" : "Omadoc"
    color: "#0d0e12"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius <= 0 ? Material.NotRounded
                         : (theme.radius <= 4 ? Material.ExtraSmallScale
                         : (theme.radius <= 8 ? Material.SmallScale
                         : Material.MediumScale))

    Item {
        id: windowFocusTarget
        focus: true
    }

    // Transparent overlay active ONLY when editing zoomInput or bottom page input, to reliably commit and dismiss on any outside click
    MouseArea {
        id: focusDismissOverlay
        anchors.fill: parent
        z: 40
        visible: (typeof zoomInput !== "undefined" && zoomInput.activeFocus) ||
                 (typeof pageScrubber !== "undefined" && pageScrubber.inputActive)
        onPressed: (mouse) => {
            if (typeof zoomInput !== "undefined" && zoomInput.activeFocus) {
                const pt = mapToItem(zoomBox, mouse.x, mouse.y);
                if (pt.x >= 0 && pt.y >= 0 && pt.x <= zoomBox.width && pt.y <= zoomBox.height) {
                    mouse.accepted = false;
                    return;
                }
            }
            if (typeof pageScrubber !== "undefined" && pageScrubber.inputActive) {
                const pt = mapToItem(pageScrubber, mouse.x, mouse.y);
                if (pt.x >= 0 && pt.y >= 0 && pt.x <= pageScrubber.width && pt.y <= pageScrubber.height) {
                    mouse.accepted = false;
                    return;
                }
            }
        }
        onClicked: {
            if (zoomInput.activeFocus) zoomInput.commitZoom();
            if (pageScrubber.inputActive) pageScrubber.commitPageInput();
        }
    }

    property string activeAnnotationColor: "#ffe082"

    // Check if user is typing in a text input field to avoid intercepting keystrokes
    readonly property bool isInputActive: (typeof zoomInput !== "undefined" && zoomInput.activeFocus) ||
                                          (typeof pageScrubber !== "undefined" && pageScrubber.inputActive) ||
                                          (typeof searchField !== "undefined" && searchField.activeFocus)

    readonly property var currentPageAnnots: {
        backend.currentPage;
        backend.rotation;
        backend.annotations;
        return backend.pageAnnotations(backend.currentPage);
    }

    readonly property var currentSearchHighlights: {
        backend.currentPage;
        backend.rotation;
        backend.searchQuery;
        backend.searchResultsCount;
        return backend.searchHighlights(backend.currentPage);
    }

    // Application Shortcuts
    Shortcut { sequence: "Q"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "?"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.toggleHelp() }
    Shortcut { sequence: "O"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequences: [StandardKey.Save]; context: Qt.ApplicationShortcut; onActivated: backend.saveDocument() }
    Shortcut { sequences: [StandardKey.SaveAs]; context: Qt.ApplicationShortcut; onActivated: backend.saveAsDocument() }
    Shortcut { sequence: "D"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.toggleDarkMode() }
    Shortcut { sequence: "R"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.rotateClockwise() }
    Shortcut { sequence: "Shift+R"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.rotateCounterClockwise() }
    Shortcut { sequence: "H"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.activeTool = (backend.activeTool === 1 ? 0 : 1) }
    Shortcut { sequence: "V"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.activeTool = 0 }
    Shortcut { sequence: "T"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.toggleToc() }
    Shortcut { sequence: "W"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.fitToWidth(flickable.width) }
    Shortcut { sequence: "P"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.fitToPage(flickable.width, flickable.height) }
    Shortcut { sequence: "J"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "K"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Right"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "Left"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Space"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "Shift+Space"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Down"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: flickable.flick(0, -600) }
    Shortcut { sequence: "Up"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: flickable.flick(0, 600) }
    Shortcut { sequence: "PageDown"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "PageUp"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Home"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(0) }
    Shortcut { sequence: "End"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(backend.pageCount - 1) }
    Shortcut { sequence: "+"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "="; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "-"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "0"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.resetZoom() }
    Shortcut {
        sequence: "/"
        enabled: !isInputActive
        context: Qt.ApplicationShortcut
        onActivated: {
            floatingSearch.openSearch();
        }
    }
    Shortcut { sequence: "N"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.nextSearchResult() }
    Shortcut { sequence: "Shift+N"; enabled: !isInputActive; context: Qt.ApplicationShortcut; onActivated: backend.prevSearchResult() }
    Shortcut {
        sequence: "Escape"
        enabled: !(typeof zoomInput !== "undefined" && zoomInput.activeFocus) &&
                 !(typeof pageScrubber !== "undefined" && pageScrubber.inputActive)
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.helpVisible) backend.helpVisible = false;
            else if (floatingSearch.visible) floatingSearch.closeSearch();
            else if (backend.activeTool !== 0) backend.activeTool = 0;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Streamlined Header Toolbar (Grouped)
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            height: 48
            color: "#12131a"
            z: 50

            MouseArea {
                anchors.fill: parent
                z: -1
                onClicked: {
                    if (zoomInput.activeFocus) {
                        zoomInput.commitZoom();
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#1e202a"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // Group 1: Document Context & Sidebar
                RowLayout {
                    spacing: 8
                    Layout.maximumWidth: 320

                    IconButton {
                        iconSource: "qrc:/icons/sidebar.svg"
                        tooltipText: qsTr("Table of Contents (T)")
                        active: backend.tocVisible
                        visible: backend.hasDocument && backend.toc.length > 0
                        onClicked: backend.toggleToc()
                    }

                    Image {
                        source: "qrc:/icons/document.svg"
                        width: 16
                        height: 16
                        visible: backend.hasDocument
                    }

                    Text {
                        text: backend.hasDocument ? backend.title : "Omadoc"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: "#f3f4f6"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Item { Layout.fillWidth: true } // Spacer

                // Group 2: Center Unified Navigation & View Island
                Rectangle {
                    visible: backend.hasDocument
                    implicitHeight: 34
                    implicitWidth: navIslandRow.implicitWidth + 8
                    radius: 17
                    color: "#191b24"
                    border.color: "#272a38"
                    border.width: 1

                    RowLayout {
                        id: navIslandRow
                        anchors.centerIn: parent
                        spacing: 2

                        // Zoom Controls
                        IconButton {
                            iconSource: "qrc:/icons/zoom-out.svg"
                            iconSize: 13
                            tooltipText: qsTr("Zoom Out (-)")
                            onClicked: backend.zoomOut()
                        }

                        Rectangle {
                            id: zoomBox
                            Layout.preferredWidth: 54
                            Layout.preferredHeight: 24
                            radius: 4
                            color: zoomInput.activeFocus ? "#181a24" : (zoomHover.hovered ? "#1c1e2a" : "transparent")
                            border.color: zoomInput.activeFocus ? theme.accent : (zoomHover.hovered ? "#2f3346" : "#222533")
                            border.width: 1

                            HoverHandler {
                                id: zoomHover
                            }

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 1

                                TextInput {
                                    id: zoomInput
                                    property bool dirty: false
                                    text: Math.round(backend.zoom * 100).toString()
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: zoomInput.activeFocus ? "#ffffff" : "#b0b5c4"
                                    selectionColor: theme.accent
                                    selectedTextColor: theme.accentForeground
                                    selectByMouse: true
                                    activeFocusOnTab: true
                                    horizontalAlignment: TextInput.AlignRight
                                    verticalAlignment: TextInput.AlignVCenter
                                    Layout.preferredWidth: Math.max(22, contentWidth)
                                    clip: true

                                    function commitZoom() {
                                        dirty = false;
                                        var clean = text.replace(/[^0-9]/g, "");
                                        var val = parseInt(clean);
                                        if (!isNaN(val) && val >= 10 && val <= 1000) {
                                            backend.setZoom(val / 100.0);
                                        }
                                        text = Math.round(backend.zoom * 100).toString();
                                        windowFocusTarget.forceActiveFocus();
                                    }

                                    onTextEdited: dirty = true
                                    onAccepted: commitZoom()

                                    Keys.onEscapePressed: {
                                        dirty = false;
                                        text = Math.round(backend.zoom * 100).toString();
                                        windowFocusTarget.forceActiveFocus();
                                    }

                                    onActiveFocusChanged: {
                                        if (activeFocus) {
                                            dirty = false;
                                            selectAll();
                                        } else if (dirty) {
                                            commitZoom();
                                        } else {
                                            text = Math.round(backend.zoom * 100).toString();
                                        }
                                    }

                                    onTextChanged: {
                                        if (text.indexOf("%") !== -1 || /[^0-9]/.test(text)) {
                                            var digits = text.replace(/[^0-9]/g, "");
                                            if (digits !== text) text = digits;
                                        }
                                    }
                                }

                                Text {
                                    text: "%"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    color: zoomInput.activeFocus ? theme.accent : "#717789"
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Connections {
                                target: backend
                                function onZoomChanged() {
                                    zoomInput.dirty = false;
                                    if (!zoomInput.activeFocus) {
                                        zoomInput.text = Math.round(backend.zoom * 100).toString();
                                    }
                                }
                            }

                            ToolTip.visible: zoomHover.hovered && !zoomInput.activeFocus
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Zoom percentage (click to edit)")
                        }

                        IconButton {
                            iconSource: "qrc:/icons/zoom-in.svg"
                            iconSize: 13
                            tooltipText: qsTr("Zoom In (+)")
                            onClicked: backend.zoomIn()
                        }

                        // Divider Line
                        Rectangle {
                            width: 1
                            height: 16
                            color: "#272a38"
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                        }

                        // Unified Fit Toggle (Fit to Width / Fit to Page)
                        IconButton {
                            iconSource: backend.fitMode === 1 ? "qrc:/icons/fit-page.svg" : "qrc:/icons/fit-width.svg"
                            iconSize: 14
                            active: backend.fitMode !== 0
                            tooltipText: backend.fitMode === 1 ? qsTr("Fit to Page (P)") : qsTr("Fit to Width (W)")
                            onClicked: {
                                if (backend.fitMode === 1) {
                                    backend.fitToPage(flickable.width, flickable.height);
                                } else {
                                    backend.fitToWidth(flickable.width);
                                }
                            }
                        }

                        // Rotate Anti-Clockwise
                        IconButton {
                            iconSource: "qrc:/icons/rotate-left.svg"
                            iconSize: 14
                            tooltipText: qsTr("Rotate Anti-Clockwise (Shift+R)")
                            onClicked: backend.rotateCounterClockwise()
                        }

                        // Rotate Clockwise
                        IconButton {
                            iconSource: "qrc:/icons/rotate.svg"
                            iconSize: 14
                            tooltipText: qsTr("Rotate Clockwise (R)")
                            onClicked: backend.rotateClockwise()
                        }

                        // Divider Line
                        Rectangle {
                            width: 1
                            height: 16
                            color: "#272a38"
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                        }

                        // Pan / Select Tool
                        IconButton {
                            iconSource: "qrc:/icons/pointer.svg"
                            iconSize: 13
                            active: backend.activeTool === 0
                            tooltipText: qsTr("Pan / Select (V)")
                            onClicked: backend.activeTool = 0
                        }

                        // Highlighter Tool
                        IconButton {
                            iconSource: "qrc:/icons/highlighter.svg"
                            iconSize: 14
                            active: backend.activeTool === 1
                            tooltipText: qsTr("Highlight Area (H)")
                            onClicked: backend.activeTool = (backend.activeTool === 1 ? 0 : 1)
                        }

                        // Color Presets for Active Tool
                        Row {
                            visible: backend.activeTool !== 0
                            spacing: 4
                            Layout.alignment: Qt.AlignVCenter
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4

                            Repeater {
                                model: ["#ffe082", "#80deea", "#81c784", "#ff8a65", "#ce93d8"]
                                delegate: Rectangle {
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: modelData
                                    border.color: win.activeAnnotationColor === modelData ? "#ffffff" : "#40000000"
                                    border.width: win.activeAnnotationColor === modelData ? 2 : 1

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.activeAnnotationColor = modelData
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true } // Spacer

                // Group 3: Tools & App Actions
                RowLayout {
                    spacing: 4

                    // Search Trigger
                    IconButton {
                        iconSource: "qrc:/icons/search.svg"
                        visible: backend.hasDocument
                        active: floatingSearch.visible
                        tooltipText: qsTr("Find in Document (/)")
                        onClicked: {
                            if (floatingSearch.visible) floatingSearch.closeSearch();
                            else floatingSearch.openSearch();
                        }
                    }

                    // Night Mode
                    IconButton {
                        iconSource: backend.darkMode ? "qrc:/icons/sun.svg" : "qrc:/icons/moon.svg"
                        visible: backend.hasDocument
                        active: backend.darkMode
                        tooltipText: qsTr("Night Reading Mode (D)")
                        onClicked: backend.toggleDarkMode()
                    }

                    // More Menu Button
                    IconButton {
                        id: moreBtn
                        iconSource: "qrc:/icons/dots-vertical.svg"
                        tooltipText: qsTr("More Options")
                        onClicked: moreMenu.open()

                        component ThemedMenuItem: MenuItem {
                            id: itemCtrl
                            implicitWidth: 232
                            implicitHeight: 34
                            leftPadding: 12
                            rightPadding: 12

                            contentItem: Text {
                                text: itemCtrl.text
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: !itemCtrl.enabled ? "#555b6e"
                                     : itemCtrl.highlighted ? "#ffffff"
                                     : "#e5e7eb"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }

                            background: Rectangle {
                                radius: Math.max(5, Math.min(theme.radius, 8))
                                color: !itemCtrl.enabled ? "transparent"
                                     : itemCtrl.down ? "#282a36"
                                     : itemCtrl.highlighted ? "#20232e"
                                     : "transparent"
                                border.color: itemCtrl.highlighted && itemCtrl.enabled ? theme.accent : "transparent"
                                border.width: itemCtrl.highlighted && itemCtrl.enabled ? 1 : 0
                            }
                        }

                        component ThemedMenuSeparator: MenuSeparator {
                            topPadding: 5
                            bottomPadding: 5
                            leftPadding: 8
                            rightPadding: 8
                            background: Item {}
                            contentItem: Rectangle {
                                implicitWidth: 216
                                implicitHeight: 1
                                color: "#252838"
                            }
                        }

                        Menu {
                            id: moreMenu
                            y: moreBtn.height + 4
                            width: 244
                            padding: 6
                            Material.theme: Material.Dark

                            background: Rectangle {
                                implicitWidth: 244
                                radius: Math.max(8, theme.radius)
                                color: "#161823"
                                border.color: "#2a2d3e"
                                border.width: 1
                            }

                            ThemedMenuItem {
                                text: qsTr("Open Document (Ctrl+O)")
                                onTriggered: backend.openPortal()
                            }
                            ThemedMenuItem {
                                text: qsTr("Save (Ctrl+S)")
                                enabled: backend.hasDocument
                                onTriggered: backend.saveDocument()
                            }
                            ThemedMenuItem {
                                text: qsTr("Save As... (Ctrl+Shift+S)")
                                enabled: backend.hasDocument
                                onTriggered: backend.saveAsDocument()
                            }
                            ThemedMenuSeparator {}
                            ThemedMenuItem {
                                text: qsTr("Clear Page Annotations")
                                enabled: backend.hasDocument && win.currentPageAnnots.length > 0
                                onTriggered: backend.clearAnnotationsForPage(backend.currentPage)
                            }
                            ThemedMenuItem {
                                text: qsTr("Clear All Annotations")
                                enabled: backend.hasDocument && backend.annotationCount > 0
                                onTriggered: backend.clearAllAnnotations()
                            }
                            ThemedMenuSeparator {}
                            ThemedMenuItem {
                                text: qsTr("Keyboard Shortcuts (?)")
                                onTriggered: backend.toggleHelp()
                            }
                            ThemedMenuItem {
                                text: qsTr("Close Document")
                                enabled: backend.hasDocument
                                onTriggered: backend.closeDocument()
                            }
                            ThemedMenuItem {
                                text: qsTr("Quit (Q)")
                                onTriggered: Qt.quit()
                            }
                        }
                    }
                }
            }
        }

        // Main Document Work Area
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Table of Contents Sidebar
            Rectangle {
                id: tocSidebar
                Layout.preferredWidth: (backend.hasDocument && backend.tocVisible && backend.toc.length > 0) ? 280 : 0
                Layout.fillHeight: true
                color: "#111218"
                clip: true
                visible: Layout.preferredWidth > 0

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 160; easing.type: Easing.InOutQuad }
                }

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: "#1c1e28"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Sidebar Header
                    Rectangle {
                        Layout.fillWidth: true
                        height: 40
                        color: "#14151e"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 8

                            Text {
                                text: qsTr("Table of Contents")
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: "#9ca3af"
                                Layout.fillWidth: true
                            }

                            IconButton {
                                iconSource: "qrc:/icons/close.svg"
                                iconSize: 14
                                implicitWidth: 26
                                implicitHeight: 26
                                onClicked: backend.toggleToc()
                            }
                        }
                    }

                    // Outline List
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: backend.toc
                        clip: true
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            width: ListView.view.width - 12
                            height: 32
                            x: 6
                            radius: 6
                            color: tocMouse.containsMouse ? "#1c1e29" : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8 + (modelData.depth * 14)
                                anchors.rightMargin: 8
                                spacing: 8

                                Rectangle {
                                    width: 4
                                    height: 4
                                    radius: 2
                                    color: theme.accent
                                }

                                Text {
                                    text: modelData.title
                                    font.pixelSize: 12
                                    color: tocMouse.containsMouse ? "#ffffff" : "#c4c8d4"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            MouseArea {
                                id: tocMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (modelData.page !== undefined) {
                                        backend.goToPage(modelData.page);
                                    } else {
                                        backend.search(modelData.title);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Document Viewport Container
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Minimalist Empty State (Zero Emojis)
                Item {
                    anchors.fill: parent
                    visible: !backend.hasDocument

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 48, 520)
                        spacing: 20

                        // Dropzone Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 220
                            radius: 14
                            color: dropArea.containsDrag ? "#1b1d28" : "#12141c"
                            border.color: dropArea.containsDrag ? theme.accent : "#222533"
                            border.width: dropArea.containsDrag ? 2 : 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 14

                                Rectangle {
                                    width: 56
                                    height: 56
                                    radius: 16
                                    color: "#1c1e2a"
                                    Layout.alignment: Qt.AlignHCenter

                                    Image {
                                        anchors.centerIn: parent
                                        source: "qrc:/icons/document.svg"
                                        width: 26
                                        height: 26
                                    }
                                }

                                ColumnLayout {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignHCenter

                                    Text {
                                        text: qsTr("Drop PDF, Manga or EPUB here")
                                        font.pixelSize: 17
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text {
                                        text: qsTr("or select a document from your files")
                                        font.pixelSize: 13
                                        color: "#717789"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }

                                IconButton {
                                    iconSource: "qrc:/icons/folder.svg"
                                    text: qsTr("Browse Files (Ctrl+O)")
                                    highlighted: true
                                    Layout.alignment: Qt.AlignHCenter
                                    onClicked: backend.openPortal()
                                }
                            }
                        }

                        // Recent Files Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: recentList.count > 0 ? Math.min(recentList.count * 36 + 44, 180) : 0
                            visible: recentList.count > 0
                            radius: 12
                            color: "#12141c"
                            border.color: "#1f222e"
                            border.width: 1
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: qsTr("Recent Documents")
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                        color: "#8b92a4"
                                        Layout.fillWidth: true
                                    }
                                    IconButton {
                                        iconSource: "qrc:/icons/close.svg"
                                        iconSize: 12
                                        implicitWidth: 20
                                        implicitHeight: 20
                                        tooltipText: qsTr("Clear Recent Files")
                                        onClicked: backend.clearRecents()
                                    }
                                }

                                ListView {
                                    id: recentList
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: backend.recentFiles
                                    clip: true
                                    spacing: 2

                                    delegate: Rectangle {
                                        width: ListView.view.width
                                        height: 30
                                        radius: 6
                                        color: recentItemMouse.containsMouse ? "#1c1e29" : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 8

                                            Image {
                                                source: "qrc:/icons/document.svg"
                                                width: 13
                                                height: 13
                                            }

                                            Text {
                                                text: modelData.split('/').pop()
                                                font.pixelSize: 12
                                                color: recentItemMouse.containsMouse ? theme.accent : "#d1d5db"
                                                elide: Text.ElideMiddle
                                                Layout.fillWidth: true
                                            }
                                        }

                                        MouseArea {
                                            id: recentItemMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: backend.openRecent(index)
                                        }
                                    }
                                }
                            }
                        }

                        // Shortcuts Quick Reference
                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 10

                            PillBadge { text: qsTr("Space: Next Page") }
                            PillBadge { text: qsTr("D: Night Mode") }
                            PillBadge { text: qsTr("/: Search") }
                            PillBadge { text: qsTr("?: All Shortcuts") }
                        }
                    }
                }

                // Document Canvas Viewport
                Flickable {
                    id: flickable
                    focus: true
                    anchors.fill: parent
                    visible: backend.hasDocument
                    contentWidth: Math.max(width, pagePaper.width + 64)
                    contentHeight: Math.max(height, pagePaper.height + 64)
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    onWidthChanged: {
                        if (backend.fitMode === 1) backend.fitToWidth(width);
                        else if (backend.fitMode === 2) backend.fitToPage(width, height);
                    }
                    onHeightChanged: {
                        if (backend.fitMode === 2) backend.fitToPage(width, height);
                    }

                    Connections {
                        target: backend
                        function onRotationChanged() {
                            if (backend.fitMode === 1) {
                                backend.fitToWidth(flickable.width);
                            } else if (backend.fitMode === 2) {
                                backend.fitToPage(flickable.width, flickable.height);
                            }
                        }
                        function onCurrentPageChanged() {
                            if (backend.fitMode === 1) {
                                backend.fitToWidth(flickable.width);
                            } else if (backend.fitMode === 2) {
                                backend.fitToPage(flickable.width, flickable.height);
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; active: true }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                    Item {
                        width: flickable.contentWidth
                        height: flickable.contentHeight

                        // Realistic Paper Sheet Canvas
                        Rectangle {
                            id: pagePaper
                            anchors.centerIn: parent
                            width: docImage.width
                            height: docImage.height
                            radius: 3
                            color: backend.darkMode ? "#12141a" : "#ffffff"
                            border.color: backend.darkMode ? "#2a2d3e" : "#30000000"
                            border.width: 1

                            Image {
                                id: docImage
                                anchors.centerIn: parent
                                asynchronous: true
                                cache: true
                                smooth: true
                                fillMode: Image.PreserveAspectFit

                                source: backend.hasDocument
                                    ? backend.pageImageUrl(backend.currentPage, 140 * backend.zoom, backend.rotation, backend.darkMode ? 1 : 0)
                                    : ""

                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: docImage.status === Image.Loading
                                }
                            }

                            // Layer 0.5: Render Search Match Highlights
                            Repeater {
                                model: win.currentSearchHighlights
                                delegate: Rectangle {
                                    x: Math.round(modelData.x * pagePaper.width)
                                    y: Math.round(modelData.y * pagePaper.height)
                                    width: Math.max(4, Math.round(modelData.width * pagePaper.width))
                                    height: Math.max(4, Math.round(modelData.height * pagePaper.height))
                                    z: 12
                                    color: theme.accent
                                    opacity: 0.42
                                    radius: 2
                                    border.color: theme.accent
                                    border.width: 1
                                }
                            }

                            // Layer 1: Render Highlights
                            Repeater {
                                model: win.currentPageAnnots
                                delegate: Item {
                                    id: hlItem
                                    visible: modelData.type === "highlight"
                                    x: Math.round(modelData.x * pagePaper.width)
                                    y: Math.round(modelData.y * pagePaper.height)
                                    width: Math.max(3, Math.round(modelData.width * pagePaper.width))
                                    height: Math.max(3, Math.round(modelData.height * pagePaper.height))
                                    z: 15

                                    Rectangle {
                                        anchors.fill: parent
                                        color: modelData.color || "#ffe082"
                                        opacity: hlHover.hovered ? 0.60 : 0.40
                                        radius: 2
                                        border.color: hlHover.hovered ? (modelData.color || "#ffe082") : "transparent"
                                        border.width: 1

                                        Behavior on opacity { NumberAnimation { duration: 100 } }
                                    }

                                    HoverHandler {
                                        id: hlHover
                                    }

                                    // Small delete button on hover
                                    Rectangle {
                                        visible: hlHover.hovered
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.topMargin: -8
                                        anchors.rightMargin: -8
                                        width: 18
                                        height: 18
                                        radius: 9
                                        color: "#161823"
                                        border.color: "#3f4459"
                                        border.width: 1
                                        z: 35

                                        Image {
                                            anchors.centerIn: parent
                                            source: "qrc:/icons/trash.svg"
                                            width: 10
                                            height: 10
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: backend.deleteAnnotation(modelData.id)
                                        }
                                    }
                                }
                            }

                            // Layer 2: Drag Highlight Selection Preview
                            Rectangle {
                                id: dragHighlightPreview
                                visible: paperAnnotArea.isDrawing && backend.activeTool === 1
                                x: Math.min(paperAnnotArea.startX, paperAnnotArea.currentX)
                                y: Math.min(paperAnnotArea.startY, paperAnnotArea.currentY)
                                width: Math.abs(paperAnnotArea.currentX - paperAnnotArea.startX)
                                height: Math.abs(paperAnnotArea.currentY - paperAnnotArea.startY)
                                color: win.activeAnnotationColor
                                opacity: 0.35
                                border.color: win.activeAnnotationColor
                                border.width: 1.5
                                radius: 2
                                z: 25
                            }

                            // Layer 3: Annotation Interaction MouseArea
                            MouseArea {
                                id: paperAnnotArea
                                anchors.fill: parent
                                z: 20
                                enabled: backend.hasDocument && backend.activeTool === 1
                                cursorShape: Qt.CrossCursor
                                hoverEnabled: true

                                property real startX: 0
                                property real startY: 0
                                property real currentX: 0
                                property real currentY: 0
                                property bool isDrawing: false

                                onPressed: (mouse) => {
                                    if (backend.activeTool === 1) {
                                        startX = mouse.x;
                                        startY = mouse.y;
                                        currentX = mouse.x;
                                        currentY = mouse.y;
                                        isDrawing = true;
                                    }
                                }

                                onPositionChanged: (mouse) => {
                                    if (isDrawing) {
                                        currentX = mouse.x;
                                        currentY = mouse.y;
                                    }
                                }

                                onReleased: (mouse) => {
                                    if (isDrawing && backend.activeTool === 1) {
                                        isDrawing = false;
                                        var w = currentX - startX;
                                        var h = currentY - startY;
                                        if (Math.abs(w) > 4 && Math.abs(h) > 4 && pagePaper.width > 0 && pagePaper.height > 0) {
                                            var rx = Math.min(startX, currentX) / pagePaper.width;
                                            var ry = Math.min(startY, currentY) / pagePaper.height;
                                            var rw = Math.abs(w) / pagePaper.width;
                                            var rh = Math.abs(h) / pagePaper.height;
                                            backend.addHighlight(backend.currentPage, rx, ry, rw, rh, win.activeAnnotationColor);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Modern Floating Search HUD (Top-Right of Viewport)
                Rectangle {
                    id: floatingSearch
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    width: 360
                    height: 42
                    radius: 21
                    color: "#161823"
                    border.color: "#2a2d3e"
                    border.width: 1
                    visible: false

                    function openSearch() {
                        visible = true;
                        searchField.forceActiveFocus();
                        searchField.selectAll();
                    }

                    function closeSearch() {
                        searchDebounceTimer.stop();
                        searchField.text = "";
                        visible = false;
                        backend.clearSearch();
                        windowFocusTarget.forceActiveFocus();
                    }

                    Timer {
                        id: searchDebounceTimer
                        interval: 150
                        repeat: false
                        onTriggered: backend.search(searchField.text)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 8

                        Image {
                            source: "qrc:/icons/search.svg"
                            width: 15
                            height: 15
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            HoverHandler {
                                id: searchHover
                            }

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 3
                                verticalAlignment: Text.AlignVCenter
                                text: qsTr("Find in document...")
                                color: "#61677c"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                visible: searchField.text.length === 0 && !searchHover.hovered
                            }

                            TextInput {
                                id: searchField
                                anchors.fill: parent
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#ffffff"
                                selectionColor: theme.accent
                                selectedTextColor: theme.accentForeground
                                selectByMouse: true
                                font.pixelSize: 12
                                clip: true
                                onTextEdited: searchDebounceTimer.restart()
                                onAccepted: {
                                    searchDebounceTimer.stop();
                                    if (backend.searchQuery !== text.trim()) {
                                        backend.search(text);
                                    } else {
                                        backend.nextSearchResult();
                                    }
                                }
                            }
                        }

                        PillBadge {
                            visible: backend.searchQuery.length > 0
                            text: backend.searchResultsCount > 0
                                ? qsTr("%1 of %2").arg(backend.searchCurrentIndex + 1).arg(backend.searchResultsCount)
                                : qsTr("0 matches")
                            isAccent: backend.searchResultsCount > 0
                        }

                        IconButton {
                            iconSource: "qrc:/icons/arrow-up.svg"
                            iconSize: 14
                            implicitWidth: 26
                            implicitHeight: 26
                            tooltipText: qsTr("Previous match (Shift+N)")
                            enabled: backend.searchResultsCount > 0
                            onClicked: backend.prevSearchResult()
                        }

                        IconButton {
                            iconSource: "qrc:/icons/arrow-down.svg"
                            iconSize: 14
                            implicitWidth: 26
                            implicitHeight: 26
                            tooltipText: qsTr("Next match (N)")
                            enabled: backend.searchResultsCount > 0
                            onClicked: backend.nextSearchResult()
                        }

                        IconButton {
                            iconSource: "qrc:/icons/close.svg"
                            iconSize: 14
                            implicitWidth: 26
                            implicitHeight: 26
                            tooltipText: qsTr("Close Search (Esc)")
                            onClicked: floatingSearch.closeSearch()
                        }
                    }
                }

                // Floating Go-To Page HUD at Bottom
                PageScrubber {
                    id: pageScrubber
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: backend.hasDocument && backend.pageCount > 1
                    currentPage: backend.currentPage
                    totalPages: backend.pageCount
                    onJumpToPage: (p) => backend.goToPage(p)
                    onDismissFocusRequested: windowFocusTarget.forceActiveFocus()
                }

                // Toast notification banner for open/save errors & status
                Rectangle {
                    id: toastBanner
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                    z: 90
                    visible: false
                    height: 36
                    width: Math.min(parent.width - 48, toastText.implicitWidth + 32)
                    radius: 18
                    color: isError ? "#2b161b" : "#161d24"
                    border.color: isError ? "#ef4444" : theme.accent
                    border.width: 1

                    property bool isError: false

                    Timer {
                        id: toastTimer
                        interval: 3500
                        repeat: false
                        onTriggered: toastBanner.visible = false
                    }

                    Connections {
                        target: backend
                        function onErrorOccurred(message) {
                            if (message && message.length > 0) {
                                toastBanner.isError = true;
                                toastText.text = message;
                                toastBanner.visible = true;
                                toastTimer.restart();
                            }
                        }
                        function onStatusMessage(message) {
                            if (message && message.length > 0) {
                                toastBanner.isError = false;
                                toastText.text = message;
                                toastBanner.visible = true;
                                toastTimer.restart();
                            }
                        }
                    }

                    Text {
                        id: toastText
                        anchors.centerIn: parent
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: "#ffffff"
                        elide: Text.ElideRight
                        width: Math.min(implicitWidth, parent.width - 24)
                    }
                }

                // Drag and drop receiver
                DropArea {
                    id: dropArea
                    anchors.fill: parent
                    onDropped: (drop) => {
                        if (drop.hasUrls && drop.urls.length > 0) {
                            backend.openFile(drop.urls[0]);
                            drop.acceptProposedAction();
                        }
                    }
                }
            }
        }
    }

    // Help & Keyboard Shortcuts Modal
    Rectangle {
        anchors.fill: parent
        color: "#ee090a10"
        visible: backend.helpVisible

        MouseArea {
            anchors.fill: parent
            onClicked: backend.helpVisible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: 620
            implicitHeight: modalContent.implicitHeight + 48
            radius: Math.max(10, theme.radius)
            color: "#161823"
            border.color: "#2b2f40"
            border.width: 1

            MouseArea {
                anchors.fill: parent
            }

            ColumnLayout {
                id: modalContent
                anchors.fill: parent
                anchors.margins: 24
                spacing: 20

                // Modal Header
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Keyboard Shortcuts")
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: "#ffffff"
                        Layout.fillWidth: true
                    }
                    IconButton {
                        iconSource: "qrc:/icons/close.svg"
                        iconSize: 14
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: backend.helpVisible = false
                    }
                }

                // Two Column Layout
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 32

                    // Column 1: Navigation & View
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            text: qsTr("NAVIGATION & VIEW")
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: theme.accent
                            font.letterSpacing: 0.5
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "→" }
                                KeyBadge { text: "J" }
                                KeyBadge { text: "Space" }
                                KeyBadge { text: "PgDn" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Next page"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "←" }
                                KeyBadge { text: "K" }
                                KeyBadge { text: "Shift+Space" }
                                KeyBadge { text: "PgUp" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Previous page"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "Home" }
                                KeyBadge { text: "End" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("First / Last page"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "+" }
                                KeyBadge { text: "−" }
                                KeyBadge { text: "0" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Zoom In / Out / 100%"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "W" }
                                KeyBadge { text: "P" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Fit to Width / Page"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "R" }
                                KeyBadge { text: "Shift+R" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Rotate CW / CCW"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            KeyBadge { text: "D" }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Night reading mode"); color: "#d1d5db"; font.pixelSize: 12 }
                        }
                    }

                    // Column 2: Document & Search
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            text: qsTr("DOCUMENT & ANNOTATIONS")
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: theme.accent
                            font.letterSpacing: 0.5
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "H" }
                                KeyBadge { text: "V" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Highlight / Pan"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            KeyBadge { text: "T" }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Toggle Contents"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            KeyBadge { text: "/" }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Find in document"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "N" }
                                KeyBadge { text: "Shift+N" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Next / Prev match"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "Ctrl+O" }
                                KeyBadge { text: "O" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Open document"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            KeyBadge { text: "Ctrl+S" }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Save copy"); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            KeyBadge { text: "Ctrl+Shift+S" }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Save As..."); color: "#d1d5db"; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Row {
                                spacing: 4
                                KeyBadge { text: "Esc" }
                                KeyBadge { text: "Q" }
                            }
                            Item { Layout.fillWidth: true }
                            Text { text: qsTr("Close / Quit"); color: "#d1d5db"; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }
}
