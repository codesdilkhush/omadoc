#include "backend.h"
#include "documentengine.h"
#include "pageimageprovider.h"
#include "portalfilepicker.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {

// Helper: map rendered coordinates (rx, ry, rw, rh) to unrotated page coordinates (ux, uy, uw, uh)
void renderedToUnrotated(int rotation, double rx, double ry, double rw, double rh,
                         double &ux, double &uy, double &uw, double &uh) {
    const int rot = (rotation % 360 + 360) % 360;
    if (rot == 90) {
        ux = ry;
        uy = 1.0 - rx - rw;
        uw = rh;
        uh = rw;
    } else if (rot == 180) {
        ux = 1.0 - rx - rw;
        uy = 1.0 - ry - rh;
        uw = rw;
        uh = rh;
    } else if (rot == 270) {
        ux = 1.0 - ry - rh;
        uy = rx;
        uw = rh;
        uh = rw;
    } else { // 0
        ux = rx;
        uy = ry;
        uw = rw;
        uh = rh;
    }
    ux = std::clamp(ux, 0.0, 1.0);
    uy = std::clamp(uy, 0.0, 1.0);
    uw = std::clamp(uw, 0.0, 1.0 - ux);
    uh = std::clamp(uh, 0.0, 1.0 - uy);
}

// Helper: map unrotated page coordinates (ux, uy, uw, uh) to rendered coordinates (rx, ry, rw, rh)
void unrotatedToRendered(int rotation, double ux, double uy, double uw, double uh,
                         double &rx, double &ry, double &rw, double &rh) {
    const int rot = (rotation % 360 + 360) % 360;
    if (rot == 90) {
        rx = 1.0 - uy - uh;
        ry = ux;
        rw = uh;
        rh = uw;
    } else if (rot == 180) {
        rx = 1.0 - ux - uw;
        ry = 1.0 - uy - uh;
        rw = uw;
        rh = uh;
    } else if (rot == 270) {
        rx = uy;
        ry = 1.0 - ux - uw;
        rw = uh;
        rh = uw;
    } else { // 0
        rx = ux;
        ry = uy;
        rw = uw;
        rh = uh;
    }
    rx = std::clamp(rx, 0.0, 1.0);
    ry = std::clamp(ry, 0.0, 1.0);
    rw = std::clamp(rw, 0.0, 1.0 - rx);
    rh = std::clamp(rh, 0.0, 1.0 - ry);
}

} // anonymous namespace

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
        emit annotationsChanged();
    }
}

void Backend::setActiveTool(int tool) {
    if (tool != m_activeTool) {
        m_activeTool = tool;
        emit activeToolChanged();
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
        loadAnnotations();

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
        m_annotations.clear();
        clearSearch();
        ++m_revision;

        emit documentChanged();
        emit currentPageChanged();
        emit rotationChanged();
        emit annotationsChanged();
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

QVariantList Backend::pageAnnotations(int pageIndex) const {
    QVariantList list;
    for (const QVariant &var : m_annotations) {
        QVariantMap map = var.toMap();
        if (map.value(QStringLiteral("page")).toInt() == pageIndex) {
            const double ux = map.value(QStringLiteral("ux"), map.value(QStringLiteral("x"))).toDouble();
            const double uy = map.value(QStringLiteral("uy"), map.value(QStringLiteral("y"))).toDouble();
            const double uw = map.value(QStringLiteral("uw"), map.value(QStringLiteral("width"))).toDouble();
            const double uh = map.value(QStringLiteral("uh"), map.value(QStringLiteral("height"))).toDouble();

            double rx = 0, ry = 0, rw = 0, rh = 0;
            unrotatedToRendered(m_rotation, ux, uy, uw, uh, rx, ry, rw, rh);

            map[QStringLiteral("x")] = rx;
            map[QStringLiteral("y")] = ry;
            map[QStringLiteral("width")] = rw;
            map[QStringLiteral("height")] = rh;
            list.append(map);
        }
    }
    return list;
}

void Backend::addHighlight(int page, double x, double y, double width, double height, const QString &color) {
    if (std::abs(width) <= 0.0001 || std::abs(height) <= 0.0001) return;

    double rx = (width < 0) ? x + width : x;
    double ry = (height < 0) ? y + height : y;
    double rw = std::abs(width);
    double rh = std::abs(height);

    double ux = 0, uy = 0, uw = 0, uh = 0;
    renderedToUnrotated(m_rotation, rx, ry, rw, rh, ux, uy, uw, uh);

    const QString id = QStringLiteral("hl_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(1000, 9999));

    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("type")] = QStringLiteral("highlight");
    map[QStringLiteral("page")] = page;
    map[QStringLiteral("ux")] = ux;
    map[QStringLiteral("uy")] = uy;
    map[QStringLiteral("uw")] = uw;
    map[QStringLiteral("uh")] = uh;
    map[QStringLiteral("color")] = color.isEmpty() ? QStringLiteral("#ffe082") : color;
    map[QStringLiteral("created")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_annotations.append(map);
    saveAnnotations();
    emit annotationsChanged();
}

QString Backend::addStickyNote(int page, double x, double y, const QString &content, const QString &color) {
    double ux = 0, uy = 0, uw = 0, uh = 0;
    renderedToUnrotated(m_rotation, x, y, 0.0, 0.0, ux, uy, uw, uh);

    const QString id = QStringLiteral("sn_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->bounded(1000, 9999));

    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("type")] = QStringLiteral("sticky");
    map[QStringLiteral("page")] = page;
    map[QStringLiteral("ux")] = ux;
    map[QStringLiteral("uy")] = uy;
    map[QStringLiteral("uw")] = 0.0;
    map[QStringLiteral("uh")] = 0.0;
    map[QStringLiteral("content")] = content;
    map[QStringLiteral("color")] = color.isEmpty() ? QStringLiteral("#ffd54f") : color;
    map[QStringLiteral("created")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    map[QStringLiteral("modified")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    m_annotations.append(map);
    saveAnnotations();
    emit annotationsChanged();
    return id;
}

void Backend::updateStickyNote(const QString &id, const QString &content) {
    bool updated = false;
    for (int i = 0; i < m_annotations.size(); ++i) {
        QVariantMap map = m_annotations.at(i).toMap();
        if (map.value(QStringLiteral("id")).toString() == id) {
            map[QStringLiteral("content")] = content;
            map[QStringLiteral("modified")] = QDateTime::currentDateTime().toString(Qt::ISODate);
            m_annotations[i] = map;
            updated = true;
            break;
        }
    }
    if (updated) {
        saveAnnotations();
        emit annotationsChanged();
    }
}

void Backend::deleteAnnotation(const QString &id) {
    bool found = false;
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (m_annotations.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
            m_annotations.removeAt(i);
            found = true;
            break;
        }
    }
    if (found) {
        saveAnnotations();
        emit annotationsChanged();
    }
}

void Backend::clearAnnotationsForPage(int page) {
    bool found = false;
    for (int i = m_annotations.size() - 1; i >= 0; --i) {
        if (m_annotations.at(i).toMap().value(QStringLiteral("page")).toInt() == page) {
            m_annotations.removeAt(i);
            found = true;
        }
    }
    if (found) {
        saveAnnotations();
        emit annotationsChanged();
    }
}

void Backend::clearAllAnnotations() {
    if (!m_annotations.isEmpty()) {
        m_annotations.clear();
        saveAnnotations();
        emit annotationsChanged();
    }
}

void Backend::loadAnnotations() {
    m_annotations.clear();
    const QString path = annotationFilePath();
    if (path.isEmpty() || !QFile::exists(path)) {
        emit annotationsChanged();
        return;
    }
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &val : arr) {
                if (val.isObject()) {
                    m_annotations.append(val.toObject().toVariantMap());
                }
            }
        }
    }
    emit annotationsChanged();
}

void Backend::saveAnnotations() {
    const QString path = annotationFilePath();
    if (path.isEmpty()) return;
    QJsonArray arr;
    for (const QVariant &var : m_annotations) {
        arr.append(QJsonObject::fromVariantMap(var.toMap()));
    }
    QJsonDocument doc(arr);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

QString Backend::annotationFilePath() const {
    const QString path = filePath();
    if (path.isEmpty()) return QString();
    const QString canonical = QFileInfo(path).canonicalFilePath();
    const QString targetPath = canonical.isEmpty() ? path : canonical;
    const QString hash = QString::fromUtf8(QCryptographicHash::hash(targetPath.toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/omadoc/annotations");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/") + hash + QStringLiteral(".json");
}
