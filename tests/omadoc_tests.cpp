#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QPdfWriter>
#include <QPainter>

#include "backend.h"
#include "documentengine.h"
#include "pageimageprovider.h"
#include "theme.h"
#include "translator.h"

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

        backend.setZoom(0.05);
        QCOMPARE(backend.zoom(), 0.2);

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

        // Test night mode inversion
        const QImage page0Night = engine.renderPage(0, 72.0, true, 0);
        QVERIFY(!page0Night.isNull());

        // Test text search
        const auto searchRes = engine.search("First");
        QVERIFY(!searchRes.isEmpty());
        QCOMPARE(searchRes.first(), 0);

        backend.closeDocument();
        QCOMPARE(backend.hasDocument(), false);
    }
};

QTEST_MAIN(OmadocTests)
#include "omadoc_tests.moc"
