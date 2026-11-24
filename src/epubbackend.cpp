#include "epubbackend.h"

#include <archive.h>
#include <archive_entry.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>

namespace {

QString normalizePath(const QString &path) {
    QString p = path;
    p.replace('\\', '/');
    while (p.startsWith('/')) p.remove(0, 1);
    return QDir::cleanPath(p);
}

QString resolvePath(const QString &baseDir, const QString &relative) {
    if (relative.startsWith('/')) return normalizePath(relative);
    if (baseDir.isEmpty() || baseDir == QStringLiteral(".")) return normalizePath(relative);
    return normalizePath(baseDir + QStringLiteral("/") + relative);
}

} // anonymous namespace

EpubBackend::EpubBackend() = default;

EpubBackend::~EpubBackend() {
    close();
}

bool EpubBackend::load(const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    close();

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
    QHash<QString, QByteArray> files;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *rawPath = archive_entry_pathname(entry);
        if (!rawPath) continue;
        const QString name = normalizePath(QString::fromUtf8(rawPath));

        if (name.endsWith('/') || name.contains(QStringLiteral("__MACOSX"))) {
            archive_read_data_skip(a);
            continue;
        }

        la_int64_t sz = archive_entry_size(entry);
        QByteArray data;
        if (sz > 0) {
            data.resize(sz);
            archive_read_data(a, data.data(), sz);
        } else {
            char buf[16384];
            ssize_t n;
            while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) {
                data.append(buf, n);
            }
        }
        files.insert(name, data);
    }

    archive_read_close(a);
    archive_read_free(a);

    if (files.isEmpty()) {
        return false;
    }

    // 1. Locate rootfile from META-INF/container.xml
    QString opfPath;
    const QByteArray containerXml = files.value(QStringLiteral("META-INF/container.xml"));
    if (!containerXml.isEmpty()) {
        QXmlStreamReader xml(containerXml);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("rootfile")) {
                opfPath = xml.attributes().value(QStringLiteral("full-path")).toString();
                if (!opfPath.isEmpty()) break;
            }
        }
    }

    if (opfPath.isEmpty()) {
        // Fallback: look for any .opf file in the archive
        for (auto it = files.keyBegin(); it != files.keyEnd(); ++it) {
            if (it->endsWith(QStringLiteral(".opf"), Qt::CaseInsensitive)) {
                opfPath = *it;
                break;
            }
        }
    }

    opfPath = normalizePath(opfPath);
    if (opfPath.isEmpty() || !files.contains(opfPath)) {
        return false;
    }

    const QString opfDir = QFileInfo(opfPath).path();

    // 2. Parse OPF package file
    const QByteArray opfData = files.value(opfPath);
    QXmlStreamReader xml(opfData);

    QString title;
    QString author;
    QHash<QString, QString> manifestHref; // id -> resolved path
    QHash<QString, QString> manifestType;
    QList<QString> spineIdrefs;
    QString ncxId;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString name = xml.name().toString();
            if (name == QLatin1String("title") && title.isEmpty()) {
                title = xml.readElementText().trimmed();
            } else if (name == QLatin1String("creator") && author.isEmpty()) {
                author = xml.readElementText().trimmed();
            } else if (name == QLatin1String("item")) {
                const auto attrs = xml.attributes();
                const QString id = attrs.value(QStringLiteral("id")).toString();
                const QString href = attrs.value(QStringLiteral("href")).toString();
                const QString mediaType = attrs.value(QStringLiteral("media-type")).toString();
                if (!id.isEmpty() && !href.isEmpty()) {
                    manifestHref.insert(id, resolvePath(opfDir, href));
                    manifestType.insert(id, mediaType);
                }
            } else if (name == QLatin1String("spine")) {
                ncxId = xml.attributes().value(QStringLiteral("toc")).toString();
            } else if (name == QLatin1String("itemref")) {
                const QString idref = xml.attributes().value(QStringLiteral("idref")).toString();
                if (!idref.isEmpty()) {
                    spineIdrefs.append(idref);
                }
            }
        }
    }

    if (spineIdrefs.isEmpty()) {
        // If no spine, fallback to manifest HTML/image items
        for (auto it = manifestType.constBegin(); it != manifestType.constEnd(); ++it) {
            if (it.value().contains(QStringLiteral("html")) || it.value().contains(QStringLiteral("xml"))) {
                spineIdrefs.append(it.key());
            }
        }
    }

    // 3. Build pages from spine
    QList<EpubPageInfo> pages;

    for (int spineIdx = 0; spineIdx < spineIdrefs.size(); ++spineIdx) {
        const QString id = spineIdrefs.at(spineIdx);
        const QString resolvedPath = manifestHref.value(id);
        if (resolvedPath.isEmpty() || !files.contains(resolvedPath)) continue;

        const QString chapterDir = QFileInfo(resolvedPath).path();
        const QByteArray content = files.value(resolvedPath);

        // Check if this chapter is a manga scan or single-image page
        // Patterns: <img src="..." /> or <image ... xlink:href="..." />
        QString foundImgSrc;
        const QString contentStr = QString::fromUtf8(content);

        // Match <image ... xlink:href="X" or href="X"
        static const QRegularExpression svgImgRegex(QStringLiteral("<image[^>]+(?:href|xlink:href)=[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression htmlImgRegex(QStringLiteral("<img[^>]+src=[\"']([^\"']+)[\"']"), QRegularExpression::CaseInsensitiveOption);

        auto match = svgImgRegex.match(contentStr);
        if (match.hasMatch()) {
            foundImgSrc = match.captured(1);
        } else {
            match = htmlImgRegex.match(contentStr);
            if (match.hasMatch()) {
                foundImgSrc = match.captured(1);
            }
        }

        // Count approximate text length outside tags
        QString strippedText = contentStr;
        strippedText.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        strippedText = strippedText.simplified();

        bool isMangaScan = false;
        if (!foundImgSrc.isEmpty() && strippedText.length() < 80) {
            QString resolvedImg = resolvePath(chapterDir, foundImgSrc);
            if (files.contains(resolvedImg)) {
                isMangaScan = true;
                EpubPageInfo page;
                page.spineIndex = spineIdx;
                page.subPage = 0;
                page.totalSubPages = 1;
                page.href = resolvedPath;
                page.imageHref = resolvedImg;

                // Inspect image size
                QByteArray imgBytes = files.value(resolvedImg);
                QBuffer buf(&imgBytes);
                buf.open(QIODevice::ReadOnly);
                QImageReader rdr(&buf);
                const QSize sz = rdr.size();
                if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                    page.size = QSizeF(sz.width() * (72.0 / 140.0), sz.height() * (72.0 / 140.0));
                } else {
                    page.size = QSizeF(595, 842);
                }
                pages.append(page);
            }
        }

        if (!isMangaScan) {
            // Text Chapter: paginate using QTextDocument
            QTextDocument doc;
            doc.setDefaultStyleSheet(QStringLiteral("body { font-size: 14pt; line-height: 1.6; }"));
            doc.setPageSize(QSizeF(595, 842));
            doc.setHtml(contentStr);

            const int totalSubs = std::max(1, doc.pageCount());
            for (int sub = 0; sub < totalSubs; ++sub) {
                EpubPageInfo page;
                page.spineIndex = spineIdx;
                page.subPage = sub;
                page.totalSubPages = totalSubs;
                page.href = resolvedPath;
                page.htmlContent = content;
                page.size = QSizeF(595, 842);
                pages.append(page);
            }
        }
    }

    if (pages.isEmpty()) {
        return false;
    }

    // 4. Parse Table of Contents from NCX if present
    QVariantList toc;
    QString ncxPath;
    if (!ncxId.isEmpty()) {
        ncxPath = manifestHref.value(ncxId);
    }
    if (ncxPath.isEmpty() || !files.contains(ncxPath)) {
        for (auto it = files.keyBegin(); it != files.keyEnd(); ++it) {
            if (it->endsWith(QStringLiteral(".ncx"), Qt::CaseInsensitive)) {
                ncxPath = *it;
                break;
            }
        }
    }

    if (!ncxPath.isEmpty() && files.contains(ncxPath)) {
        const QString ncxDir = QFileInfo(ncxPath).path();
        const QByteArray ncxData = files.value(ncxPath);
        QXmlStreamReader ncxXml(ncxData);

        QString currentLabel;
        int depth = 0;

        while (!ncxXml.atEnd()) {
            ncxXml.readNext();
            if (ncxXml.isStartElement()) {
                if (ncxXml.name() == QLatin1String("navPoint")) {
                    depth++;
                } else if (ncxXml.name() == QLatin1String("text") && currentLabel.isEmpty()) {
                    currentLabel = ncxXml.readElementText().trimmed();
                } else if (ncxXml.name() == QLatin1String("content")) {
                    QString src = ncxXml.attributes().value(QStringLiteral("src")).toString();
                    if (src.contains('#')) src = src.left(src.indexOf('#'));
                    const QString targetPath = resolvePath(ncxDir, src);

                    // Find matching page index
                    int pageMatch = -1;
                    for (int p = 0; p < pages.size(); ++p) {
                        if (pages.at(p).href == targetPath) {
                            pageMatch = p;
                            break;
                        }
                    }

                    if (!currentLabel.isEmpty()) {
                        QVariantMap item;
                        item[QStringLiteral("title")] = currentLabel;
                        item[QStringLiteral("depth")] = std::max(0, depth - 1);
                        item[QStringLiteral("page")] = (pageMatch >= 0) ? pageMatch : 0;
                        toc.append(item);
                        currentLabel.clear();
                    }
                }
            } else if (ncxXml.isEndElement() && ncxXml.name() == QLatin1String("navPoint")) {
                depth = std::max(0, depth - 1);
            }
        }
    }

    m_filePath = filePath;
    m_title = title.isEmpty() ? QFileInfo(filePath).completeBaseName() : title;
    m_author = author;
    m_files = std::move(files);
    m_pages = std::move(pages);
    m_toc = std::move(toc);
    return true;
}

void EpubBackend::close() {
    m_files.clear();
    m_pages.clear();
    m_toc.clear();
    m_filePath.clear();
    m_title.clear();
    m_author.clear();
}

bool EpubBackend::saveCopy(const QString &targetPath) {
    QMutexLocker locker(&m_mutex);
    if (m_filePath.isEmpty() || targetPath.isEmpty())
        return false;
    if (QFileInfo(m_filePath) == QFileInfo(targetPath))
        return true;
    if (QFile::exists(targetPath))
        QFile::remove(targetPath);
    return QFile::copy(m_filePath, targetPath);
}

int EpubBackend::pageCount() const {
    QMutexLocker locker(&m_mutex);
    return m_pages.size();
}

QSizeF EpubBackend::pageSize(int index) const {
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_pages.size())
        return QSizeF(0, 0);
    return m_pages.at(index).size;
}

QImage EpubBackend::renderPage(int index, double dpi, bool darkMode, int rotation) {
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_pages.size())
        return QImage();

    const EpubPageInfo &info = m_pages.at(index);

    // Case 1: Manga / Graphic scan page
    if (!info.imageHref.isEmpty() && m_files.contains(info.imageHref)) {
        QImage img;
        img.loadFromData(m_files.value(info.imageHref));
        if (img.isNull()) return QImage();

        if (dpi > 10.0 && info.size.width() > 0) {
            const int targetW = static_cast<int>(std::round(info.size.width() * (dpi / 72.0)));
            const int targetH = static_cast<int>(std::round(info.size.height() * (dpi / 72.0)));
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
            img.invertPixels(QImage::InvertRgb);
        }

        return img;
    }

    // Case 2: Rich Text Page
    const int targetW = static_cast<int>(std::round(info.size.width() * (dpi / 72.0)));
    const int targetH = static_cast<int>(std::round(info.size.height() * (dpi / 72.0)));
    const int w = std::max(200, targetW);
    const int h = std::max(200, targetH);

    QTextDocument doc;
    doc.setPageSize(QSizeF(w, h));

    if (darkMode) {
        doc.setDefaultStyleSheet(QStringLiteral("body { color: #f3f4f6; background-color: #12141a; font-family: sans-serif; font-size: 13pt; line-height: 1.6; margin: 32px; } a { color: #93c5fd; } h1, h2, h3 { color: #ffffff; }"));
    } else {
        doc.setDefaultStyleSheet(QStringLiteral("body { color: #111827; background-color: #ffffff; font-family: sans-serif; font-size: 13pt; line-height: 1.6; margin: 32px; } a { color: #2563eb; } h1, h2, h3 { color: #000000; }"));
    }

    // Register images from archive into QTextDocument resources
    const QString chapterDir = QFileInfo(info.href).path();
    for (auto it = m_files.constBegin(); it != m_files.constEnd(); ++it) {
        const QString k = it.key();
        if (k.endsWith(QStringLiteral(".jpg")) || k.endsWith(QStringLiteral(".jpeg")) ||
            k.endsWith(QStringLiteral(".png")) || k.endsWith(QStringLiteral(".webp")) ||
            k.endsWith(QStringLiteral(".svg"))) {
            QImage resImg;
            resImg.loadFromData(it.value());
            if (!resImg.isNull()) {
                doc.addResource(QTextDocument::ImageResource, QUrl(k), resImg);
                QFileInfo fi(k);
                doc.addResource(QTextDocument::ImageResource, QUrl(fi.fileName()), resImg);
                // Also relative to chapterDir
                if (k.startsWith(chapterDir + QStringLiteral("/"))) {
                    QString rel = k.mid(chapterDir.length() + 1);
                    doc.addResource(QTextDocument::ImageResource, QUrl(rel), resImg);
                }
            }
        }
    }

    doc.setHtml(QString::fromUtf8(info.htmlContent));

    QImage result(w, h, QImage::Format_ARGB32);
    result.fill(darkMode ? QColor(QStringLiteral("#12141a")) : Qt::white);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal pageOffsetY = info.subPage * h;
    painter.translate(0, -pageOffsetY);
    const QRectF pageRect(0, pageOffsetY, w, h);
    painter.setClipRect(pageRect);
    doc.drawContents(&painter, pageRect);
    painter.end();

    if (rotation != 0) {
        QTransform t;
        t.rotate(rotation);
        result = result.transformed(t);
    }

    return result;
}

QString EpubBackend::title() const {
    QMutexLocker locker(&m_mutex);
    return m_title;
}

QString EpubBackend::author() const {
    QMutexLocker locker(&m_mutex);
    return m_author;
}

QVariantList EpubBackend::tableOfContents() {
    QMutexLocker locker(&m_mutex);
    return m_toc;
}

QList<int> EpubBackend::search(const QString &query) const {
    QMutexLocker locker(&m_mutex);
    QList<int> results;
    if (query.trimmed().isEmpty()) return results;

    for (int i = 0; i < m_pages.size(); ++i) {
        const auto &info = m_pages.at(i);
        if (!info.htmlContent.isEmpty()) {
            const QString text = QString::fromUtf8(info.htmlContent);
            if (text.contains(query, Qt::CaseInsensitive)) {
                results.append(i);
            }
        }
    }
    return results;
}
