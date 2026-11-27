#include "epubbackend.h"
#include "archiveutils.h"

#include <archive.h>
#include <archive_entry.h>

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTextLine>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kEpubLogicalWidth = 595.0;
constexpr qreal kEpubLogicalHeight = 842.0;
constexpr int kEpubMaxContentImgWidth = 530;

struct RawTocEntry {
    QString title;
    int depth = 0;
    QString targetPath;
    QString fragment;
};

QString normalizePath(const QString &path) {
    QString p = path;
    p.replace('\\', '/');
    while (p.startsWith('/')) p.remove(0, 1);
    while (p.startsWith(QStringLiteral("./"))) p.remove(0, 2);
    p = QDir::cleanPath(p);
    if (p == QStringLiteral(".")) return QString();
    return p;
}

QString normalizeDir(const QString &filePath) {
    const QString d = QFileInfo(filePath).path();
    return (d == QStringLiteral(".")) ? QString() : d;
}

QString resolvePath(const QString &baseDir, const QString &rawRelative) {
    QString rel = QUrl::fromPercentEncoding(rawRelative.toUtf8());
    if (rel.startsWith('/')) return normalizePath(rel);
    if (baseDir.isEmpty() || baseDir == QStringLiteral(".")) return normalizePath(rel);
    return normalizePath(baseDir + QStringLiteral("/") + rel);
}

QString lookupArchiveKey(const QHash<QString, QByteArray> &files, const QString &candidate) {
    if (candidate.isEmpty()) return QString();
    if (files.contains(candidate)) return candidate;

    const QString decoded = normalizePath(QUrl::fromPercentEncoding(candidate.toUtf8()));
    if (!decoded.isEmpty() && files.contains(decoded)) return decoded;

    for (auto it = files.keyBegin(); it != files.keyEnd(); ++it) {
        if (it->compare(decoded, Qt::CaseInsensitive) == 0) {
            return *it;
        }
    }
    return QString();
}

QString decodeHtmlBytes(const QByteArray &bytes) {
    if (bytes.isEmpty()) return QString();

    const QByteArray head = bytes.left(1024);
    static const QRegularExpression encRegex(
        QStringLiteral("(?:encoding|charset)=[\"']?([A-Za-z0-9_\\-]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = encRegex.match(QString::fromLatin1(head));
    if (match.hasMatch()) {
        const QString enc = match.captured(1).trimmed();
        if (enc.compare(QStringLiteral("utf-8"), Qt::CaseInsensitive) != 0 &&
            enc.compare(QStringLiteral("utf8"), Qt::CaseInsensitive) != 0) {
            QStringDecoder decoder(enc.toUtf8().constData());
            if (decoder.isValid()) {
                return decoder.decode(bytes);
            }
        }
    }
    return QString::fromUtf8(bytes);
}

QString stripNonVisibleText(QString html) {
    static const QRegularExpression commentRe(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression headRe(QStringLiteral("<head\\b[^>]*>.*?</head>"),
                                           QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression styleRe(QStringLiteral("<style\\b[^>]*>.*?</style>"),
                                            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression scriptRe(QStringLiteral("<script\\b[^>]*>.*?</script>"),
                                             QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression titleRe(QStringLiteral("<title\\b[^>]*>.*?</title>"),
                                            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tagRe(QStringLiteral("<[^>]*>"));

    html.remove(commentRe);
    html.remove(headRe);
    html.remove(styleRe);
    html.remove(scriptRe);
    html.remove(titleRe);
    html.remove(tagRe);
    return html.simplified();
}

bool isSupportedImageExtension(const QString &path) {
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg")) ||
           lower.endsWith(QStringLiteral(".png")) || lower.endsWith(QStringLiteral(".webp")) ||
           lower.endsWith(QStringLiteral(".svg")) || lower.endsWith(QStringLiteral(".gif")) ||
           lower.endsWith(QStringLiteral(".bmp"));
}

void setupChapterDocument(QTextDocument &doc,
                          const QString &chapterHref,
                          const QString &htmlText,
                          const QHash<QString, QByteArray> &files,
                          bool darkMode,
                          QHash<QString, QImage> *sharedImageCache = nullptr) {
    const QString chapterDir = normalizeDir(chapterHref);
    const QString baseUrlStr = chapterDir.isEmpty()
        ? QStringLiteral("epub:///")
        : QStringLiteral("epub:///") + chapterDir + QStringLiteral("/");
    doc.setBaseUrl(QUrl(baseUrlStr));
    doc.setPageSize(QSizeF(kEpubLogicalWidth, kEpubLogicalHeight));

    if (darkMode) {
        doc.setDefaultStyleSheet(QStringLiteral(
            "body { color: #f3f4f6; background-color: #12141a; font-family: sans-serif; "
            "font-size: 12pt; line-height: 1.55; margin: 28px; } "
            "a { color: #93c5fd; } h1, h2, h3, h4 { color: #ffffff; }"));
    } else {
        doc.setDefaultStyleSheet(QStringLiteral(
            "body { color: #111827; background-color: #ffffff; font-family: sans-serif; "
            "font-size: 12pt; line-height: 1.55; margin: 28px; } "
            "a { color: #2563eb; } h1, h2, h3, h4 { color: #000000; }"));
    }

    // Extract and register only image resources referenced by this chapter
    static const QRegularExpression imgAttrRe(
        QStringLiteral("<(?:img|image)\\b[^>]*?(?:src|xlink:href|href)=[\"']([^\"']+)[\"']"),
        QRegularExpression::CaseInsensitiveOption);

    QSet<QString> seenSrcs;
    auto it = imgAttrRe.globalMatch(htmlText);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString rawSrc = match.captured(1).trimmed();
        if (rawSrc.isEmpty() || rawSrc.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive) ||
            seenSrcs.contains(rawSrc)) {
            continue;
        }
        seenSrcs.insert(rawSrc);

        const QString resolvedCandidate = resolvePath(chapterDir, rawSrc);
        const QString key = lookupArchiveKey(files, resolvedCandidate);
        if (key.isEmpty() || !isSupportedImageExtension(key)) {
            continue;
        }

        QImage resImg;
        if (sharedImageCache && sharedImageCache->contains(key)) {
            resImg = sharedImageCache->value(key);
        } else {
            resImg.loadFromData(files.value(key));
            if (!resImg.isNull() && resImg.width() > kEpubMaxContentImgWidth) {
                resImg = resImg.scaledToWidth(kEpubMaxContentImgWidth, Qt::SmoothTransformation);
            }
            if (sharedImageCache && !resImg.isNull()) {
                sharedImageCache->insert(key, resImg);
            }
        }

        if (!resImg.isNull()) {
            doc.addResource(QTextDocument::ImageResource, QUrl(rawSrc), resImg);
            doc.addResource(QTextDocument::ImageResource, QUrl(QUrl::fromPercentEncoding(rawSrc.toUtf8())), resImg);
            doc.addResource(QTextDocument::ImageResource, QUrl(key), resImg);
            doc.addResource(QTextDocument::ImageResource, QUrl(QStringLiteral("epub:///") + key), resImg);
        }
    }

    doc.setHtml(htmlText);
}

void splitTargetAndFragment(const QString &baseDir, const QString &rawHref,
                            QString &outTargetPath, QString &outFragment) {
    QString href = rawHref.trimmed();
    const int hashPos = href.indexOf('#');
    if (hashPos >= 0) {
        outFragment = QUrl::fromPercentEncoding(href.mid(hashPos + 1).toUtf8());
        href = href.left(hashPos);
    } else {
        outFragment.clear();
    }
    outTargetPath = resolvePath(baseDir, href);
}

QList<RawTocEntry> parseEpub3NavToc(const QByteArray &navData, const QString &navDir) {
    QList<RawTocEntry> entries;
    // Use QXmlStreamReader on the XHTML nav document; fall back to regex if non-well-formed HTML5
    QXmlStreamReader xml(navData);
    bool inTocNav = false;
    int navNesting = 0;
    int olDepth = 0;
    bool collectingAnchor = false;
    QString anchorHref;
    QString anchorText;
    QList<RawTocEntry> fallbackEntries;
    bool inAnyNav = false;
    int anyNavNesting = 0;

    auto handleStart = [&](const QStringView name, const QXmlStreamAttributes &attrs, bool &inNav, int &nesting, QList<RawTocEntry> &out) {
        Q_UNUSED(out);
        if (name == QLatin1String("ol") && inNav) {
            olDepth++;
        } else if (name == QLatin1String("a") && inNav && olDepth > 0) {
            collectingAnchor = true;
            anchorHref = attrs.value(QStringLiteral("href")).toString();
            anchorText.clear();
        }
        Q_UNUSED(nesting);
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == QLatin1String("nav")) {
                const auto attrs = xml.attributes();
                QString epubType;
                for (const auto &attr : attrs) {
                    if (attr.name() == QLatin1String("type") || attr.qualifiedName() == QLatin1String("epub:type")) {
                        epubType = attr.value().toString();
                        break;
                    }
                }
                const bool isToc = epubType.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)
                                       .contains(QStringLiteral("toc"), Qt::CaseInsensitive);
                if (isToc && !inTocNav) {
                    inTocNav = true;
                    navNesting = 1;
                    olDepth = 0;
                } else if (inTocNav) {
                    navNesting++;
                } else if (!inAnyNav && entries.isEmpty()) {
                    inAnyNav = true;
                    anyNavNesting = 1;
                    olDepth = 0;
                } else if (inAnyNav) {
                    anyNavNesting++;
                }
            } else if (inTocNav) {
                handleStart(name, xml.attributes(), inTocNav, navNesting, entries);
            } else if (inAnyNav) {
                handleStart(name, xml.attributes(), inAnyNav, anyNavNesting, fallbackEntries);
            }
        } else if (xml.isCharacters() && !xml.isWhitespace()) {
            if (collectingAnchor) {
                anchorText += xml.text().toString();
            }
        } else if (xml.isWhitespace() && collectingAnchor) {
            anchorText += QLatin1Char(' ');
        } else if (xml.isEndElement()) {
            const QStringView name = xml.name();
            if (name == QLatin1String("a") && collectingAnchor) {
                collectingAnchor = false;
                const QString cleanTitle = anchorText.simplified();
                if (!cleanTitle.isEmpty() && !anchorHref.isEmpty()) {
                    RawTocEntry entry;
                    entry.title = cleanTitle;
                    entry.depth = std::max(0, olDepth - 1);
                    splitTargetAndFragment(navDir, anchorHref, entry.targetPath, entry.fragment);
                    if (inTocNav) entries.append(entry);
                    else if (inAnyNav) fallbackEntries.append(entry);
                }
                anchorHref.clear();
                anchorText.clear();
            } else if (name == QLatin1String("ol") && (inTocNav || inAnyNav)) {
                olDepth = std::max(0, olDepth - 1);
            } else if (name == QLatin1String("nav")) {
                if (inTocNav) {
                    navNesting--;
                    if (navNesting <= 0) {
                        inTocNav = false;
                        break;
                    }
                } else if (inAnyNav) {
                    anyNavNesting--;
                    if (anyNavNesting <= 0) {
                        inAnyNav = false;
                    }
                }
            }
        }
    }

    if (!entries.isEmpty()) return entries;
    if (!fallbackEntries.isEmpty()) return fallbackEntries;

    // Regex fallback for non-XML-compliant HTML5 nav documents
    const QString navStr = decodeHtmlBytes(navData);
    static const QRegularExpression navBlockRe(
        QStringLiteral("<nav\\b[^>]*epub:type=[\"'][^\"']*\\btoc\\b[^\"']*[\"'][^>]*>(.*?)</nav>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto navMatch = navBlockRe.match(navStr);
    const QString scope = navMatch.hasMatch() ? navMatch.captured(1) : navStr;

    static const QRegularExpression linkRe(
        QStringLiteral("<a\\b[^>]*href=[\"']([^\"']+)[\"'][^>]*>(.*?)</a>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto it = linkRe.globalMatch(scope);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString title = stripNonVisibleText(m.captured(2));
        if (!title.isEmpty()) {
            RawTocEntry entry;
            entry.title = title;
            entry.depth = 0;
            splitTargetAndFragment(navDir, m.captured(1), entry.targetPath, entry.fragment);
            entries.append(entry);
        }
    }
    return entries;
}

QList<RawTocEntry> parseNcxToc(const QByteArray &ncxData, const QString &ncxDir) {
    QList<RawTocEntry> entries;
    QXmlStreamReader ncxXml(ncxData);

    bool inNavMap = false;
    int depth = 0;
    QString currentLabel;

    while (!ncxXml.atEnd()) {
        ncxXml.readNext();
        if (ncxXml.isStartElement()) {
            const QStringView name = ncxXml.name();
            if (name == QLatin1String("navMap")) {
                inNavMap = true;
            } else if (!inNavMap) {
                continue;
            } else if (name == QLatin1String("navPoint")) {
                depth++;
                currentLabel.clear();
            } else if (depth > 0 && name == QLatin1String("text") && currentLabel.isEmpty()) {
                currentLabel = ncxXml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
            } else if (depth > 0 && name == QLatin1String("content")) {
                const QString rawSrc = ncxXml.attributes().value(QStringLiteral("src")).toString();
                if (!currentLabel.isEmpty() && !rawSrc.isEmpty()) {
                    RawTocEntry entry;
                    entry.title = currentLabel;
                    entry.depth = std::max(0, depth - 1);
                    splitTargetAndFragment(ncxDir, rawSrc, entry.targetPath, entry.fragment);
                    entries.append(entry);
                    currentLabel.clear();
                }
            }
        } else if (ncxXml.isEndElement()) {
            const QStringView name = ncxXml.name();
            if (name == QLatin1String("navPoint")) {
                depth = std::max(0, depth - 1);
                currentLabel.clear();
            } else if (name == QLatin1String("navMap")) {
                inNavMap = false;
            }
        }
    }
    return entries;
}

int findFragmentSubPage(const QTextDocument &doc,
                        const QString &htmlText,
                        const QString &fragment,
                        int totalSubs) {
    if (fragment.isEmpty() || totalSubs <= 1) return 0;

    // 1. Check QTextDocument anchorNames preserved by QTextHtmlImporter
    for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (frag.isValid() && frag.charFormat().anchorNames().contains(fragment)) {
                const qreal top = doc.documentLayout()->blockBoundingRect(block).top();
                return std::clamp(static_cast<int>(top / kEpubLogicalHeight), 0, totalSubs - 1);
            }
        }
    }

    // 2. Fallback: find text immediately following id="fragment" or name="fragment" in HTML
    const QString escapedFrag = QRegularExpression::escape(fragment);
    const QRegularExpression idRe(
        QStringLiteral("(?:id|name)=[\"']%1[\"'][^>]*>(.{1,250})").arg(escapedFrag),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const auto match = idRe.match(htmlText);
    if (match.hasMatch()) {
        const QString snippet = stripNonVisibleText(match.captured(1)).left(60).trimmed();
        if (snippet.length() >= 3) {
            const QTextCursor found = doc.find(snippet);
            if (!found.isNull()) {
                const qreal top = doc.documentLayout()->blockBoundingRect(found.block()).top();
                return std::clamp(static_cast<int>(top / kEpubLogicalHeight), 0, totalSubs - 1);
            }
        }
    }
    return 0;
}

} // anonymous namespace

EpubBackend::EpubBackend() = default;

EpubBackend::~EpubBackend() {
    close();
}

void EpubBackend::clearUnlocked() {
    m_files.clear();
    m_pages.clear();
    m_toc.clear();
    m_filePath.clear();
    m_title.clear();
    m_author.clear();
}

bool EpubBackend::load(const QString &filePath) {
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
    QHash<QString, QByteArray> files;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *rawPath = archive_entry_pathname(entry);
        if (!rawPath) {
            archive_read_data_skip(a);
            continue;
        }

        const QString rawStr = QString::fromUtf8(rawPath);
        if (archive_entry_filetype(entry) == AE_IFDIR ||
            rawStr.endsWith('/') || rawStr.contains(QStringLiteral("__MACOSX"))) {
            archive_read_data_skip(a);
            continue;
        }

        const QString name = normalizePath(rawStr);
        if (name.isEmpty()) {
            archive_read_data_skip(a);
            continue;
        }

        bool ok = false;
        const QByteArray data = OmadocUtils::readArchiveEntryData(a, entry, 128LL * 1024 * 1024, &ok);
        if (ok) {
            files.insert(name, data);
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    if (files.isEmpty()) {
        return false;
    }

    // 1. Locate rootfile from META-INF/container.xml
    QString opfPath;
    const QString containerKey = lookupArchiveKey(files, QStringLiteral("META-INF/container.xml"));
    const QByteArray containerXml = containerKey.isEmpty() ? QByteArray() : files.value(containerKey);
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
        for (auto it = files.keyBegin(); it != files.keyEnd(); ++it) {
            if (it->endsWith(QStringLiteral(".opf"), Qt::CaseInsensitive)) {
                opfPath = *it;
                break;
            }
        }
    }

    opfPath = lookupArchiveKey(files, resolvePath(QString(), opfPath));
    if (opfPath.isEmpty()) {
        return false;
    }

    const QString opfDir = normalizeDir(opfPath);

    // 2. Parse OPF package file
    const QByteArray opfData = files.value(opfPath);
    QXmlStreamReader xml(opfData);

    QString title;
    QStringList authors;
    QHash<QString, QString> manifestHref; // id -> resolved archive key/path
    QHash<QString, QString> manifestType;
    QList<QString> spineIdrefs;
    QString ncxId;
    QString navPath;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == QLatin1String("title") && title.isEmpty()) {
                title = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
            } else if (name == QLatin1String("creator")) {
                const QString creator = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                if (!creator.isEmpty() && !authors.contains(creator)) {
                    authors.append(creator);
                }
            } else if (name == QLatin1String("item")) {
                const auto attrs = xml.attributes();
                const QString id = attrs.value(QStringLiteral("id")).toString();
                const QString href = attrs.value(QStringLiteral("href")).toString();
                const QString mediaType = attrs.value(QStringLiteral("media-type")).toString();
                const QString props = attrs.value(QStringLiteral("properties")).toString();
                if (!id.isEmpty() && !href.isEmpty()) {
                    const QString resolved = resolvePath(opfDir, href);
                    const QString key = lookupArchiveKey(files, resolved);
                    const QString finalPath = key.isEmpty() ? resolved : key;
                    manifestHref.insert(id, finalPath);
                    manifestType.insert(id, mediaType);
                    if (navPath.isEmpty() &&
                        props.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)
                             .contains(QStringLiteral("nav"), Qt::CaseInsensitive)) {
                        navPath = finalPath;
                    }
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
        for (auto it = manifestType.constBegin(); it != manifestType.constEnd(); ++it) {
            const QString mt = it.value().toLower();
            if (mt == QLatin1String("application/xhtml+xml") ||
                mt == QLatin1String("text/html") ||
                mt.startsWith(QLatin1String("image/"))) {
                spineIdrefs.append(it.key());
            }
        }
    }

    // 3. Parse Table of Contents early (prefer EPUB 3 <nav>, fallback to NCX)
    QList<RawTocEntry> rawToc;
    if (!navPath.isEmpty() && files.contains(navPath)) {
        rawToc = parseEpub3NavToc(files.value(navPath), normalizeDir(navPath));
    }

    if (rawToc.isEmpty()) {
        QString ncxPath;
        if (!ncxId.isEmpty()) {
            ncxPath = lookupArchiveKey(files, manifestHref.value(ncxId));
        }
        if (ncxPath.isEmpty()) {
            for (auto it = files.keyBegin(); it != files.keyEnd(); ++it) {
                if (it->endsWith(QStringLiteral(".ncx"), Qt::CaseInsensitive)) {
                    ncxPath = *it;
                    break;
                }
            }
        }
        if (!ncxPath.isEmpty() && files.contains(ncxPath)) {
            rawToc = parseNcxToc(files.value(ncxPath), normalizeDir(ncxPath));
        }
    }

    // Normalize TOC target paths to archive keys and collect requested fragments per chapter
    QHash<QString, QSet<QString>> requestedFragments;
    for (auto &entryItem : rawToc) {
        const QString key = lookupArchiveKey(files, entryItem.targetPath);
        if (!key.isEmpty()) {
            entryItem.targetPath = key;
        }
        if (!entryItem.targetPath.isEmpty() && !entryItem.fragment.isEmpty()) {
            requestedFragments[entryItem.targetPath].insert(entryItem.fragment);
        }
    }

    // 4. Build pages from spine with shared image cache during load
    QList<EpubPageInfo> pages;
    QHash<QString, int> fileStartPage;
    QHash<QString, QHash<QString, int>> fileFragmentPage;
    QHash<QString, QImage> loadImageCache;

    for (int spineIdx = 0; spineIdx < spineIdrefs.size(); ++spineIdx) {
        const QString id = spineIdrefs.at(spineIdx);
        const QString resolvedPath = lookupArchiveKey(files, manifestHref.value(id));
        if (resolvedPath.isEmpty() || !files.contains(resolvedPath)) continue;

        const QString mediaType = manifestType.value(id).toLower();
        const QByteArray content = files.value(resolvedPath);

        if (!fileStartPage.contains(resolvedPath)) {
            fileStartPage.insert(resolvedPath, pages.size());
        }

        // Direct image spine item
        if (mediaType.startsWith(QStringLiteral("image/")) || isSupportedImageExtension(resolvedPath)) {
            QByteArray imgBytes = content;
            QBuffer buf(&imgBytes);
            buf.open(QIODevice::ReadOnly);
            QImageReader rdr(&buf);
            const QSize sz = rdr.size();
            EpubPageInfo page;
            page.spineIndex = spineIdx;
            page.subPage = 0;
            page.totalSubPages = 1;
            page.href = resolvedPath;
            page.imageHref = resolvedPath;
            page.size = (sz.isValid() && sz.width() > 0 && sz.height() > 0)
                ? QSizeF(sz.width() * (72.0 / 140.0), sz.height() * (72.0 / 140.0))
                : QSizeF(kEpubLogicalWidth, kEpubLogicalHeight);
            pages.append(page);
            continue;
        }

        const QString chapterDir = normalizeDir(resolvedPath);
        const QString contentStr = decodeHtmlBytes(content);

        // Check if this chapter is a manga scan or single-image fixed page
        static const QRegularExpression anyImgRe(
            QStringLiteral("<(?:img|image)\\b[^>]*?(?:src|xlink:href|href)=[\"']([^\"']+)[\"']"),
            QRegularExpression::CaseInsensitiveOption);

        QList<QString> foundImgSrcs;
        auto imgIt = anyImgRe.globalMatch(contentStr);
        while (imgIt.hasNext()) {
            foundImgSrcs.append(imgIt.next().captured(1));
        }

        const QString visibleText = stripNonVisibleText(contentStr);

        bool isMangaScan = false;
        if (foundImgSrcs.size() == 1 && visibleText.length() < 30) {
            const QString resolvedImg = lookupArchiveKey(files, resolvePath(chapterDir, foundImgSrcs.first()));
            if (!resolvedImg.isEmpty() && files.contains(resolvedImg)) {
                isMangaScan = true;
                EpubPageInfo page;
                page.spineIndex = spineIdx;
                page.subPage = 0;
                page.totalSubPages = 1;
                page.href = resolvedPath;
                page.imageHref = resolvedImg;
                page.plainText = visibleText;

                QByteArray imgBytes = files.value(resolvedImg);
                QBuffer buf(&imgBytes);
                buf.open(QIODevice::ReadOnly);
                QImageReader rdr(&buf);
                const QSize sz = rdr.size();
                if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                    page.size = QSizeF(sz.width() * (72.0 / 140.0), sz.height() * (72.0 / 140.0));
                } else {
                    page.size = QSizeF(kEpubLogicalWidth, kEpubLogicalHeight);
                }
                pages.append(page);
            }
        }

        if (!isMangaScan) {
            // Text / illustrated chapter: paginate using the EXACT logical geometry/style used by renderPage
            QTextDocument doc;
            setupChapterDocument(doc, resolvedPath, contentStr, files, false, &loadImageCache);

            const int totalSubs = std::max(1, doc.pageCount());

            // Partition plain text per sub-page using block vertical positions for accurate search
            QStringList subPlainTexts;
            subPlainTexts.reserve(totalSubs);
            for (int s = 0; s < totalSubs; ++s) {
                subPlainTexts.append(QString());
            }

            for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
                const QString blockText = block.text().trimmed();
                if (blockText.isEmpty()) continue;
                const qreal top = doc.documentLayout()->blockBoundingRect(block).top();
                const int subIdx = std::clamp(static_cast<int>(top / kEpubLogicalHeight), 0, totalSubs - 1);
                if (!subPlainTexts[subIdx].isEmpty()) {
                    subPlainTexts[subIdx] += QLatin1Char('\n');
                }
                subPlainTexts[subIdx] += blockText;
            }

            // Resolve any TOC fragment anchors inside this chapter
            if (requestedFragments.contains(resolvedPath)) {
                const int basePageIdx = pages.size();
                for (const QString &frag : requestedFragments.value(resolvedPath)) {
                    const int sub = findFragmentSubPage(doc, contentStr, frag, totalSubs);
                    fileFragmentPage[resolvedPath].insert(frag, basePageIdx + sub);
                }
            }

            for (int sub = 0; sub < totalSubs; ++sub) {
                EpubPageInfo page;
                page.spineIndex = spineIdx;
                page.subPage = sub;
                page.totalSubPages = totalSubs;
                page.href = resolvedPath;
                page.htmlText = contentStr;
                page.plainText = subPlainTexts.at(sub);
                page.size = QSizeF(kEpubLogicalWidth, kEpubLogicalHeight);
                pages.append(page);
            }
        }
    }

    if (pages.isEmpty()) {
        return false;
    }

    // 5. Resolve TOC page indices
    QVariantList toc;
    for (const RawTocEntry &entryItem : rawToc) {
        int resolvedPage = 0;
        if (!entryItem.fragment.isEmpty() &&
            fileFragmentPage.contains(entryItem.targetPath) &&
            fileFragmentPage.value(entryItem.targetPath).contains(entryItem.fragment)) {
            resolvedPage = fileFragmentPage.value(entryItem.targetPath).value(entryItem.fragment);
        } else if (fileStartPage.contains(entryItem.targetPath)) {
            resolvedPage = fileStartPage.value(entryItem.targetPath);
        }
        QVariantMap item;
        item[QStringLiteral("title")] = entryItem.title;
        item[QStringLiteral("depth")] = entryItem.depth;
        item[QStringLiteral("page")] = resolvedPage;
        toc.append(item);
    }

    m_filePath = filePath;
    m_title = title.isEmpty() ? QFileInfo(filePath).completeBaseName() : title;
    m_author = authors.join(QStringLiteral(", "));
    m_files = std::move(files);
    m_pages = std::move(pages);
    m_toc = std::move(toc);
    return true;
}

void EpubBackend::close() {
    QMutexLocker locker(&m_mutex);
    clearUnlocked();
}

bool EpubBackend::saveCopy(const QString &targetPath) {
    QMutexLocker locker(&m_mutex);
    return OmadocUtils::safeCopyFile(m_filePath, targetPath);
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
    const double safeDpi = (dpi > 10.0) ? std::clamp(dpi, 10.0, 2000.0) : 150.0;
    const int normRot = (rotation % 360 + 360) % 360;

    // Case 1: Manga / Graphic scan page
    if (!info.imageHref.isEmpty() && m_files.contains(info.imageHref)) {
        QImage img;
        img.loadFromData(m_files.value(info.imageHref));
        if (img.isNull()) return QImage();

        if (info.size.width() > 0) {
            const int targetW = static_cast<int>(std::round(info.size.width() * (safeDpi / 72.0)));
            const int targetH = static_cast<int>(std::round(info.size.height() * (safeDpi / 72.0)));
            if (std::abs(img.width() - targetW) > 10 && targetW > 0 && targetH > 0) {
                img = img.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }

        if (normRot != 0) {
            QTransform t;
            t.rotate(normRot);
            img = img.transformed(t);
        }

        if (darkMode) {
            OmadocUtils::invertImageRgbSafely(img);
        }

        return img;
    }

    // Case 2: Rich Text Page — lay out at logical 72-DPI A4 and scale QPainter uniformly
    const int targetW = static_cast<int>(std::round(info.size.width() * (safeDpi / 72.0)));
    const int targetH = static_cast<int>(std::round(info.size.height() * (safeDpi / 72.0)));
    const int w = std::max(100, targetW);
    const int h = std::max(100, targetH);

    QTextDocument doc;
    setupChapterDocument(doc, info.href, info.htmlText, m_files, darkMode, nullptr);

    QImage result(w, h, QImage::Format_ARGB32);
    result.fill(darkMode ? QColor(QStringLiteral("#12141a")) : Qt::white);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal scaleX = static_cast<qreal>(w) / kEpubLogicalWidth;
    const qreal scaleY = static_cast<qreal>(h) / kEpubLogicalHeight;
    painter.scale(scaleX, scaleY);

    const qreal pageOffsetY = info.subPage * kEpubLogicalHeight;
    painter.translate(0.0, -pageOffsetY);
    const QRectF pageRect(0.0, pageOffsetY, kEpubLogicalWidth, kEpubLogicalHeight);
    painter.setClipRect(pageRect);
    doc.drawContents(&painter, pageRect);
    painter.end();

    if (normRot != 0) {
        QTransform t;
        t.rotate(normRot);
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
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) return results;

    for (int i = 0; i < m_pages.size(); ++i) {
        const auto &info = m_pages.at(i);
        if (!info.plainText.isEmpty() && info.plainText.contains(trimmed, Qt::CaseInsensitive)) {
            results.append(i);
        }
    }
    return results;
}

QList<QRectF> EpubBackend::searchHighlights(int index, const QString &query) const {
    QMutexLocker locker(&m_mutex);
    QList<QRectF> highlights;
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty() || index < 0 || index >= m_pages.size())
        return highlights;

    const EpubPageInfo &info = m_pages.at(index);
    if (info.htmlText.isEmpty())
        return highlights;

    QTextDocument doc;
    setupChapterDocument(doc, info.href, info.htmlText, m_files, false, nullptr);

    const qreal pageTop = info.subPage * kEpubLogicalHeight;
    const qreal pageBottom = pageTop + kEpubLogicalHeight;

    QTextCursor cursor = doc.find(trimmed, 0);
    constexpr int kMaxMatches = 500;
    for (int count = 0; count < kMaxMatches && !cursor.isNull(); ++count) {
        const int selStart = cursor.selectionStart();
        const int selEnd = cursor.selectionEnd();
        if (selEnd <= selStart) break;

        QTextBlock block = doc.findBlock(selStart);
        while (block.isValid() && block.position() < selEnd) {
            const int blockPos = block.position();
            const int localStart = std::max(0, selStart - blockPos);
            const int localEnd = std::min(block.length() - 1, selEnd - blockPos);
            const QRectF blockRect = doc.documentLayout()->blockBoundingRect(block);
            const QTextLayout *layout = block.layout();

            if (layout && localEnd > localStart) {
                for (int li = 0; li < layout->lineCount(); ++li) {
                    const QTextLine line = layout->lineAt(li);
                    const int lineStart = line.textStart();
                    const int lineEnd = lineStart + line.textLength();
                    const int segStart = std::max(localStart, lineStart);
                    const int segEnd = std::min(localEnd, lineEnd);
                    if (segEnd <= segStart) continue;

                    const qreal x1 = line.cursorToX(segStart);
                    const qreal x2 = line.cursorToX(segEnd);
                    const qreal left = blockRect.left() + std::min(x1, x2);
                    const qreal right = blockRect.left() + std::max(x1, x2);
                    const qreal top = blockRect.top() + line.y();
                    const qreal height = line.height();

                    if (top + height > pageTop && top < pageBottom && right > left) {
                        const double nx = std::clamp(left / kEpubLogicalWidth, 0.0, 1.0);
                        const double ny = std::clamp((top - pageTop) / kEpubLogicalHeight, 0.0, 1.0);
                        const double nw = std::clamp((right - left) / kEpubLogicalWidth, 0.0, 1.0 - nx);
                        const double nh = std::clamp(height / kEpubLogicalHeight, 0.0, 1.0 - ny);
                        if (nw > 0.0 && nh > 0.0) {
                            highlights.append(QRectF(nx, ny, nw, nh));
                        }
                    }
                }
            }
            block = block.next();
        }

        cursor = doc.find(trimmed, selEnd);
    }

    return highlights;
}
