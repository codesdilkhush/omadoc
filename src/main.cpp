#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

#include "documentengine.h"
#include "pageimageprovider.h"
#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // CLI handling for headless agent/script queries
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "info" && i + 1 < argc) {
            QString path = QString::fromUtf8(argv[i + 1]);
            DocumentEngine engine;
            if (engine.load(path)) {
                QJsonObject obj;
                obj["pages"] = engine.pageCount();
                obj["title"] = engine.title();
                obj["author"] = engine.author();
                obj["file"] = path;
                std::cout << QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString() << std::endl;
                return 0;
            } else {
                std::cerr << "Failed to load document: " << path.toStdString() << std::endl;
                return 1;
            }
        }
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omadoc"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setDesktopFileName(QStringLiteral("omadoc"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omadoc")));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    DocumentEngine docEngine;
    auto *imageProvider = new PageImageProvider(&docEngine);
    Backend backend(&docEngine, imageProvider);

    QQmlApplicationEngine qmlEngine;
    qmlEngine.addImageProvider(QStringLiteral("doc"), imageProvider);

    QObject::connect(&translator, &Translator::changed, &qmlEngine, &QQmlEngine::retranslate);

    qmlEngine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    // If an initial file path was given, open it
    const QStringList posArgs = app.arguments().mid(1);
    for (const auto &arg : posArgs) {
        if (!arg.startsWith('-') && arg != "open") {
            backend.openFile(arg);
            break;
        }
    }

    qmlEngine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (qmlEngine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
