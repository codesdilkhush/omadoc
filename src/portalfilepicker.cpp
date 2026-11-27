#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QUrl>

PortalFilePicker::PortalFilePicker(QObject *parent) : QObject(parent) {}

QString PortalFilePicker::predictedRequestPath(const QString &token) const {
    QString sender = QDBusConnection::sessionBus().baseService();
    sender.replace('.', '_');
    if (sender.startsWith(':')) {
        sender.remove(0, 1);
    }
    if (sender.isEmpty()) {
        return QString();
    }
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

void PortalFilePicker::disconnectRequestPath(QString &path, const char *slot) {
    if (!path.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            path,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            slot
        );
        path.clear();
    }
}

void PortalFilePicker::openFile(const QString &title) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        emit cancelled();
        return;
    }

    disconnectRequestPath(m_openRequestPath, SLOT(onResponse(uint, QVariantMap)));

    const QString token = QStringLiteral("omadoc_%1").arg(QRandomGenerator::global()->generate());
    m_openRequestPath = predictedRequestPath(token);
    if (!m_openRequestPath.isEmpty()) {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            m_openRequestPath,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onResponse(uint, QVariantMap))
        );
    }

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
            disconnectRequestPath(m_openRequestPath, SLOT(onResponse(uint, QVariantMap)));
            emit cancelled();
            return;
        }

        const QString requestPath = reply.value().path();
        if (!requestPath.isEmpty() && requestPath != m_openRequestPath) {
            disconnectRequestPath(m_openRequestPath, SLOT(onResponse(uint, QVariantMap)));
            m_openRequestPath = requestPath;
            QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                m_openRequestPath,
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"),
                this,
                SLOT(onResponse(uint, QVariantMap))
            );
        }
    });
}

void PortalFilePicker::saveFile(const QString &suggestedName, const QString &title, const QString &defaultSuffix) {
    if (!QDBusConnection::sessionBus().isConnected()) {
        emit cancelled();
        return;
    }

    disconnectRequestPath(m_saveRequestPath, SLOT(onSaveResponse(uint, QVariantMap)));

    m_defaultSaveSuffix = defaultSuffix;
    if (m_defaultSaveSuffix.startsWith('.')) {
        m_defaultSaveSuffix.remove(0, 1);
    }

    const QString token = QStringLiteral("omadoc_save_%1").arg(QRandomGenerator::global()->generate());
    m_saveRequestPath = predictedRequestPath(token);
    if (!m_saveRequestPath.isEmpty()) {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"),
            m_saveRequestPath,
            QStringLiteral("org.freedesktop.portal.Request"),
            QStringLiteral("Response"),
            this,
            SLOT(onSaveResponse(uint, QVariantMap))
        );
    }

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    if (!suggestedName.isEmpty()) {
        QString name = suggestedName;
        if (QFileInfo(name).suffix().isEmpty() && !m_defaultSaveSuffix.isEmpty()) {
            name += QStringLiteral(".") + m_defaultSaveSuffix;
        }
        options.insert(QStringLiteral("current_name"), name);
    }

    QDBusMessage request = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.FileChooser"),
        QStringLiteral("SaveFile")
    );

    request << QString() << title << options;

    QDBusPendingCall pcall = QDBusConnection::sessionBus().asyncCall(request);
    auto *watcher = new QDBusPendingCallWatcher(pcall, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            disconnectRequestPath(m_saveRequestPath, SLOT(onSaveResponse(uint, QVariantMap)));
            emit cancelled();
            return;
        }

        const QString requestPath = reply.value().path();
        if (!requestPath.isEmpty() && requestPath != m_saveRequestPath) {
            disconnectRequestPath(m_saveRequestPath, SLOT(onSaveResponse(uint, QVariantMap)));
            m_saveRequestPath = requestPath;
            QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                m_saveRequestPath,
                QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"),
                this,
                SLOT(onSaveResponse(uint, QVariantMap))
            );
        }
    });
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    disconnectRequestPath(m_openRequestPath, SLOT(onResponse(uint, QVariantMap)));

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

void PortalFilePicker::onSaveResponse(uint response, const QVariantMap &results) {
    disconnectRequestPath(m_saveRequestPath, SLOT(onSaveResponse(uint, QVariantMap)));

    if (response == 0 && results.contains(QStringLiteral("uris"))) {
        const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
        if (!uris.isEmpty()) {
            QUrl url(uris.first());
            QString path = url.isLocalFile() ? url.toLocalFile() : uris.first();
            if (QFileInfo(path).suffix().isEmpty() && !m_defaultSaveSuffix.isEmpty()) {
                path += QStringLiteral(".") + m_defaultSaveSuffix;
            }
            emit saveTargetSelected(path);
            return;
        }
    }
    emit cancelled();
}
