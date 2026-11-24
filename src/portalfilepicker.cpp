#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QUrl>

PortalFilePicker::PortalFilePicker(QObject *parent) : QObject(parent) {}

void PortalFilePicker::openFile(const QString &title) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        emit cancelled();
        return;
    }

    const QString token = QStringLiteral("omadoc_%1").arg(QRandomGenerator::global()->generate());

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("multiple"), false);

    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.FileChooser"),
        QStringLiteral("OpenFile")
    );

    request << QString() << title << options;

    QDBusPendingCall pcall = QDBusConnection::sessionBus().asyncCall(request);
    auto *watcher = new QDBusPendingCallWatcher(pcall, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            emit cancelled();
            return;
        }

        const QString requestPath = reply.value().path();
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            requestPath,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onResponse(uint, QVariantMap))
        );
    });
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    if (response == 0 && results.contains(QStringLiteral("uris"))) {
        const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
        if (!uris.isEmpty()) {
            QUrl url(uris.first());
            emit fileSelected(url.isLocalFile() ? url.toLocalFile() : uris.first());
            return;
        }
    }
    emit cancelled();
}
