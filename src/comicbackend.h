#ifndef OMADOC_COMICBACKEND_H
#define OMADOC_COMICBACKEND_H

#include "documentbackend.h"
#include <QByteArray>
#include <QList>
#include <QMutex>

struct ComicPageEntry {
    QString name;
    QByteArray data;
    QSizeF size;
};

class ComicBackend : public IDocumentBackend {
public:
    ComicBackend();
    ~ComicBackend() override;

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

private:
    void clearUnlocked();

    QString m_filePath;
    QString m_title;
    QString m_author;
    QList<ComicPageEntry> m_pages;
    mutable QMutex m_mutex;
};

#endif // OMADOC_COMICBACKEND_H
