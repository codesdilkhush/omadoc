#ifndef OMADOC_DOCUMENTBACKEND_H
#define OMADOC_DOCUMENTBACKEND_H

#include <QString>
#include <QImage>
#include <QSizeF>
#include <QList>
#include <QRectF>
#include <QVariantList>

class IDocumentBackend {
public:
    virtual ~IDocumentBackend() = default;

    virtual bool load(const QString &filePath) = 0;
    virtual void close() = 0;
    virtual bool saveCopy(const QString &targetPath) = 0;

    virtual int pageCount() const = 0;
    virtual QSizeF pageSize(int index) const = 0;
    virtual QImage renderPage(int index, double dpi = 150.0, bool darkMode = false, int rotation = 0) = 0;

    virtual QString title() const = 0;
    virtual QString author() const = 0;
    virtual QVariantList tableOfContents() = 0;
    virtual QList<int> search(const QString &query) const = 0;
    virtual QList<QRectF> searchHighlights(int index, const QString &query) const {
        Q_UNUSED(index);
        Q_UNUSED(query);
        return {};
    }
};

#endif // OMADOC_DOCUMENTBACKEND_H
