#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QPdfWriter>
#include <QPainter>
#include <QBuffer>
#include <archive.h>
#include <archive_entry.h>

#include "backend.h"
#include "documentengine.h"
#include "pageimageprovider.h"
#include "theme.h"
#include "translator.h"

namespace {

struct TestEntry {
    QString name;
    QByteArray data;
};

void createZipArchive(const QString &zipPath, const QList<TestEntry> &entries) {
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, zipPath.toUtf8().constData());
    for (const auto &item : entries) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, item.name.toUtf8().constData());
        archive_entry_set_size(entry, item.data.size());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, item.data.constData(), item.data.size());
        archive_entry_free(entry);
    }
    archive_write_close(a);
    archive_write_free(a);
}

QByteArray createTestPng(const QString &text, int width = 400, int height = 600) {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setPen(Qt::black);
    p.setFont(QFont(QStringLiteral("sans-serif"), 14));
    p.drawText(img.rect(), Qt::AlignCenter, text);
    p.end();

    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}

} // anonymous namespace

class OmadocTests : public QObject {
    Q_OBJECT

private slots:
    void testInitialBackendState() {
        DocumentEngine engine;
        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);

        QCOMPARE(backend.hasDocument(), false);
        QCOMPARE(backend.currentPage(), 0);
        QCOMPARE(backend.zoom(), 1.0);
        QCOMPARE(backend.darkMode(), false);
        QCOMPARE(backend.rotation(), 0);
        QCOMPARE(backend.pageCount(), 0);
    }

    void testZoomClamping() {
        DocumentEngine engine;
        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);

        backend.setFitMode(1);
        QCOMPARE(backend.fitMode(), 1);

        backend.setZoom(0.05);
        QCOMPARE(backend.zoom(), 0.2);
        QCOMPARE(backend.fitMode(), 0);

        backend.setZoom(10.0);
        QCOMPARE(backend.zoom(), 5.0);

        backend.resetZoom();
        QCOMPARE(backend.zoom(), 1.0);
    }

    void testDarkModeAndRotationToggle() {
        DocumentEngine engine;
        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);

        const int initialRev = backend.revision();
        backend.toggleDarkMode();
        QCOMPARE(backend.darkMode(), true);
        QVERIFY(backend.revision() > initialRev);

        backend.rotateClockwise();
        QCOMPARE(backend.rotation(), 90);

        backend.rotateClockwise();
        QCOMPARE(backend.rotation(), 180);

        backend.rotateClockwise();
        QCOMPARE(backend.rotation(), 270);

        backend.rotateClockwise();
        QCOMPARE(backend.rotation(), 0);

        backend.rotateCounterClockwise();
        QCOMPARE(backend.rotation(), 270);

        backend.rotateCounterClockwise();
        QCOMPARE(backend.rotation(), 180);

        backend.rotateCounterClockwise();
        QCOMPARE(backend.rotation(), 90);

        backend.rotateCounterClockwise();
        QCOMPARE(backend.rotation(), 0);
    }

    void testThemeFallback() {
        QTemporaryDir temp;
        Theme theme(temp.path());
        QCOMPARE(theme.accent(), QStringLiteral("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("black"));
        QCOMPARE(theme.radius(), 0);
    }

    void testThemeCustomAccent() {
        QTemporaryDir temp;
        QDir(temp.path()).mkpath("theme");
        QFile colors(temp.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly | QIODevice::Text));
        colors.write("accent = \"#336699\"\n");
        colors.close();

        Theme theme(temp.path());
        QCOMPARE(theme.accent(), QStringLiteral("#336699"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("white"));
    }

    void testTranslatorFallback() {
        QTemporaryDir temp;
        Translator translator(temp.path() + "/language", temp.path());
        QCOMPARE(translator.language(), QStringLiteral("en"));
    }

    void testDocumentEngineWithGeneratedPdf() {
        QTemporaryDir temp;
        const QString pdfPath = temp.filePath("sample.pdf");

        // Generate a 2-page test PDF
        {
            QPdfWriter writer(pdfPath);
            writer.setPageSize(QPageSize(QPageSize::A4));
            QPainter painter(&writer);
            painter.drawText(100, 100, "Omadoc First Page Content");
            writer.newPage();
            painter.drawText(100, 100, "Omadoc Second Page Content");
            painter.end();
        }

        DocumentEngine engine;
        QVERIFY(engine.load(pdfPath));
        QCOMPARE(engine.pageCount(), 2);

        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);

        backend.openFile(pdfPath);
        QCOMPARE(backend.hasDocument(), true);
        QCOMPARE(backend.pageCount(), 2);
        QCOMPARE(backend.currentPage(), 0);

        backend.nextPage();
        QCOMPARE(backend.currentPage(), 1);

        backend.nextPage(); // Should clamp at page 1
        QCOMPARE(backend.currentPage(), 1);

        backend.prevPage();
        QCOMPARE(backend.currentPage(), 0);

        // Test page rendering normal and rotated
        const QImage page0 = engine.renderPage(0, 72.0, false, 0);
        QVERIFY(!page0.isNull());
        QVERIFY(page0.width() > 0 && page0.height() > 0);

        const QImage page0Rot = engine.renderPage(0, 72.0, false, 90);
        QVERIFY(!page0Rot.isNull());
        QCOMPARE(page0Rot.width(), page0.height());
        QCOMPARE(page0Rot.height(), page0.width());

        // Test provider rotation fast-path
        QSize s0, sRot;
        const QImage prov0 = provider.requestImage("0_72_0_0_0", &s0, QSize());
        QVERIFY(!prov0.isNull());
        const QImage provRot = provider.requestImage("0_72_0_90_0", &sRot, QSize());
        QVERIFY(!provRot.isNull());
        QCOMPARE(provRot.width(), prov0.height());
        QCOMPARE(provRot.height(), prov0.width());

        // Test night mode inversion
        const QImage page0Night = engine.renderPage(0, 72.0, true, 0);
        QVERIFY(!page0Night.isNull());

        // Test text search & search highlight bounding boxes
        const auto searchRes = engine.search("First");
        QVERIFY(!searchRes.isEmpty());
        QCOMPARE(searchRes.first(), 0);

        backend.search("First");
        const QVariantList pdfHl0 = backend.searchHighlights(0);
        QVERIFY(!pdfHl0.isEmpty());
        const QVariantMap firstBox = pdfHl0.first().toMap();
        QVERIFY(firstBox.value("width").toDouble() > 0.0);
        QVERIFY(firstBox.value("height").toDouble() > 0.0);
        backend.setRotation(90);
        const QVariantList pdfHl90 = backend.searchHighlights(0);
        QCOMPARE(pdfHl90.size(), pdfHl0.size());
        backend.setRotation(0);
        backend.clearSearch();
        QVERIFY(backend.searchHighlights(0).isEmpty());

        // Test save copy (Save As)
        const QString copyPath = temp.filePath("saved_copy.pdf");
        QVERIFY(engine.saveCopy(copyPath));
        QVERIFY(QFile::exists(copyPath));
        QVERIFY(QFileInfo(copyPath).size() > 0);

        // Test in-place save (Save / Ctrl+S) without corruption
        const qint64 origSize = QFileInfo(pdfPath).size();
        QVERIFY(origSize > 0);
        QVERIFY(engine.saveCopy(pdfPath));
        QVERIFY(QFile::exists(pdfPath));
        QVERIFY(QFileInfo(pdfPath).size() > 0);
        QCOMPARE(engine.pageCount(), 2);

        // Verify reloaded document from in-place saved file
        DocumentEngine reloadedEngine;
        QVERIFY(reloadedEngine.load(pdfPath));
        QCOMPARE(reloadedEngine.pageCount(), 2);

        // Test annotations (Highlighting & Sticky Notes)
        QCOMPARE(backend.annotationCount(), 0);
        backend.setActiveTool(1);
        QCOMPARE(backend.activeTool(), 1);

        backend.addHighlight(0, 0.1, 0.2, 0.3, 0.05, "#ffe082");
        QCOMPARE(backend.annotationCount(), 1);
        QCOMPARE(backend.pageAnnotations(0).size(), 1);
        QCOMPARE(backend.pageAnnotations(1).size(), 0);

        const QVariantMap hlMap = backend.pageAnnotations(0).first().toMap();
        QCOMPARE(hlMap.value("type").toString(), QStringLiteral("highlight"));
        QCOMPARE(hlMap.value("color").toString(), QStringLiteral("#ffe082"));
        QVERIFY(std::abs(hlMap.value("x").toDouble() - 0.1) < 0.001);
        QVERIFY(std::abs(hlMap.value("y").toDouble() - 0.2) < 0.001);

        // Add second highlight
        backend.addHighlight(0, 0.5, 0.6, 0.2, 0.05, "#80deea");
        QCOMPARE(backend.annotationCount(), 2);
        QCOMPARE(backend.pageAnnotations(0).size(), 2);

        // Test rotation transformation
        backend.setRotation(90);
        const QVariantList rotAnnots = backend.pageAnnotations(0);
        QCOMPARE(rotAnnots.size(), 2);
        backend.setRotation(0);

        // Delete second highlight
        const QString secondId = backend.pageAnnotations(0).at(1).toMap().value("id").toString();
        backend.deleteAnnotation(secondId);
        QCOMPARE(backend.annotationCount(), 1);

        // Test persistence: re-open file should reload highlight
        backend.openFile(pdfPath);
        QCOMPARE(backend.annotationCount(), 1);

        // Clear all annotations
        backend.clearAllAnnotations();
        QCOMPARE(backend.annotationCount(), 0);

        backend.closeDocument();
        QCOMPARE(backend.hasDocument(), false);
    }

    void testComicArchiveEngine() {
        QTemporaryDir temp;
        const QString cbzPath = temp.filePath("sample.cbz");

        QList<TestEntry> entries;
        entries.append(TestEntry{QStringLiteral("page_01.png"), createTestPng("Chapter 1: The Beginning")});
        entries.append(TestEntry{QStringLiteral("page_02.png"), createTestPng("Chapter 1: Page 2")});
        entries.append(TestEntry{QStringLiteral("page_10.png"), createTestPng("Chapter 1: Page 10")});
        entries.append(TestEntry{QStringLiteral("ComicInfo.xml"), QByteArray("<ComicInfo><Title>Sample Manga</Title><Writer>Manga Artist</Writer></ComicInfo>")});

        createZipArchive(cbzPath, entries);
        QVERIFY(QFile::exists(cbzPath));

        DocumentEngine engine;
        QVERIFY(engine.load(cbzPath));
        QCOMPARE(engine.isLoaded(), true);
        QCOMPARE(engine.pageCount(), 3);
        QCOMPARE(engine.title(), QStringLiteral("Sample Manga"));
        QCOMPARE(engine.author(), QStringLiteral("Manga Artist"));

        // Test page dimensions
        QSizeF sz0 = engine.pageSize(0);
        QVERIFY(sz0.width() > 0 && sz0.height() > 0);

        // Test rendering
        QImage p0 = engine.renderPage(0, 140.0, false, 0);
        QVERIFY(!p0.isNull());
        QCOMPARE(p0.width(), 400);
        QCOMPARE(p0.height(), 600);

        // Test rotation
        QImage p0Rot = engine.renderPage(0, 140.0, false, 90);
        QVERIFY(!p0Rot.isNull());
        QCOMPARE(p0Rot.width(), 600);
        QCOMPARE(p0Rot.height(), 400);

        // Test dark mode
        QImage p0Dark = engine.renderPage(0, 140.0, true, 0);
        QVERIFY(!p0Dark.isNull());

        // Test search
        auto searchRes = engine.search("page_02");
        QCOMPARE(searchRes.size(), 1);
        QCOMPARE(searchRes.first(), 1);

        // Test integration with Backend
        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);
        backend.openFile(cbzPath);
        QCOMPARE(backend.hasDocument(), true);
        QCOMPARE(backend.pageCount(), 3);

        // Test annotations on CBZ
        backend.addHighlight(0, 0.1, 0.1, 0.2, 0.2, "#80deea");
        QCOMPARE(backend.annotationCount(), 1);
        backend.clearAllAnnotations();

        backend.closeDocument();
        QCOMPARE(backend.hasDocument(), false);
    }

    void testEpubEngine() {
        QTemporaryDir temp;
        const QString epubPath = temp.filePath("sample.epub");

        QList<TestEntry> entries;
        entries.append(TestEntry{QStringLiteral("mimetype"), QByteArray("application/epub+zip")});
        entries.append(TestEntry{QStringLiteral("META-INF/container.xml"),
            QByteArray("<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
            "  <rootfiles>"
            "    <rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/>"
            "  </rootfiles>"
            "</container>")
        });
        entries.append(TestEntry{QStringLiteral("OEBPS/content.opf"),
            QByteArray("<package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
            "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "    <dc:title>Epub Adventure</dc:title>"
            "    <dc:creator>Jane Novelist</dc:creator>"
            "  </metadata>"
            "  <manifest>"
            "    <item id=\"ncx\" href=\"toc.ncx\" media-type=\"application/x-dtbncx+xml\"/>"
            "    <item id=\"ch1\" href=\"ch1.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "    <item id=\"ch2\" href=\"ch2.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "  </manifest>"
            "  <spine toc=\"ncx\">"
            "    <itemref idref=\"ch1\"/>"
            "    <itemref idref=\"ch2\"/>"
            "  </spine>"
            "</package>")
        });
        entries.append(TestEntry{QStringLiteral("OEBPS/toc.ncx"),
            QByteArray("<ncx version=\"2005-1\" xmlns=\"http://www.daisy.org/z3986/2005/ncx/\">"
            "  <navMap>"
            "    <navPoint id=\"np1\" playOrder=\"1\">"
            "      <navLabel><text>Chapter 1: The Departure</text></navLabel>"
            "      <content src=\"ch1.xhtml\"/>"
            "    </navPoint>"
            "    <navPoint id=\"np2\" playOrder=\"2\">"
            "      <navLabel><text>Chapter 2: The Arrival</text></navLabel>"
            "      <content src=\"ch2.xhtml\"/>"
            "    </navPoint>"
            "  </navMap>"
            "</ncx>")
        });
        entries.append(TestEntry{QStringLiteral("OEBPS/ch1.xhtml"),
            QByteArray("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
            "<head><title>Chapter 1</title></head>"
            "<body><h1>Chapter 1</h1><p>It was a dark and stormy night when the adventure began.</p></body>"
            "</html>")
        });
        entries.append(TestEntry{QStringLiteral("OEBPS/ch2.xhtml"),
            QByteArray("<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
            "<head><title>Chapter 2</title></head>"
            "<body><h1>Chapter 2</h1><p>The dawn brought new hope and clear skies across the ocean.</p></body>"
            "</html>")
        });

        createZipArchive(epubPath, entries);
        QVERIFY(QFile::exists(epubPath));

        DocumentEngine engine;
        QVERIFY(engine.load(epubPath));
        QCOMPARE(engine.isLoaded(), true);
        QVERIFY(engine.pageCount() >= 2);
        QCOMPARE(engine.title(), QStringLiteral("Epub Adventure"));
        QCOMPARE(engine.author(), QStringLiteral("Jane Novelist"));

        // Test table of contents
        QVariantList toc = engine.tableOfContents();
        QCOMPARE(toc.size(), 2);
        QCOMPARE(toc.at(0).toMap().value("title").toString(), QStringLiteral("Chapter 1: The Departure"));
        QCOMPARE(toc.at(1).toMap().value("title").toString(), QStringLiteral("Chapter 2: The Arrival"));

        // Test rendering
        QImage p0 = engine.renderPage(0, 140.0, false, 0);
        QVERIFY(!p0.isNull());
        QVERIFY(p0.width() > 0 && p0.height() > 0);

        // Test dark mode rendering
        QImage p0Dark = engine.renderPage(0, 140.0, true, 0);
        QVERIFY(!p0Dark.isNull());

        // Test search (plain text matches, HTML tags do not)
        auto searchRes = engine.search("stormy");
        QVERIFY(!searchRes.isEmpty());
        QCOMPARE(searchRes.first(), 0);
        QVERIFY(engine.search("xhtml").isEmpty());

        // Test integration with Backend
        PageImageProvider provider(&engine);
        Backend backend(&engine, &provider);
        backend.openFile(epubPath);
        QCOMPARE(backend.hasDocument(), true);
        QCOMPARE(backend.title(), QStringLiteral("Epub Adventure"));

        // Test EPUB search highlight bounding boxes
        backend.search("stormy");
        const QVariantList epubHl = backend.searchHighlights(0);
        QCOMPARE(epubHl.size(), 1);
        QVERIFY(epubHl.first().toMap().value("width").toDouble() > 0.0);
        QVERIFY(epubHl.first().toMap().value("height").toDouble() > 0.0);
        backend.clearSearch();

        // Test highlight persistence on EPUB
        backend.addHighlight(0, 0.1, 0.1, 0.3, 0.05, "#80deea");
        QCOMPARE(backend.annotationCount(), 1);

        // Re-open to verify highlight persisted
        backend.openFile(epubPath);
        QCOMPARE(backend.annotationCount(), 1);
        QCOMPARE(backend.pageAnnotations(0).first().toMap().value("color").toString(), QStringLiteral("#80deea"));

        // Opening a non-existent file must emit errorOccurred and keep current document loaded
        QSignalSpy errSpy(&backend, &Backend::errorOccurred);
        backend.openFile(temp.filePath("does_not_exist.epub"));
        QCOMPARE(errSpy.count(), 1);
        QCOMPARE(backend.hasDocument(), true);

        backend.clearAllAnnotations();

        backend.closeDocument();
        QCOMPARE(backend.hasDocument(), false);
    }

    void testEpub3NavPercentPathsAndMultiPageSearch() {
        QTemporaryDir temp;
        const QString epubPath = temp.filePath("epub3_nav.epub");

        QString longBody;
        for (int i = 0; i < 60; ++i) {
            longBody += QStringLiteral("<p>Paragraph %1 of filler text to force pagination across multiple subpages in chapter one.</p>").arg(i);
        }
        longBody += QStringLiteral("<h2 id=\"part2\">Deep Anchor Target</h2><p>UniqueNeedleToken appears only at the end.</p>");

        QList<TestEntry> entries;
        entries.append(TestEntry{QStringLiteral("mimetype"), QByteArray("application/epub+zip")});
        entries.append(TestEntry{QStringLiteral("META-INF/container.xml"),
            QByteArray("<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
            "  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>"
            "</container>")});
        entries.append(TestEntry{QStringLiteral("OEBPS/content.opf"),
            QByteArray("<package version=\"3.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
            "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "    <dc:title>EPUB3 Sample</dc:title>"
            "    <dc:creator>Author One</dc:creator>"
            "    <dc:creator>Author Two</dc:creator>"
            "  </metadata>"
            "  <manifest>"
            "    <item id=\"nav\" href=\"nav.xhtml\" properties=\"nav\" media-type=\"application/xhtml+xml\"/>"
            "    <item id=\"ch1\" href=\"Text/Chapter%201.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "    <item id=\"fig\" href=\"Images/fig%201.png\" media-type=\"image/png\"/>"
            "  </manifest>"
            "  <spine><itemref idref=\"ch1\"/></spine>"
            "</package>")});
        entries.append(TestEntry{QStringLiteral("OEBPS/nav.xhtml"),
            QByteArray("<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">"
            "<body><nav epub:type=\"toc\"><ol>"
            "  <li><a href=\"Text/Chapter%201.xhtml\">Start Here</a>"
            "    <ol><li><a href=\"Text/Chapter%201.xhtml#part2\">Part Two Anchor</a></li></ol>"
            "  </li>"
            "</ol></nav></body></html>")});
        entries.append(TestEntry{QStringLiteral("OEBPS/Images/fig 1.png"), createTestPng("Figure 1", 200, 120)});
        entries.append(TestEntry{QStringLiteral("OEBPS/Text/Chapter 1.xhtml"),
            QStringLiteral("<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
            "<h1>Chapter 1</h1><img src=\"../Images/fig%201.png\"/>%1</body></html>").arg(longBody).toUtf8()});

        createZipArchive(epubPath, entries);

        DocumentEngine engine;
        QVERIFY(engine.load(epubPath));
        QCOMPARE(engine.author(), QStringLiteral("Author One, Author Two"));
        QVERIFY(engine.pageCount() >= 2);

        const QVariantList toc = engine.tableOfContents();
        QCOMPARE(toc.size(), 2);
        QCOMPARE(toc.at(0).toMap().value("title").toString(), QStringLiteral("Start Here"));
        QCOMPARE(toc.at(0).toMap().value("page").toInt(), 0);
        QCOMPARE(toc.at(1).toMap().value("title").toString(), QStringLiteral("Part Two Anchor"));
        QCOMPARE(toc.at(1).toMap().value("depth").toInt(), 1);
        QVERIFY(toc.at(1).toMap().value("page").toInt() > 0);

        // Search only matches the sub-page containing UniqueNeedleToken, not page 0
        const QList<int> hits = engine.search("UniqueNeedleToken");
        QCOMPARE(hits.size(), 1);
        QVERIFY(hits.first() > 0);

        // Last sub-page at 140 DPI must have non-background text pixels (not blank)
        const QImage lastImg = engine.renderPage(engine.pageCount() - 1, 140.0, false, 0);
        QVERIFY(!lastImg.isNull());
        int nonWhite = 0;
        for (int y = 0; y < lastImg.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb*>(lastImg.constScanLine(y));
            for (int x = 0; x < lastImg.width(); ++x) {
                if (line[x] != qRgb(255, 255, 255)) nonWhite++;
            }
        }
        QVERIFY(nonWhite > 50);
    }

    void testComicDotSlashPrefixArchive() {
        QTemporaryDir temp;
        const QString cbzPath = temp.filePath("dotslash.cbz");

        QList<TestEntry> entries;
        entries.append(TestEntry{QStringLiteral("./page_01.png"), createTestPng("DotSlash Page 1")});
        entries.append(TestEntry{QStringLiteral("./page_02.png"), createTestPng("DotSlash Page 2")});
        createZipArchive(cbzPath, entries);

        DocumentEngine engine;
        QVERIFY(engine.load(cbzPath));
        QCOMPARE(engine.pageCount(), 2);
    }
};

QTEST_MAIN(OmadocTests)
#include "omadoc_tests.moc"
