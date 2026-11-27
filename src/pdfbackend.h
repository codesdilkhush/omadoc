#ifndef OMADOC_PDFBACKEND_H
#define OMADOC_PDFBACKEND_H

#include "documentbackend.h"
#include <memory>
#include <mutex>

namespace poppler {
class document;
}

class PdfBackend : public IDocumentBackend {
public:
    PdfBackend();
    ~PdfBackend() override;

    bool load(const QString &filePath) override;
    void close() override;
    bool saveCopy(const QString &targetPath) override;

    int pageCount() const override;
    QSizeF pageSize(int index) const override;
    QImage renderPage(int index, double dpi = 150.0, bool darkMode = false, int rotation = 0) override;

    QString title() const override;
    QString author() const override;
    QVariantList tableOfContents() override;
    QList<int> search(const QString &query) const override;
    QList<QRectF> searchHighlights(int index, const QString &query) const override;

private:
    QString m_filePath;
    std::unique_ptr<poppler::document> m_doc;
    mutable std::mutex m_mutex;
};

#endif // OMADOC_PDFBACKEND_H
