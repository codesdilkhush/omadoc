#include "documentengine.h"
#include "pdfbackend.h"
#include "comicbackend.h"
#include "epubbackend.h"

#include <QFileInfo>

DocumentEngine::DocumentEngine(QObject *parent) : QObject(parent) {}

DocumentEngine::~DocumentEngine() {
    close();
}

bool DocumentEngine::load(const QString &path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }

    bool loaded = false;
    bool hadPrevious = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_backend) {
            m_backend->close();
            m_backend.reset();
            m_filePath.clear();
            hadPrevious = true;
        }

        const QString lower = path.toLower();
        bool triedComic = false;
        bool triedEpub = false;

        // 1. Comic / Manga Archives
        if (lower.endsWith(QStringLiteral(".cbz")) || lower.endsWith(QStringLiteral(".cbr")) ||
            lower.endsWith(QStringLiteral(".cbt")) || lower.endsWith(QStringLiteral(".cb7"))) {
            triedComic = true;
            auto comic = std::make_unique<ComicBackend>();
            if (comic->load(path)) {
                m_backend = std::move(comic);
                m_filePath = path;
                loaded = true;
            }
        }

        // 2. EPUB Ebooks
        if (!loaded && lower.endsWith(QStringLiteral(".epub"))) {
            triedEpub = true;
            auto epub = std::make_unique<EpubBackend>();
            if (epub->load(path)) {
                m_backend = std::move(epub);
                m_filePath = path;
                loaded = true;
            }
        }

        // 3. PDF via Poppler (skip if known archive extension failed unless fallback needed)
        if (!loaded && !triedComic && !triedEpub) {
            auto pdf = std::make_unique<PdfBackend>();
            if (pdf->load(path)) {
                m_backend = std::move(pdf);
                m_filePath = path;
                loaded = true;
            }
        }

        // 4. Fallback autodetection if extension didn't match
        if (!loaded && !triedEpub) {
            auto epubFallback = std::make_unique<EpubBackend>();
            if (epubFallback->load(path)) {
                m_backend = std::move(epubFallback);
                m_filePath = path;
                loaded = true;
            }
        }

        if (!loaded && !triedComic) {
            auto comicFallback = std::make_unique<ComicBackend>();
            if (comicFallback->load(path)) {
                m_backend = std::move(comicFallback);
                m_filePath = path;
                loaded = true;
            }
        }

        if (!loaded && (triedComic || triedEpub)) {
            auto pdfFallback = std::make_unique<PdfBackend>();
            if (pdfFallback->load(path)) {
                m_backend = std::move(pdfFallback);
                m_filePath = path;
                loaded = true;
            }
        }
    }

    if (loaded) {
        emit documentLoaded();
        return true;
    }

    if (hadPrevious) {
        emit documentClosed();
    }
    return false;
}

void DocumentEngine::close() {
    bool wasLoaded = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_backend) {
            m_backend->close();
            m_backend.reset();
            m_filePath.clear();
            wasLoaded = true;
        }
    }
    if (wasLoaded) {
        emit documentClosed();
    }
}

bool DocumentEngine::saveCopy(const QString &targetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->saveCopy(targetPath) : false;
}

bool DocumentEngine::isLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend != nullptr && m_backend->pageCount() > 0;
}

QString DocumentEngine::filePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_filePath;
}

int DocumentEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->pageCount() : 0;
}

QSizeF DocumentEngine::pageSize(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->pageSize(index) : QSizeF(0, 0);
}

QImage DocumentEngine::renderPage(int index, double dpi, bool darkMode, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->renderPage(index, dpi, darkMode, rotation) : QImage();
}

QString DocumentEngine::title() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->title() : QString();
}

QString DocumentEngine::author() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->author() : QString();
}

QVariantList DocumentEngine::tableOfContents() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->tableOfContents() : QVariantList();
}

QList<int> DocumentEngine::search(const QString &query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->search(query) : QList<int>();
}

QList<QRectF> DocumentEngine::searchHighlights(int index, const QString &query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backend ? m_backend->searchHighlights(index, query) : QList<QRectF>();
}
