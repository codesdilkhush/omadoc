#include "documentengine.h"

#include <poppler-document.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>
#include <poppler-toc.h>

#include <functional>

DocumentEngine::DocumentEngine(QObject *parent) : QObject(parent) {}

DocumentEngine::~DocumentEngine() {
    close();
}

bool DocumentEngine::load(const QString &path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = path;
    m_doc.reset(poppler::document::load_from_file(path.toStdString()));
    if (!m_doc || m_doc->is_locked()) {
        m_doc.reset();
        m_filePath.clear();
        emit documentClosed();
        return false;
    }
    emit documentLoaded();
    return true;
}

void DocumentEngine::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_doc) {
        m_doc.reset();
        m_filePath.clear();
        emit documentClosed();
    }
}

bool DocumentEngine::saveCopy(const QString &targetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || targetPath.isEmpty())
        return false;
    return m_doc->save_a_copy(targetPath.toStdString());
}

bool DocumentEngine::isLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_doc != nullptr;
}

int DocumentEngine::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_doc ? m_doc->pages() : 0;
}

QSizeF DocumentEngine::pageSize(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || index < 0 || index >= m_doc->pages())
        return QSizeF(0, 0);

    std::unique_ptr<poppler::page> p(m_doc->create_page(index));
    if (!p)
        return QSizeF(0, 0);

    const poppler::rectf rect = p->page_rect();
    return QSizeF(rect.width(), rect.height());
}

QImage DocumentEngine::renderPage(int index, double dpi, bool darkMode, int rotation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || index < 0 || index >= m_doc->pages())
        return QImage();

    std::unique_ptr<poppler::page> p(m_doc->create_page(index));
    if (!p)
        return QImage();

    poppler::page_renderer renderer;
    renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
    renderer.set_render_hint(poppler::page_renderer::text_hinting, true);
    renderer.set_image_format(poppler::image::format_argb32);

    poppler::rotation_enum rot = poppler::rotate_0;
    if (rotation == 90) rot = poppler::rotate_90;
    else if (rotation == 180) rot = poppler::rotate_180;
    else if (rotation == 270) rot = poppler::rotate_270;

    poppler::image img = renderer.render_page(p.get(), dpi, dpi, -1, -1, -1, -1, rot);
    if (!img.is_valid())
        return QImage();

    QImage qimg(reinterpret_cast<const uchar*>(img.const_data()),
                img.width(), img.height(), img.bytes_per_row(),
                QImage::Format_ARGB32);
    QImage result = qimg.copy();

    if (darkMode) {
        result.invertPixels(QImage::InvertRgb);
    }

    return result;
}

QString DocumentEngine::title() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc) return {};
    auto s = m_doc->get_title().to_utf8();
    return QString::fromUtf8(s.data(), s.size());
}

QString DocumentEngine::author() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc) return {};
    auto s = m_doc->get_author().to_utf8();
    return QString::fromUtf8(s.data(), s.size());
}

QVariantList DocumentEngine::tableOfContents() {
    std::lock_guard<std::mutex> lock(m_mutex);
    QVariantList items;
    if (!m_doc) return items;

    std::unique_ptr<poppler::toc> outline(m_doc->create_toc());
    if (!outline || !outline->root())
        return items;

    std::function<void(poppler::toc_item*, int)> walk;
    walk = [&](poppler::toc_item *node, int level) {
        for (auto *child : node->children()) {
            if (!child) continue;
            auto raw = child->title().to_utf8();
            QString heading = QString::fromUtf8(raw.data(), raw.size()).trimmed();
            if (!heading.isEmpty()) {
                QVariantMap row;
                row["title"] = heading;
                row["depth"] = level;
                items.append(row);
            }
            walk(child, level + 1);
        }
    };
    walk(outline->root(), 0);
    return items;
}

QList<int> DocumentEngine::search(const QString &query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QList<int> foundPages;
    if (!m_doc || query.trimmed().isEmpty())
        return foundPages;

    const int total = m_doc->pages();
    for (int i = 0; i < total; ++i) {
        std::unique_ptr<poppler::page> p(m_doc->create_page(i));
        if (!p) continue;
        auto raw = p->text().to_utf8();
        QString content = QString::fromUtf8(raw.data(), raw.size());
        if (content.contains(query, Qt::CaseInsensitive)) {
            foundPages.append(i);
        }
    }
    return foundPages;
}
