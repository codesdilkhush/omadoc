#ifndef OMADOC_TRANSLATOR_H
#define OMADOC_TRANSLATOR_H

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonValue>
#include <QLocale>
#include <QTimer>
#include <QTranslator>

class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)

public:
    explicit Translator(const QString &configFile = {}, const QString &catalogBase = ":/i18n", QObject *parent = nullptr);

    QString language() const { return m_language; }
    bool isEmpty() const override { return m_entries.isEmpty(); }
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override;

    static QString resolveLanguageCode(const QString &path);

signals:
    void changed();

private:
    void reloadLanguage();

    QString m_configPath;
    QString m_catalogPath;
    QString m_language = QStringLiteral("en");
    QHash<QString, QJsonValue> m_entries;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

#endif
