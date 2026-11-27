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
    Q_INVOKABLE void saveFile(const QString &suggestedName = QStringLiteral("document.pdf"),
                              const QString &title = QStringLiteral("Save Document"),
                              const QString &defaultSuffix = QString());

signals:
    void fileSelected(const QString &filePath);
    void saveTargetSelected(const QString &filePath);
    void cancelled();

private slots:
    void onResponse(uint response, const QVariantMap &results);
    void onSaveResponse(uint response, const QVariantMap &results);

private:
    QString predictedRequestPath(const QString &token) const;
    void disconnectRequestPath(QString &path, const char *slot);

    QString m_defaultSaveSuffix;
    QString m_openRequestPath;
    QString m_saveRequestPath;
};

#endif
