#include "comicbackend.h"
#include "archiveutils.h"

#include <archive.h>
#include <archive_entry.h>

#include <QBuffer>
#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>

ComicBackend::ComicBackend() = default;

ComicBackend::~ComicBackend() {
    close();
}

void ComicBackend::clearUnlocked() {
    m_pages.clear();
    m_filePath.clear();
    m_title.clear();
    m_author.clear();
}

bool ComicBackend::load(const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    clearUnlocked();

    struct archive *a = archive_read_new();
    if (!a) return false;
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, filePath.toUtf8().constData(), 16384);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry *entry = nullptr;
    QList<ComicPageEntry> pages;
    QString detectedTitle;
    QString detectedAuthor;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *rawPath = archive_entry_pathname(entry);
        if (!rawPath) {
            archive_read_data_skip(a);
            continue;
        }
        QString name = QString::fromUtf8(rawPath);
        name.replace('\\', '/');
        while (name.startsWith(QStringLiteral("./"))) name.remove(0, 2);
        while (name.startsWith('/')) name.remove(0, 1);
        name = QDir::cleanPath(name);

        const QString baseName = QFileInfo(name).fileName();
        // Skip directories, root ".", macOS resource forks, and dotfiles
        if (archive_entry_filetype(entry) == AE_IFDIR || name.isEmpty() ||
            name == QStringLiteral(".") || name.endsWith('/') ||
            name.contains(QStringLiteral("__MACOSX")) || baseName.startsWith('.')) {
            archive_read_data_skip(a);
            continue;
        }

        // Check for ComicInfo.xml metadata
        if (baseName.compare(QStringLiteral("ComicInfo.xml"), Qt::CaseInsensitive) == 0) {
            bool ok = false;
            const QByteArray xmlData = OmadocUtils::readArchiveEntryData(a, entry, 16LL * 1024 * 1024, &ok);
            if (!ok) continue;
            QXmlStreamReader xml(xmlData);
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    if (xml.name() == QLatin1String("Title") && detectedTitle.isEmpty()) {
                        detectedTitle = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                    } else if (xml.name() == QLatin1String("Writer") && detectedAuthor.isEmpty()) {
                        detectedAuthor = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                    }
                }
            }
            continue;
        }

        // Check if image
        const QString lower = name.toLower();
        if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg")) ||
            lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".webp")) ||
            lower.endsWith(QStringLiteral(".avif")) || lower.endsWith(QStringLiteral(".gif")) ||
            lower.endsWith(QStringLiteral(".bmp"))) {

            bool ok = false;
            const QByteArray data = OmadocUtils::readArchiveEntryData(a, entry, 128LL * 1024 * 1024, &ok);
            if (ok && !data.isEmpty()) {
                ComicPageEntry p;
                p.name = name;
                p.data = data;
                pages.append(p);
            }
        } else {
            archive_read_data_skip(a);
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    if (pages.isEmpty()) {
        return false;
    }

    // Natural numerical sorting so "page_1.jpg", "page_2.jpg", "page_10.jpg" sort correctly
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(pages.begin(), pages.end(), [&collator](const ComicPageEntry &x, const ComicPageEntry &y) {
        return collator.compare(x.name, y.name) < 0;
    });

    // Inspect natural image dimensions via QImageReader
    for (auto &p : pages) {
        QBuffer buffer(&p.data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize sz = reader.size();
        if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
            p.size = QSizeF(sz.width() * (72.0 / 140.0), sz.height() * (72.0 / 140.0));
        } else {
            p.size = QSizeF(595, 842);
        }
    }

    m_filePath = filePath;
    m_title = detectedTitle.isEmpty() ? QFileInfo(filePath).completeBaseName() : detectedTitle;
    m_author = detectedAuthor;
    m_pages = std::move(pages);
    return true;
}

void ComicBackend::close() {
    QMutexLocker locker(&m_mutex);
    clearUnlocked();
}

bool ComicBackend::saveCopy(const QString &targetPath) {
    QMutexLocker locker(&m_mutex);
    return OmadocUtils::safeCopyFile(m_filePath, targetPath);
}

int ComicBackend::pageCount() const {
    QMutexLocker locker(&m_mutex);
    return m_pages.size();
}

QSizeF ComicBackend::pageSize(int index) const {
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_pages.size())
        return QSizeF(0, 0);
    return m_pages.at(index).size;
}

QImage ComicBackend::renderPage(int index, double dpi, bool darkMode, int rotation) {
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_pages.size())
        return QImage();

    QImage img;
    img.loadFromData(m_pages.at(index).data);
    if (img.isNull())
        return QImage();

    if (dpi > 10.0 && m_pages.at(index).size.width() > 0) {
        const int targetW = static_cast<int>(std::round(m_pages.at(index).size.width() * (dpi / 72.0)));
        const int targetH = static_cast<int>(std::round(m_pages.at(index).size.height() * (dpi / 72.0)));
        if (std::abs(img.width() - targetW) > 10 && targetW > 0 && targetH > 0) {
            img = img.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (rotation != 0) {
        QTransform t;
        t.rotate(rotation);
        img = img.transformed(t);
    }

    if (darkMode) {
        OmadocUtils::invertImageRgbSafely(img);
    }

    return img;
}

QString ComicBackend::title() const {
    QMutexLocker locker(&m_mutex);
    return m_title;
}

QString ComicBackend::author() const {
    QMutexLocker locker(&m_mutex);
    return m_author;
}

QVariantList ComicBackend::tableOfContents() {
    QMutexLocker locker(&m_mutex);
    QVariantList toc;
    QString lastDir;
    for (int i = 0; i < m_pages.size(); ++i) {
        const QFileInfo fi(m_pages.at(i).name);
        const QString dir = fi.path();
        if (!dir.isEmpty() && dir != QStringLiteral(".") && dir != lastDir) {
            lastDir = dir;
            QVariantMap item;
            item[QStringLiteral("title")] = dir;
            item[QStringLiteral("depth")] = 0;
            item[QStringLiteral("page")] = i;
            toc.append(item);
        }
    }
    return toc;
}

QList<int> ComicBackend::search(const QString &query) const {
    QMutexLocker locker(&m_mutex);
    QList<int> results;
    if (query.trimmed().isEmpty()) return results;
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i).name.contains(query, Qt::CaseInsensitive)) {
            results.append(i);
        }
    }
    return results;
}
