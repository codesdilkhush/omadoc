#ifndef OMADOC_PORTALFILEPICKER_H
#define OMADOC_PORTALFILEPICKER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class PortalFilePicker : public QObject {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    Q_INVOKABLE void openFile(const QString &title = QStringLiteral("Open Document"));

signals:
    void fileSelected(const QString &filePath);
    void cancelled();

private slots:
    void onResponse(uint response, const QVariantMap &results);
};

#endif
