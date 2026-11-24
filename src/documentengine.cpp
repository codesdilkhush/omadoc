#include "documentengine.h"
#include "pdfbackend.h"
#include "comicbackend.h"
#include "epubbackend.h"

DocumentEngine::DocumentEngine(QObject *parent) : QObject(parent) {}

DocumentEngine::~DocumentEngine() {
    close();
}

bool DocumentEngine::load(const QString &path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_backend) {
        m_backend->close();
        m_backend.reset();
        m_filePath.clear();
    }

    const QString lower = path.toLower();

    // 1. Comic / Manga Archives
    if (lower.endsWith(QStringLiteral(".cbz")) || lower.endsWith(QStringLiteral(".cbr")) ||
        lower.endsWith(QStringLiteral(".cbt")) || lower.endsWith(QStringLiteral(".cb7"))) {
        auto comic = std::make_unique<ComicBackend>();
        if (comic->load(path)) {
            m_backend = std::move(comic);
            m_filePath = path;
            emit documentLoaded();
            return true;
        }
    }

    // 2. EPUB Ebooks
    if (lower.endsWith(QStringLiteral(".epub"))) {
        auto epub = std::make_unique<EpubBackend>();
        if (epub->load(path)) {
            m_backend = std::move(epub);
            m_filePath = path;
            emit documentLoaded();
            return true;
        }
    }

    // 3. PDF via Poppler
    auto pdf = std::make_unique<PdfBackend>();
    if (pdf->load(path)) {
        m_backend = std::move(pdf);
        m_filePath = path;
        emit documentLoaded();
        return true;
    }

    // 4. Fallback autodetection if extension doesn't match
    auto epubFallback = std::make_unique<EpubBackend>();
    if (epubFallback->load(path)) {
        m_backend = std::move(epubFallback);
        m_filePath = path;
        emit documentLoaded();
        return true;
    }

    auto comicFallback = std::make_unique<ComicBackend>();
    if (comicFallback->load(path)) {
        m_backend = std::move(comicFallback);
        m_filePath = path;
        emit documentLoaded();
        return true;
    }

    m_filePath.clear();
    emit documentClosed();
    return false;
}

void DocumentEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_backend) {
        m_backend->close();
        m_backend.reset();
        m_filePath.clear();
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
