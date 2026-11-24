#include "backend.h"
#include "documentengine.h"
#include "pageimageprovider.h"
#include "portalfilepicker.h"

#include <QFileInfo>
#include <algorithm>

Backend::Backend(DocumentEngine *engine, PageImageProvider *provider, QObject *parent)
    : QObject(parent), m_engine(engine), m_provider(provider) {
    m_picker = new PortalFilePicker(this);
    connect(m_picker, &PortalFilePicker::fileSelected, this, &Backend::openFile);
}

QString Backend::filePath() const {
    return m_engine ? m_engine->filePath() : QString();
}

QString Backend::fileName() const {
    const QString path = filePath();
    return path.isEmpty() ? QString() : QFileInfo(path).fileName();
}

QString Backend::title() const {
    if (!m_engine) return {};
    const QString docTitle = m_engine->title();
    return docTitle.isEmpty() ? fileName() : docTitle;
}

int Backend::pageCount() const {
    return m_engine ? m_engine->pageCount() : 0;
}

bool Backend::hasDocument() const {
    return m_engine && m_engine->isLoaded() && m_engine->pageCount() > 0;
}

void Backend::setCurrentPage(int page) {
    const int total = pageCount();
    if (total == 0) {
        if (m_currentPage != 0) {
            m_currentPage = 0;
            emit currentPageChanged();
        }
        return;
    }
    const int clamped = std::clamp(page, 0, total - 1);
    if (clamped != m_currentPage) {
        m_currentPage = clamped;
        emit currentPageChanged();
    }
}

void Backend::setZoom(double z) {
    const double clamped = std::clamp(z, 0.25, 4.0);
    if (clamped != m_zoom) {
        m_zoom = clamped;
        emit zoomChanged();
    }
}

void Backend::setDarkMode(bool dark) {
    if (dark != m_darkMode) {
        m_darkMode = dark;
        if (m_provider) m_provider->clearCache();
        ++m_revision;
        emit darkModeChanged();
        emit revisionChanged();
    }
}

void Backend::setTocVisible(bool visible) {
    if (visible != m_tocVisible) {
        m_tocVisible = visible;
        emit tocVisibleChanged();
    }
}

void Backend::setHelpVisible(bool visible) {
    if (visible != m_helpVisible) {
        m_helpVisible = visible;
        emit helpVisibleChanged();
    }
}

void Backend::openFile(const QString &path) {
    if (!m_engine) return;

    QString cleanPath = path;
    if (cleanPath.startsWith("file://")) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    if (m_engine->load(cleanPath)) {
        if (m_provider) m_provider->clearCache();
        m_currentPage = 0;
        m_zoom = 1.0;
        m_toc = m_engine->tableOfContents();
        clearSearch();
        ++m_revision;

        emit documentChanged();
        emit currentPageChanged();
        emit zoomChanged();
        emit tocChanged();
        emit revisionChanged();
    }
}

void Backend::openPortal() {
    if (m_picker) {
        m_picker->openFile(QStringLiteral("Open Document"));
    }
}

void Backend::closeDocument() {
    if (m_engine) {
        m_engine->close();
        if (m_provider) m_provider->clearCache();
        m_currentPage = 0;
        m_toc.clear();
        clearSearch();
        ++m_revision;

        emit documentChanged();
        emit currentPageChanged();
        emit tocChanged();
        emit revisionChanged();
    }
}

void Backend::nextPage() {
    goToPage(m_currentPage + 1);
}

void Backend::prevPage() {
    goToPage(m_currentPage - 1);
}

void Backend::goToPage(int page) {
    setCurrentPage(page);
}

void Backend::zoomIn() {
    setZoom(m_zoom * 1.25);
}

void Backend::zoomOut() {
    setZoom(m_zoom / 1.25);
}

void Backend::resetZoom() {
    setZoom(1.0);
}

void Backend::toggleDarkMode() {
    setDarkMode(!m_darkMode);
}

void Backend::toggleToc() {
    setTocVisible(!m_tocVisible);
}

void Backend::toggleHelp() {
    setHelpVisible(!m_helpVisible);
}

void Backend::search(const QString &query) {
    m_searchQuery = query.trimmed();
    m_searchResults.clear();
    m_searchIndex = -1;

    if (!m_searchQuery.isEmpty() && m_engine) {
        m_searchResults = m_engine->search(m_searchQuery);
        if (!m_searchResults.isEmpty()) {
            m_searchIndex = 0;
            goToPage(m_searchResults.first());
        }
    }
    emit searchChanged();
}

void Backend::nextSearchResult() {
    if (m_searchResults.isEmpty()) return;
    m_searchIndex = (m_searchIndex + 1) % m_searchResults.size();
    goToPage(m_searchResults.at(m_searchIndex));
    emit searchChanged();
}

void Backend::prevSearchResult() {
    if (m_searchResults.isEmpty()) return;
    m_searchIndex = (m_searchIndex - 1 + m_searchResults.size()) % m_searchResults.size();
    goToPage(m_searchResults.at(m_searchIndex));
    emit searchChanged();
}

void Backend::clearSearch() {
    m_searchQuery.clear();
    m_searchResults.clear();
    m_searchIndex = -1;
    emit searchChanged();
}

QString Backend::pageImageUrl(int pageIndex, double dpi) const {
    const QString darkFlag = m_darkMode ? QStringLiteral("1") : QStringLiteral("0");
    return QStringLiteral("image://doc/%1_%2_%3_%4")
        .arg(pageIndex)
        .arg(static_cast<int>(dpi))
        .arg(darkFlag)
        .arg(m_revision);
}

QSizeF Backend::pageSize(int pageIndex) const {
    return m_engine ? m_engine->pageSize(pageIndex) : QSizeF(0, 0);
}
