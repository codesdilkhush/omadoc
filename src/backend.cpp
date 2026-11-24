#include "backend.h"
#include "documentengine.h"
#include "pageimageprovider.h"
#include "portalfilepicker.h"

#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <algorithm>

Backend::Backend(DocumentEngine *engine, PageImageProvider *provider, QObject *parent)
    : QObject(parent), m_engine(engine), m_provider(provider) {
    m_picker = new PortalFilePicker(this);
    connect(m_picker, &PortalFilePicker::fileSelected, this, &Backend::openFile);
    connect(m_picker, &PortalFilePicker::saveTargetSelected, this, [this](const QString &target) {
        if (m_engine && m_engine->saveCopy(target)) {
            addRecentFile(target);
        }
    });
    loadRecents();
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
    const double clamped = std::clamp(z, 0.2, 5.0);
    if (std::abs(clamped - m_zoom) > 0.001) {
        m_zoom = clamped;
        if (m_fitMode != 0) {
            m_fitMode = 0;
            emit fitModeChanged();
        }
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

void Backend::setRotation(int rot) {
    const int norm = (rot % 360 + 360) % 360;
    if (norm != m_rotation) {
        m_rotation = norm;
        emit rotationChanged();
    }
}

void Backend::setFitMode(int mode) {
    if (mode != m_fitMode) {
        m_fitMode = mode;
        emit fitModeChanged();
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
        m_rotation = 0;
        m_fitMode = 0;
        m_toc = m_engine->tableOfContents();
        clearSearch();
        ++m_revision;

        addRecentFile(cleanPath);

        emit documentChanged();
        emit currentPageChanged();
        emit zoomChanged();
        emit rotationChanged();
        emit fitModeChanged();
        emit tocChanged();
        emit revisionChanged();
    }
}

void Backend::openRecent(int index) {
    if (index >= 0 && index < m_recentFiles.size()) {
        openFile(m_recentFiles.at(index));
    }
}

void Backend::openPortal() {
    if (m_picker) {
        m_picker->openFile(QStringLiteral("Open Document"));
    }
}

void Backend::saveDocument() {
    if (!hasDocument() || !m_engine) return;
    m_engine->saveCopy(filePath());
}

void Backend::saveAsDocument() {
    if (!hasDocument() || !m_picker) return;
    m_picker->saveFile(fileName(), QStringLiteral("Save Document As"));
}

void Backend::closeDocument() {
    if (m_engine) {
        m_engine->close();
        if (m_provider) m_provider->clearCache();
        m_currentPage = 0;
        m_rotation = 0;
        m_toc.clear();
        clearSearch();
        ++m_revision;

        emit documentChanged();
        emit currentPageChanged();
        emit rotationChanged();
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
    m_fitMode = 0;
    emit fitModeChanged();
    setZoom(m_zoom * 1.25);
}

void Backend::zoomOut() {
    m_fitMode = 0;
    emit fitModeChanged();
    setZoom(m_zoom / 1.25);
}

void Backend::resetZoom() {
    m_fitMode = 0;
    emit fitModeChanged();
    setZoom(1.0);
}

void Backend::fitToWidth(double viewportWidth) {
    const QSizeF ps = pageSize(m_currentPage);
    const double w = (m_rotation == 90 || m_rotation == 270) ? ps.height() : ps.width();
    if (w > 0.0 && viewportWidth > 64.0) {
        // Render base DPI is 140 / 72.0
        const double baseWidth = w * (140.0 / 72.0);
        const double targetZoom = (viewportWidth - 48.0) / baseWidth;
        setZoom(targetZoom);
        setFitMode(1);
    }
}

void Backend::fitToPage(double viewportWidth, double viewportHeight) {
    const QSizeF ps = pageSize(m_currentPage);
    const double w = (m_rotation == 90 || m_rotation == 270) ? ps.height() : ps.width();
    const double h = (m_rotation == 90 || m_rotation == 270) ? ps.width() : ps.height();
    if (w > 0.0 && h > 0.0 && viewportWidth > 64.0 && viewportHeight > 64.0) {
        const double baseWidth = w * (140.0 / 72.0);
        const double baseHeight = h * (140.0 / 72.0);
        const double zW = (viewportWidth - 48.0) / baseWidth;
        const double zH = (viewportHeight - 64.0) / baseHeight;
        setZoom(std::min(zW, zH));
        setFitMode(2);
    }
}

void Backend::toggleDarkMode() {
    setDarkMode(!m_darkMode);
}

void Backend::rotateClockwise() {
    setRotation(m_rotation + 90);
}

void Backend::rotateCounterClockwise() {
    setRotation(m_rotation - 90);
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

QString Backend::pageImageUrl(int pageIndex, double dpi, int rot, int dark) const {
    const int effectiveRot = (rot >= 0) ? rot : m_rotation;
    const bool effectiveDark = (dark >= 0) ? (dark != 0) : m_darkMode;
    const QString darkFlag = effectiveDark ? QStringLiteral("1") : QStringLiteral("0");
    return QStringLiteral("image://doc/%1_%2_%3_%4_%5")
        .arg(pageIndex)
        .arg(static_cast<int>(dpi))
        .arg(darkFlag)
        .arg(effectiveRot)
        .arg(m_revision);
}

QSizeF Backend::pageSize(int pageIndex) const {
    return m_engine ? m_engine->pageSize(pageIndex) : QSizeF(0, 0);
}

double Backend::pageAspectRatio(int pageIndex) const {
    const QSizeF ps = pageSize(pageIndex);
    if (ps.height() <= 0.0) return 0.707; // standard A4
    if (m_rotation == 90 || m_rotation == 270)
        return ps.height() / ps.width();
    return ps.width() / ps.height();
}

void Backend::addRecentFile(const QString &path) {
    if (path.isEmpty()) return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 8) {
        m_recentFiles.removeLast();
    }
    saveRecents();
    emit recentFilesChanged();
}

void Backend::clearRecents() {
    m_recentFiles.clear();
    saveRecents();
    emit recentFilesChanged();
}

void Backend::loadRecents() {
    QSettings settings(QStringLiteral("omarchy"), QStringLiteral("omadoc"));
    m_recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
}

void Backend::saveRecents() {
    QSettings settings(QStringLiteral("omarchy"), QStringLiteral("omadoc"));
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
}
