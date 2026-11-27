#ifndef OMADOC_DOCUMENTENGINE_H
#define OMADOC_DOCUMENTENGINE_H

#include "documentbackend.h"

#include <QObject>
#include <QString>
#include <QImage>
#include <QSizeF>
#include <QList>
#include <QRectF>
#include <QVariantList>
#include <memory>
#include <mutex>

class DocumentEngine : public QObject {
    Q_OBJECT

public:
    explicit DocumentEngine(QObject *parent = nullptr);
    ~DocumentEngine() override;

    bool load(const QString &path);
    void close();
    bool saveCopy(const QString &targetPath);

    bool isLoaded() const;
    QString filePath() const;
    int pageCount() const;
    QSizeF pageSize(int index) const;
    QImage renderPage(int index, double dpi = 150.0, bool darkMode = false, int rotation = 0);

    QString title() const;
    QString author() const;
    QVariantList tableOfContents();
    QList<int> search(const QString &query) const;
    QList<QRectF> searchHighlights(int index, const QString &query) const;

signals:
    void documentLoaded();
    void documentClosed();

private:
    QString m_filePath;
    std::unique_ptr<IDocumentBackend> m_backend;
    mutable std::mutex m_mutex;
};

#endif
