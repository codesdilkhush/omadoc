#ifndef OMADOC_ARCHIVEUTILS_H
#define OMADOC_ARCHIVEUTILS_H

#include <archive.h>
#include <archive_entry.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QString>

namespace OmadocUtils {

inline QByteArray readArchiveEntryData(struct archive *a,
                                       struct archive_entry *entry,
                                       qint64 maxBytes = 256LL * 1024 * 1024,
                                       bool *ok = nullptr) {
    if (ok) *ok = true;
    const la_int64_t declaredSize = archive_entry_size(entry);
    if (declaredSize > maxBytes) {
        archive_read_data_skip(a);
        if (ok) *ok = false;
        return {};
    }

    QByteArray data;
    if (declaredSize > 0) {
        data.reserve(static_cast<qsizetype>(declaredSize));
    }

    char buf[16384];
    ssize_t n = 0;
    while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) {
        if (data.size() + n > maxBytes) {
            if (ok) *ok = false;
            return {};
        }
        data.append(buf, static_cast<qsizetype>(n));
    }

    if (n < 0) {
        if (ok) *ok = false;
        return {};
    }
    return data;
}

inline bool safeCopyFile(const QString &sourcePath, const QString &targetPath) {
    if (sourcePath.isEmpty() || targetPath.isEmpty())
        return false;

    const QFileInfo srcInfo(sourcePath);
    const QFileInfo dstInfo(targetPath);
    if (!srcInfo.exists() || !srcInfo.isFile())
        return false;
    if (srcInfo == dstInfo)
        return true;

    QDir dstDir = dstInfo.absoluteDir();
    if (!dstDir.exists() && !dstDir.mkpath(QStringLiteral(".")))
        return false;

    QFile src(sourcePath);
    if (!src.open(QIODevice::ReadOnly))
        return false;

    QSaveFile dst(targetPath);
    if (!dst.open(QIODevice::WriteOnly))
        return false;

    char buf[65536];
    while (!src.atEnd()) {
        const qint64 bytesRead = src.read(buf, sizeof(buf));
        if (bytesRead < 0) {
            dst.cancelWriting();
            return false;
        }
        if (bytesRead > 0 && dst.write(buf, bytesRead) != bytesRead) {
            dst.cancelWriting();
            return false;
        }
    }

    return dst.commit();
}

inline void invertImageRgbSafely(QImage &img) {
    if (img.isNull())
        return;

    if (img.format() == QImage::Format_ARGB32_Premultiplied ||
        img.format() == QImage::Format_RGBA8888_Premultiplied ||
        img.format() == QImage::Format_RGBA64_Premultiplied) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    img.invertPixels(QImage::InvertRgb);
}

} // namespace OmadocUtils

#endif // OMADOC_ARCHIVEUTILS_H
