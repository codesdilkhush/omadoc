#ifndef OMADOC_EPUBBACKEND_H
#define OMADOC_EPUBBACKEND_H

#include "documentbackend.h"
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>

struct EpubPageInfo {
    int spineIndex = 0;
    int subPage = 0;
    int totalSubPages = 1;
    QString chapterTitle;
    QString imageHref;
    QString href;
    QString htmlText;
    QString plainText;
    QSizeF size;
};

class EpubBackend : public IDocumentBackend {
public:
    EpubBackend();
    ~EpubBackend() override;

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
    void clearUnlocked();

    QString m_filePath;
    QString m_title;
    QString m_author;
    QHash<QString, QByteArray> m_files;
    QList<EpubPageInfo> m_pages;
    QVariantList m_toc;
    mutable QMutex m_mutex;
};

#endif // OMADOC_EPUBBACKEND_H
