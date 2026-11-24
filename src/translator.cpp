#include "translator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>

Translator::Translator(const QString &configFile, const QString &catalogBase, QObject *parent)
    : QTranslator(parent),
      m_configPath(configFile.isEmpty() ? QDir::homePath() + "/.config/omarchy/language" : configFile),
      m_catalogPath(catalogBase) {

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reloadLanguage);

    reloadLanguage();
}

QString Translator::resolveLanguageCode(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("en");
    }
    const QString code = QString::fromUtf8(file.readLine()).trimmed();
    return code.isEmpty() ? QStringLiteral("en") : code;
}

QString Translator::translate(const char *, const char *sourceText, const char *, int n) const {
    const QString key = QString::fromUtf8(sourceText);
    const QJsonValue val = m_entries.value(key);
    if (val.isString()) {
        return val.toString();
    }
    if (val.isObject()) {
        const QJsonObject plurals = val.toObject();
        if (n == 1 && plurals.contains("one")) return plurals.value("one").toString();
        if (plurals.contains("other")) return plurals.value("other").toString();
    }
    return {};
}

void Translator::reloadLanguage() {
    const auto monitored = m_watcher.files() + m_watcher.directories();
    if (!monitored.isEmpty())
        m_watcher.removePaths(monitored);

    const QString dir = QFileInfo(m_configPath).absolutePath();
    if (QFileInfo::exists(dir)) m_watcher.addPath(dir);
    if (QFileInfo::exists(m_configPath)) m_watcher.addPath(m_configPath);

    const QString targetLang = resolveLanguageCode(m_configPath);
    if (targetLang == m_language && !m_entries.isEmpty())
        return;

    QHash<QString, QJsonValue> loaded;
    const QStringList fallbackChain = { QStringLiteral("en"), targetLang.section('-', 0, 0), targetLang };
    for (const auto &code : fallbackChain) {
        QFile catalog(m_catalogPath + "/" + code + ".json");
        if (!catalog.open(QIODevice::ReadOnly)) continue;
        const QJsonObject obj = QJsonDocument::fromJson(catalog.readAll()).object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            loaded.insert(it.key(), it.value());
        }
    }

    m_language = targetLang;
    m_entries = loaded;

    const QLocale locale(targetLang);
    QLocale::setDefault(locale);
    QGuiApplication::setLayoutDirection(locale.textDirection());

    emit changed();
}
