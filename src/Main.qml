import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1080
    height: 780
    minimumWidth: 700
    minimumHeight: 500
    visible: true
    title: backend.hasDocument ? backend.title + " — Omadoc" : "Omadoc"
    color: "#0f1015"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    // Application Shortcuts
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: backend.toggleHelp() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; onActivated: backend.toggleDarkMode() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: backend.rotateClockwise() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; onActivated: backend.toggleToc() }
    Shortcut { sequence: "W"; context: Qt.ApplicationShortcut; onActivated: backend.fitToWidth(flickable.width) }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.fitToPage(flickable.width, flickable.height) }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "Shift+Space"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; onActivated: flickable.flick(0, -600) }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; onActivated: flickable.flick(0, 600) }
    Shortcut { sequence: "PageDown"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "PageUp"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(0) }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(backend.pageCount - 1) }
    Shortcut { sequence: "+"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "="; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; onActivated: backend.resetZoom() }
    Shortcut {
        sequence: "/"
        context: Qt.ApplicationShortcut
        onActivated: {
            searchBar.visible = true;
            searchInput.forceActiveFocus();
        }
    }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; onActivated: backend.nextSearchResult() }
    Shortcut { sequence: "Shift+N"; context: Qt.ApplicationShortcut; onActivated: backend.prevSearchResult() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.helpVisible) backend.helpVisible = false;
            else if (searchBar.visible) { searchBar.visible = false; backend.clearSearch(); }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Modern Header Toolbar
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            height: 52
            color: "#14151c"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#21232d"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Sidebar Toggle
                IconButton {
                    iconText: "☰"
                    tooltipText: qsTr("Table of Contents (T)")
                    active: backend.tocVisible
                    visible: backend.hasDocument && backend.toc.length > 0
                    onClicked: backend.toggleToc()
                }

                // Document Title
                RowLayout {
                    spacing: 8
                    Layout.maximumWidth: 260
                    Layout.fillWidth: false

                    Rectangle {
                        width: 24
                        height: 24
                        radius: 6
                        color: theme.accent
                        visible: backend.hasDocument

                        Text {
                            anchors.centerIn: parent
                            text: "PDF"
                            font.pixelSize: 9
                            font.bold: true
                            color: theme.accentForeground
                        }
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

                // Centered Navigation Controls
                RowLayout {
                    visible: backend.hasDocument
                    spacing: 6

                    IconButton {
                        iconText: "‹"
                        tooltipText: qsTr("Previous Page (K)")
                        onClicked: backend.prevPage()
                    }

                    Rectangle {
                        implicitWidth: 84
                        implicitHeight: 28
                        radius: 14
                        color: "#1c1e27"
                        border.color: "#2b2e3c"
                        border.width: 1

                        Row {
                            anchors.centerIn: parent
                            spacing: 4

                            TextInput {
                                id: pageInput
                                text: (backend.currentPage + 1).toString()
                                font.pixelSize: 12
                                font.bold: true
                                color: "#ffffff"
                                selectByMouse: true
                                horizontalAlignment: Text.AlignRight
                                anchors.verticalCenter: parent.verticalCenter
                                onAccepted: {
                                    const val = parseInt(text) - 1;
                                    if (!isNaN(val)) backend.goToPage(val);
                                    pageInput.focus = false;
                                }
                            }

                            Text {
                                text: "/ " + backend.pageCount
                                font.pixelSize: 11
                                color: "#787e91"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    IconButton {
                        iconText: "›"
                        tooltipText: qsTr("Next Page (J / Space)")
                        onClicked: backend.nextPage()
                    }
                }

                Item { Layout.fillWidth: true } // Spacer

                // Right Toolbar Controls
                RowLayout {
                    spacing: 4

                    // Zoom Pill Group
                    Rectangle {
                        visible: backend.hasDocument
                        implicitHeight: 30
                        implicitWidth: zoomRow.implicitWidth + 8
                        radius: 15
                        color: "#1c1e27"
                        border.color: "#2b2e3c"
                        border.width: 1

                        RowLayout {
                            id: zoomRow
                            anchors.centerIn: parent
                            spacing: 2

                            IconButton {
                                iconText: "−"
                                implicitWidth: 26
                                implicitHeight: 26
                                tooltipText: qsTr("Zoom Out (-)")
                                onClicked: backend.zoomOut()
                            }

                            Text {
                                text: Math.round(backend.zoom * 100) + "%"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: "#c0c4d2"
                                Layout.minimumWidth: 38
                                horizontalAlignment: Text.AlignHCenter
                            }

                            IconButton {
                                iconText: "+"
                                implicitWidth: 26
                                implicitHeight: 26
                                tooltipText: qsTr("Zoom In (+)")
                                onClicked: backend.zoomIn()
                            }
                        }
                    }

                    // Fit to Width
                    IconButton {
                        iconText: "⇥"
                        visible: backend.hasDocument
                        active: backend.fitMode === 1
                        tooltipText: qsTr("Fit to Width (W)")
                        onClicked: backend.fitToWidth(flickable.width)
                    }

                    // Fit to Page
                    IconButton {
                        iconText: "↕"
                        visible: backend.hasDocument
                        active: backend.fitMode === 2
                        tooltipText: qsTr("Fit to Page (P)")
                        onClicked: backend.fitToPage(flickable.width, flickable.height)
                    }

                    // Rotate
                    IconButton {
                        iconText: "↻"
                        visible: backend.hasDocument
                        tooltipText: qsTr("Rotate (R)")
                        onClicked: backend.rotateClockwise()
                    }

                    // Night Mode
                    IconButton {
                        iconText: backend.darkMode ? "☀️" : "🌙"
                        visible: backend.hasDocument
                        active: backend.darkMode
                        tooltipText: qsTr("Night Reading Mode (D)")
                        onClicked: backend.toggleDarkMode()
                    }

                    // Search Toggle
                    IconButton {
                        iconText: "🔍"
                        visible: backend.hasDocument
                        active: searchBar.visible
                        tooltipText: qsTr("Search (/)")
                        onClicked: {
                            searchBar.visible = !searchBar.visible;
                            if (searchBar.visible) searchInput.forceActiveFocus();
                        }
                    }

                    // Help
                    IconButton {
                        iconText: "?"
                        active: backend.helpVisible
                        tooltipText: qsTr("Shortcuts (?)")
                        onClicked: backend.toggleHelp()
                    }

                    // Open Button
                    Rectangle {
                        implicitHeight: 32
                        implicitWidth: openBtnRow.implicitWidth + 20
                        radius: 16
                        color: openMouseArea.containsPress ? "#282a36"
                             : openMouseArea.containsMouse ? "#20232e"
                             : "#181a22"
                        border.color: theme.accent
                        border.width: 1

                        RowLayout {
                            id: openBtnRow
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: "📂"
                                font.pixelSize: 12
                            }

                            Text {
                                text: qsTr("Open")
                                font.pixelSize: 12
                                font.weight: Font.SemiBold
                                color: "#ffffff"
                            }
                        }

                        MouseArea {
                            id: openMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.openPortal()
                        }
                    }
                }
            }
        }

        // Inline Search Drawer
        Rectangle {
            id: searchBar
            Layout.fillWidth: true
            height: visible ? 46 : 0
            visible: false
            color: "#181922"
            clip: true

            Behavior on height {
                NumberAnimation { duration: 150; easing.type: Easing.InOutQuad }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#272a38"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 10

                Text {
                    text: "🔍"
                    font.pixelSize: 14
                }

                TextField {
                    id: searchInput
                    placeholderText: qsTr("Search text in document...")
                    placeholderTextColor: "#63697e"
                    color: "#ffffff"
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    background: Item {}
                    onTextChanged: backend.search(text)
                    onAccepted: backend.nextSearchResult()
                }

                PillBadge {
                    visible: backend.searchQuery.length > 0
                    text: backend.searchResultsCount > 0
                        ? qsTr("%1 of %2").arg(backend.searchCurrentIndex + 1).arg(backend.searchResultsCount)
                        : qsTr("No matches")
                    isAccent: backend.searchResultsCount > 0
                }

                IconButton {
                    iconText: "↑"
                    implicitWidth: 30
                    implicitHeight: 30
                    tooltipText: qsTr("Previous match (Shift+N)")
                    enabled: backend.searchResultsCount > 0
                    onClicked: backend.prevSearchResult()
                }

                IconButton {
                    iconText: "↓"
                    implicitWidth: 30
                    implicitHeight: 30
                    tooltipText: qsTr("Next match (N)")
                    enabled: backend.searchResultsCount > 0
                    onClicked: backend.nextSearchResult()
                }

                IconButton {
                    iconText: "✕"
                    implicitWidth: 30
                    implicitHeight: 30
                    tooltipText: qsTr("Close Search (Esc)")
                    onClicked: {
                        searchBar.visible = false;
                        backend.clearSearch();
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
                color: "#13141b"
                clip: true
                visible: Layout.preferredWidth > 0

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
                }

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: "#21232d"
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Sidebar Header
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        color: "#161822"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 10

                            Text {
                                text: qsTr("Outline")
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: "#d1d5db"
                                Layout.fillWidth: true
                            }

                            IconButton {
                                iconText: "✕"
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
                            color: tocMouse.containsMouse ? "#21232e" : "transparent"

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
                                onClicked: backend.search(modelData.title)
                            }
                        }
                    }
                }
            }

            // Viewport & Empty State
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Beautiful Welcoming Empty State
                Item {
                    anchors.fill: parent
                    visible: !backend.hasDocument

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 40, 520)
                        spacing: 24

                        // Dropzone Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: 240
                            radius: 16
                            color: dropArea.containsDrag ? "#1d202b" : "#14151d"
                            border.color: dropArea.containsDrag ? theme.accent : "#252834"
                            border.width: dropArea.containsDrag ? 2 : 1

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 14

                                // Large Document Icon
                                Rectangle {
                                    width: 64
                                    height: 64
                                    radius: 18
                                    color: "#212430"
                                    Layout.alignment: Qt.AlignHCenter

                                    Text {
                                        anchors.centerIn: parent
                                        text: "📄"
                                        font.pixelSize: 30
                                    }
                                }

                                ColumnLayout {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignHCenter

                                    Text {
                                        text: qsTr("Drop a PDF here to begin")
                                        font.pixelSize: 18
                                        font.weight: Font.Bold
                                        color: "#ffffff"
                                        Layout.alignment: Qt.AlignHCenter
                                    }

                                    Text {
                                        text: qsTr("or select a document from your system")
                                        font.pixelSize: 13
                                        color: "#787e91"
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }

                                // Open Button
                                Rectangle {
                                    implicitWidth: 160
                                    implicitHeight: 40
                                    radius: 20
                                    color: theme.accent
                                    Layout.alignment: Qt.AlignHCenter

                                    Text {
                                        anchors.centerIn: parent
                                        text: qsTr("Choose File (Ctrl+O)")
                                        font.pixelSize: 13
                                        font.weight: Font.SemiBold
                                        color: theme.accentForeground
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.openPortal()
                                    }
                                }
                            }
                        }

                        // Recent Files Card
                        Rectangle {
                            Layout.fillWidth: true
                            height: recentList.count > 0 ? Math.min(recentList.count * 38 + 48, 180) : 0
                            visible: recentList.count > 0
                            radius: 12
                            color: "#14151d"
                            border.color: "#222530"
                            border.width: 1
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: qsTr("Recent Documents")
                                        font.pixelSize: 12
                                        font.weight: Font.Bold
                                        color: "#9ca3af"
                                        Layout.fillWidth: true
                                    }
                                    IconButton {
                                        iconText: "✕"
                                        implicitWidth: 20
                                        implicitHeight: 20
                                        tooltipText: qsTr("Clear Recents")
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
                                        height: 32
                                        radius: 6
                                        color: recentItemMouse.containsMouse ? "#212430" : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 8

                                            Text {
                                                text: "📄"
                                                font.pixelSize: 11
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

                        // Quick Keyboard Tips
                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 12

                            PillBadge { text: "Space: Next Page" }
                            PillBadge { text: "D: Night Mode" }
                            PillBadge { text: "/: Search" }
                            PillBadge { text: "?: All Shortcuts" }
                        }
                    }
                }

                // Document Viewer Canvas
                Flickable {
                    id: flickable
                    anchors.fill: parent
                    visible: backend.hasDocument
                    contentWidth: Math.max(width, pagePaper.width + 80)
                    contentHeight: Math.max(height, pagePaper.height + 80)
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; active: true }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                    Item {
                        width: flickable.contentWidth
                        height: flickable.contentHeight

                        // Realistic Paper Sheet with Soft Shadow
                        Rectangle {
                            id: pagePaper
                            anchors.centerIn: parent
                            width: docImage.width
                            height: docImage.height
                            radius: 4
                            color: backend.darkMode ? "#14151b" : "#ffffff"

                            // Simulated realistic drop shadow
                            border.color: backend.darkMode ? "#2b2e3e" : "#00000030"
                            border.width: 1

                            Image {
                                id: docImage
                                anchors.centerIn: parent
                                asynchronous: true
                                cache: false
                                smooth: true
                                fillMode: Image.PreserveAspectFit

                                source: backend.hasDocument
                                    ? backend.pageImageUrl(backend.currentPage, 140 * backend.zoom)
                                    : ""

                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: docImage.status === Image.Loading
                                }
                            }
                        }
                    }
                }

                // Floating Scrubber HUD at Bottom
                PageScrubber {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: backend.hasDocument && backend.pageCount > 1
                    currentPage: backend.currentPage
                    totalPages: backend.pageCount
                    onJumpToPage: (p) => backend.goToPage(p)
                }

                // Drop Area for drag-and-drop support
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
        color: "#c8000000"
        visible: backend.helpVisible

        MouseArea {
            anchors.fill: parent
            onClicked: backend.helpVisible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: 520
            height: 440
            radius: Math.max(8, theme.radius)
            color: "#161822"
            border.color: "#282c3c"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

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
                        iconText: "✕"
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: backend.helpVisible = false
                    }
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 28
                    Layout.fillWidth: true

                    Row {
                        spacing: 4
                        PillBadge { text: "J"; isAccent: true }
                        PillBadge { text: "Space" }
                        PillBadge { text: "PgDn" }
                    }
                    Text { text: qsTr("Next page"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "K"; isAccent: true }
                        PillBadge { text: "Shift+Space" }
                        PillBadge { text: "PgUp" }
                    }
                    Text { text: qsTr("Previous page"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "Home" }
                        PillBadge { text: "End" }
                    }
                    Text { text: qsTr("First / Last page"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "+" }
                        PillBadge { text: "−" }
                        PillBadge { text: "0" }
                    }
                    Text { text: qsTr("Zoom In / Out / Reset"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "W" }
                        PillBadge { text: "P" }
                    }
                    Text { text: qsTr("Fit to Width / Page"); color: "#e5e7eb"; font.pixelSize: 13 }

                    PillBadge { text: "R"; isAccent: true }
                    Text { text: qsTr("Rotate 90 degrees"); color: "#e5e7eb"; font.pixelSize: 13 }

                    PillBadge { text: "D"; isAccent: true }
                    Text { text: qsTr("Night reading mode"); color: "#e5e7eb"; font.pixelSize: 13 }

                    PillBadge { text: "T"; isAccent: true }
                    Text { text: qsTr("Toggle Table of Contents"); color: "#e5e7eb"; font.pixelSize: 13 }

                    PillBadge { text: "/"; isAccent: true }
                    Text { text: qsTr("Search in document"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "N" }
                        PillBadge { text: "Shift+N" }
                    }
                    Text { text: qsTr("Next / Previous match"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "O" }
                        PillBadge { text: "Ctrl+O" }
                    }
                    Text { text: qsTr("Open file dialog"); color: "#e5e7eb"; font.pixelSize: 13 }

                    Row {
                        spacing: 4
                        PillBadge { text: "Q" }
                        PillBadge { text: "Esc" }
                    }
                    Text { text: qsTr("Quit / Close overlay"); color: "#e5e7eb"; font.pixelSize: 13 }
                }
            }
        }
    }
}
