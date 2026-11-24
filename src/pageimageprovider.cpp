#include "pageimageprovider.h"
#include "documentengine.h"

#include <QStringList>

PageImageProvider::PageImageProvider(DocumentEngine *engine)
    : QQuickImageProvider(QQuickImageProvider::Image), m_engine(engine) {
    m_cache.setMaxCost(32);
}

QImage PageImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(requestedSize);

    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(id)) {
            QImage *cached = m_cache.object(id);
            if (size) *size = cached->size();
            return *cached;
        }
    }

    const QStringList parts = id.split('_');
    const int pageIndex = parts.value(0).toInt();
    double dpi = parts.size() > 1 ? parts.value(1).toDouble() : 150.0;
    if (dpi <= 10.0) dpi = 150.0;
    const bool darkMode = parts.size() > 2 ? (parts.value(2) == "1") : false;
    const int rotation = parts.size() > 3 ? parts.value(3).toInt() : 0;

    const QImage img = m_engine ? m_engine->renderPage(pageIndex, dpi, darkMode, rotation) : QImage();

    if (!img.isNull()) {
        QMutexLocker locker(&m_mutex);
        m_cache.insert(id, new QImage(img), 1);
    }

    if (size) *size = img.size();
    return img;
}

void PageImageProvider::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}
