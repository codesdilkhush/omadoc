#include "pageimageprovider.h"
#include "documentengine.h"

#include <QStringList>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace {
constexpr qsizetype kMaxImageCacheBytes = 256LL * 1024 * 1024;

qsizetype imageCacheCost(const QImage &img) {
    return std::max<qsizetype>(1, img.sizeInBytes());
}
} // namespace

PageImageProvider::PageImageProvider(DocumentEngine *engine)
    : QQuickImageProvider(QQuickImageProvider::Image), m_engine(engine) {
    m_cache.setMaxCost(kMaxImageCacheBytes);
}

QImage PageImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(requestedSize);
    const uint64_t startGen = m_generation.load();

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
    const int normDpi = static_cast<int>(std::round(dpi));
    const QString rawDpi = parts.size() > 1 ? parts.value(1) : QStringLiteral("150");
    const bool darkMode = parts.size() > 2 ? (parts.value(2) == "1") : false;
    const int rotation = ((parts.size() > 3 ? parts.value(3).toInt() : 0) % 360 + 360) % 360;
    const QString revision = parts.size() > 4 ? parts.value(4) : QStringLiteral("0");

    // Fast rotation path: check if another rotation of this exact page is already cached
    QImage sourceImg;
    int sourceRot = -1;
    {
        QMutexLocker locker(&m_mutex);
        const int allRotations[] = {0, 90, 180, 270};
        const QStringList dpiTokens = (rawDpi == QString::number(normDpi))
            ? QStringList{rawDpi}
            : QStringList{rawDpi, QString::number(normDpi)};

        for (int otherRot : allRotations) {
            if (otherRot == rotation) continue;
            for (const QString &tok : dpiTokens) {
                const QString otherId = QStringLiteral("%1_%2_%3_%4_%5")
                    .arg(pageIndex)
                    .arg(tok)
                    .arg(darkMode ? QStringLiteral("1") : QStringLiteral("0"))
                    .arg(otherRot)
                    .arg(revision);
                if (m_cache.contains(otherId)) {
                    QImage *cached = m_cache.object(otherId);
                    if (cached && !cached->isNull()) {
                        sourceImg = *cached;
                        sourceRot = otherRot;
                        break;
                    }
                }
            }
            if (sourceRot >= 0) break;
        }
    }

    if (sourceRot >= 0) {
        int delta = (rotation - sourceRot) % 360;
        if (delta < 0) delta += 360;
        QTransform t;
        t.rotate(delta);
        QImage rotated = sourceImg.transformed(t);
        if (m_generation.load() == startGen && !rotated.isNull()) {
            QMutexLocker locker(&m_mutex);
            if (m_generation.load() == startGen) {
                m_cache.insert(id, new QImage(rotated), imageCacheCost(rotated));
            }
        }
        if (size) *size = rotated.size();
        return rotated;
    }

    // If not cached and rotation != 0, render base rotation 0 first and transform
    if (rotation != 0) {
        const QImage base = m_engine ? m_engine->renderPage(pageIndex, dpi, darkMode, 0) : QImage();
        if (!base.isNull()) {
            const QString baseId = QStringLiteral("%1_%2_%3_0_%4")
                .arg(pageIndex)
                .arg(normDpi)
                .arg(darkMode ? QStringLiteral("1") : QStringLiteral("0"))
                .arg(revision);

            QTransform t;
            t.rotate(rotation);
            QImage rotated = base.transformed(t);

            if (m_generation.load() == startGen) {
                QMutexLocker locker(&m_mutex);
                if (m_generation.load() == startGen) {
                    m_cache.insert(baseId, new QImage(base), imageCacheCost(base));
                    m_cache.insert(id, new QImage(rotated), imageCacheCost(rotated));
                }
            }

            if (size) *size = rotated.size();
            return rotated;
        }
    }

    const QImage img = m_engine ? m_engine->renderPage(pageIndex, dpi, darkMode, rotation) : QImage();

    if (!img.isNull() && m_generation.load() == startGen) {
        QMutexLocker locker(&m_mutex);
        if (m_generation.load() == startGen) {
            m_cache.insert(id, new QImage(img), imageCacheCost(img));
        }
    }

    if (size) *size = img.size();
    return img;
}

void PageImageProvider::clearCache() {
    m_generation.fetch_add(1);
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}
