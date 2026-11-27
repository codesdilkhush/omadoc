#include "pdfbackend.h"
#include "archiveutils.h"

#include <poppler-document.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>
#include <poppler-toc.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>

#include <algorithm>
#include <functional>

PdfBackend::PdfBackend() = default;

PdfBackend::~PdfBackend() {
    close();
}

bool PdfBackend::load(const QString &filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = filePath;
    m_doc.reset(poppler::document::load_from_file(filePath.toStdString()));
    if (!m_doc || m_doc->is_locked()) {
        m_doc.reset();
        m_filePath.clear();
        return false;
    }
    return true;
}

void PdfBackend::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_doc) {
        m_doc.reset();
        m_filePath.clear();
    }
}

bool PdfBackend::saveCopy(const QString &targetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || targetPath.isEmpty())
        return false;

    const QFileInfo targetInfo(targetPath);
    QDir dir = targetInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QString tempPath = dir.absoluteFilePath(
        QStringLiteral(".omadoc_%1_%2.tmp")
            .arg(QRandomGenerator::global()->generate())
            .arg(targetInfo.fileName())
    );

    bool saved = m_doc->save_a_copy(tempPath.toStdString());
    if (!saved || !QFile::exists(tempPath) || QFileInfo(tempPath).size() == 0) {
        if (QFile::exists(tempPath)) QFile::remove(tempPath);

        tempPath = QDir::temp().absoluteFilePath(
            QStringLiteral("omadoc_%1_%2.tmp")
                .arg(QRandomGenerator::global()->generate())
                .arg(targetInfo.fileName())
        );
        saved = m_doc->save_a_copy(tempPath.toStdString());
        if (!saved || !QFile::exists(tempPath) || QFileInfo(tempPath).size() == 0) {
            if (QFile::exists(tempPath)) QFile::remove(tempPath);
            return false;
        }
    }

    const bool isSameAsLoaded = (QFileInfo(m_filePath) == targetInfo);

    bool success = false;
    if (QFileInfo(tempPath).absoluteDir() == dir) {
        const QString backupPath = targetPath + QStringLiteral(".bak_%1").arg(QRandomGenerator::global()->generate());
        const bool hadTarget = QFile::exists(targetPath);
        if (hadTarget && !QFile::rename(targetPath, backupPath)) {
            QFile::remove(tempPath);
            return false;
        }
        success = QFile::rename(tempPath, targetPath);
        if (!success) {
            if (hadTarget) {
                QFile::rename(backupPath, targetPath);
            }
            QFile::remove(tempPath);
        } else if (hadTarget) {
            QFile::remove(backupPath);
        }
    } else {
        success = OmadocUtils::safeCopyFile(tempPath, targetPath);
        QFile::remove(tempPath);
    }

    if (success && isSameAsLoaded) {
        m_doc.reset(poppler::document::load_from_file(m_filePath.toStdString()));
        if (!m_doc) {
            return false;
        }
    }

    return success;
}

int PdfBackend::pageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_doc ? m_doc->pages() : 0;
}

QSizeF PdfBackend::pageSize(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || index < 0 || index >= m_doc->pages())
        return QSizeF(0, 0);

    std::unique_ptr<poppler::page> p(m_doc->create_page(index));
    if (!p)
        return QSizeF(0, 0);

    const poppler::rectf rect = p->page_rect();
    return QSizeF(rect.width(), rect.height());
}

QImage PdfBackend::renderPage(int index, double dpi, bool darkMode, int rotation) {
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

    const double safeDpi = (dpi > 0.0) ? std::clamp(dpi, 10.0, 2000.0) : 150.0;
    const int normRot = (rotation % 360 + 360) % 360;
    poppler::rotation_enum rot = poppler::rotate_0;
    if (normRot == 90) rot = poppler::rotate_90;
    else if (normRot == 180) rot = poppler::rotate_180;
    else if (normRot == 270) rot = poppler::rotate_270;

    poppler::image img = renderer.render_page(p.get(), safeDpi, safeDpi, -1, -1, -1, -1, rot);
    if (!img.is_valid())
        return QImage();

    QImage qimg(reinterpret_cast<const uchar*>(img.const_data()),
                img.width(), img.height(), img.bytes_per_row(),
                QImage::Format_ARGB32);
    QImage result = qimg.copy();

    if (darkMode) {
        OmadocUtils::invertImageRgbSafely(result);
    }

    return result;
}

QString PdfBackend::title() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc) return {};
    auto s = m_doc->get_title().to_utf8();
    return QString::fromUtf8(s.data(), s.size());
}

QString PdfBackend::author() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc) return {};
    auto s = m_doc->get_author().to_utf8();
    return QString::fromUtf8(s.data(), s.size());
}

QVariantList PdfBackend::tableOfContents() {
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

QList<int> PdfBackend::search(const QString &query) const {
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

QList<QRectF> PdfBackend::searchHighlights(int index, const QString &query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QList<QRectF> highlights;
    const QString trimmed = query.trimmed();
    if (!m_doc || trimmed.isEmpty() || index < 0 || index >= m_doc->pages())
        return highlights;

    std::unique_ptr<poppler::page> p(m_doc->create_page(index));
    if (!p) return highlights;

    const poppler::rectf pageRect = p->page_rect(poppler::crop_box);
    if (pageRect.width() <= 0.0 || pageRect.height() <= 0.0)
        return highlights;

    const QByteArray utf8 = trimmed.toUtf8();
    const poppler::ustring needle = poppler::ustring::from_utf8(utf8.constData(), utf8.size());

    poppler::rectf r;
    auto dir = poppler::page::search_from_top;
    constexpr int kMaxMatches = 500;
    for (int count = 0; count < kMaxMatches; ++count) {
        if (!p->search(needle, r, dir, poppler::case_insensitive, poppler::rotate_0)) {
            break;
        }
        dir = poppler::page::search_next_result;

        const double nx = std::clamp((r.left() - pageRect.left()) / pageRect.width(), 0.0, 1.0);
        const double ny = std::clamp((r.top() - pageRect.top()) / pageRect.height(), 0.0, 1.0);
        const double nw = std::clamp(r.width() / pageRect.width(), 0.0, 1.0 - nx);
        const double nh = std::clamp(r.height() / pageRect.height(), 0.0, 1.0 - ny);
        if (nw > 0.0 && nh > 0.0) {
            highlights.append(QRectF(nx, ny, nw, nh));
        }
    }
    return highlights;
}
