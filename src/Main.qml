import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1020
    height: 740
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: backend.hasDocument ? backend.title + " — Omadoc" : "Omadoc"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    // Application-wide shortcuts
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: backend.toggleHelp() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.openPortal() }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; onActivated: backend.toggleDarkMode() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; onActivated: backend.toggleToc() }
    Shortcut { sequence: "J"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "Shift+Space"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "PageDown"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
    Shortcut { sequence: "PageUp"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Home"; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(0) }
    Shortcut { sequence: "End"; context: Qt.ApplicationShortcut; onActivated: backend.goToPage(backend.pageCount - 1) }
    Shortcut { sequence: "+"; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "="; context: Qt.ApplicationShortcut; onActivated: backend.zoomIn() }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: backend.zoomOut() }
    Shortcut { sequence: "0"; context: Qt.ApplicationShortcut; onActivated: backend.resetZoom() }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; onActivated: { searchBar.visible = true; searchInput.forceActiveFocus(); } }
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

        // Top Toolbar
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "#16161a"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Button {
                    text: qsTr("TOC")
                    visible: backend.hasDocument && backend.toc.length > 0
                    highlighted: backend.tocVisible
                    onClicked: backend.toggleToc()
                }

                Label {
                    text: backend.hasDocument ? backend.title : "Omadoc"
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    color: "#ffffff"
                }

                // Page Navigation
                RowLayout {
                    visible: backend.hasDocument
                    spacing: 4

                    Button {
                        text: "‹"
                        enabled: backend.currentPage > 0
                        onClicked: backend.prevPage()
                    }

                    Label {
                        text: qsTr("Page %1 of %2").arg(backend.currentPage + 1).arg(backend.pageCount)
                        color: "#cccccc"
                    }

                    Button {
                        text: "›"
                        enabled: backend.currentPage < backend.pageCount - 1
                        onClicked: backend.nextPage()
                    }
                }

                // Zoom controls
                RowLayout {
                    visible: backend.hasDocument
                    spacing: 4

                    Button {
                        text: "−"
                        onClicked: backend.zoomOut()
                    }

                    Label {
                        text: Math.round(backend.zoom * 100) + "%"
                        color: "#aaaaaa"
                        Layout.minimumWidth: 40
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Button {
                        text: "+"
                        onClicked: backend.zoomIn()
                    }
                }

                Button {
                    text: qsTr("Night")
                    visible: backend.hasDocument
                    highlighted: backend.darkMode
                    onClicked: backend.toggleDarkMode()
                }

                Button {
                    text: qsTr("Search")
                    visible: backend.hasDocument
                    onClicked: {
                        searchBar.visible = !searchBar.visible;
                        if (searchBar.visible) searchInput.forceActiveFocus();
                    }
                }

                Button {
                    text: qsTr("Open")
                    onClicked: backend.openPortal()
                }

                Button {
                    text: "?"
                    onClicked: backend.toggleHelp()
                }
            }
        }

        // Inline Search Bar
        Rectangle {
            id: searchBar
            Layout.fillWidth: true
            height: 44
            color: "#1e1e24"
            visible: false

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                TextField {
                    id: searchInput
                    placeholderText: qsTr("Search in document...")
                    Layout.fillWidth: true
                    onTextChanged: backend.search(text)
                    onAccepted: backend.nextSearchResult()
                }

                Label {
                    visible: backend.searchQuery.length > 0
                    text: backend.searchResultsCount > 0
                        ? qsTr("%1 of %2").arg(backend.searchCurrentIndex + 1).arg(backend.searchResultsCount)
                        : qsTr("No matches")
                    color: backend.searchResultsCount > 0 ? theme.accent : "#ff6666"
                }

                Button {
                    text: "↑"
                    enabled: backend.searchResultsCount > 0
                    onClicked: backend.prevSearchResult()
                }

                Button {
                    text: "↓"
                    enabled: backend.searchResultsCount > 0
                    onClicked: backend.nextSearchResult()
                }

                Button {
                    text: "✕"
                    onClicked: {
                        searchBar.visible = false;
                        backend.clearSearch();
                    }
                }
            }
        }

        // Main Document Area
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Table of Contents Sidebar
            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: "#121215"
                visible: backend.hasDocument && backend.tocVisible && backend.toc.length > 0

                ListView {
                    anchors.fill: parent
                    anchors.margins: 8
                    model: backend.toc
                    clip: true
                    spacing: 4

                    delegate: ItemDelegate {
                        width: parent.width
                        text: modelData.title
                        leftPadding: 8 + (modelData.depth * 14)
                        onClicked: {
                            backend.search(modelData.title);
                        }
                    }
                }
            }

            // Document Viewport or Empty State
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Empty State
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: !backend.hasDocument
                    spacing: 16

                    Rectangle {
                        width: 72
                        height: 72
                        radius: Math.max(8, theme.radius)
                        color: theme.accent
                        Layout.alignment: Qt.AlignHCenter

                        Text {
                            anchors.centerIn: parent
                            text: "📄"
                            font.pixelSize: 36
                        }
                    }

                    Label {
                        text: qsTr("No Document Open")
                        font.pixelSize: 20
                        font.bold: true
                        color: "#ffffff"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: qsTr("Open a PDF file or drop it anywhere here")
                        color: "#888888"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Button {
                        text: qsTr("Choose File (O)")
                        highlighted: true
                        Layout.alignment: Qt.AlignHCenter
                        onClicked: backend.openPortal()
                    }
                }

                // Document Viewer
                Flickable {
                    id: flickable
                    anchors.fill: parent
                    visible: backend.hasDocument
                    contentWidth: Math.max(width, docImage.width + 40)
                    contentHeight: Math.max(height, docImage.height + 40)
                    clip: true

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

                    Item {
                        width: flickable.contentWidth
                        height: flickable.contentHeight

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

                DropArea {
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

    // Help & Shortcuts Overlay
    Rectangle {
        anchors.fill: parent
        color: "#d0000000"
        visible: backend.helpVisible

        MouseArea {
            anchors.fill: parent
            onClicked: backend.helpVisible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: 480
            height: 380
            radius: Math.max(6, theme.radius)
            color: "#1a1a20"
            border.color: theme.accent
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Keyboard Shortcuts")
                        font.pixelSize: 18
                        font.bold: true
                        color: "#ffffff"
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "✕"
                        onClicked: backend.helpVisible = false
                    }
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 6
                    columnSpacing: 24
                    Layout.fillWidth: true

                    Label { text: "J / Space / PageDown"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Next page"); color: "#dddddd" }

                    Label { text: "K / Shift+Space / PageUp"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Previous page"); color: "#dddddd" }

                    Label { text: "Home / End"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("First / Last page"); color: "#dddddd" }

                    Label { text: "+ / - / 0"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Zoom In / Out / Reset"); color: "#dddddd" }

                    Label { text: "D"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Toggle Night Mode"); color: "#dddddd" }

                    Label { text: "T"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Toggle Outline / TOC"); color: "#dddddd" }

                    Label { text: "/"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Search in document"); color: "#dddddd" }

                    Label { text: "N / Shift+N"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Next / Prev match"); color: "#dddddd" }

                    Label { text: "O / Ctrl+O"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Open File dialog"); color: "#dddddd" }

                    Label { text: "Q / Esc"; color: theme.accent; font.bold: true }
                    Label { text: qsTr("Quit / Close dialogs"); color: "#dddddd" }
                }
            }
        }
    }
}
