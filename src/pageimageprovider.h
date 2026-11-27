#ifndef OMADOC_PAGEIMAGEPROVIDER_H
#define OMADOC_PAGEIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QCache>
#include <QMutex>
#include <atomic>
#include <cstdint>

class DocumentEngine;

class PageImageProvider : public QQuickImageProvider {
public:
    explicit PageImageProvider(DocumentEngine *engine);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    void clearCache();

private:
    DocumentEngine *m_engine;
    QCache<QString, QImage> m_cache;
    QMutex m_mutex;
    std::atomic<uint64_t> m_generation{0};
};

#endif
