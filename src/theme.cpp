#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &statePath, QObject *parent, const QString &hyprctlExecutable)
    : QObject(parent),
      m_stateDirectory(statePath.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : statePath) {

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounceTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounceTimer.start(); });
    connect(&m_debounceTimer, &QTimer::timeout, this, &Theme::reloadTheme);

    m_hyprctlPath = !hyprctlExecutable.isEmpty() ? hyprctlExecutable
                  : (statePath.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
                        ? QStandardPaths::findExecutable("hyprctl") : QString();

    connect(&m_roundingProcess, &QProcess::finished, this, [this](int exitCode) {
        if (exitCode != 0) return;
        const auto doc = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput());
        const int val = doc.object().value("int").toInt(-1);
        if (val >= 0 && val != m_radius) {
            m_radius = val;
            emit changed();
        }
    });

    m_roundingPollTimer.setInterval(1500);
    connect(&m_roundingPollTimer, &QTimer::timeout, this, &Theme::queryHyprlandRounding);

    if (!m_hyprctlPath.isEmpty()) {
        m_roundingPollTimer.start();
        queryHyprlandRounding();
    }

    reloadTheme();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(500);
    }
}

void Theme::queryHyprlandRounding() {
    if (m_hyprctlPath.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctlPath, QStringList{"-j", "getoption", "decoration:rounding"});
}

QString Theme::readAccentColor(const QString &filePath) {
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        static const QRegularExpression regex(R"(accent\s*=\s*["'](#[0-9a-fA-F]{6})["'])");
        const auto match = regex.match(QString::fromUtf8(file.readAll()));
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }
    return QStringLiteral("#FFD60A");
}

QString Theme::accentForeground() const {
    const QColor col(m_accent);
    auto toLinear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const double lum = 0.2126 * toLinear(col.redF()) +
                       0.7152 * toLinear(col.greenF()) +
                       0.0722 * toLinear(col.blueF());
    return lum > 0.179 ? QStringLiteral("black") : QStringLiteral("white");
}

void Theme::reloadTheme() {
    queryHyprlandRounding();

    const auto watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);

    const QString colorFile = m_stateDirectory + "/theme/colors.toml";
    const QString themeDir = m_stateDirectory + "/theme";

    if (QFileInfo::exists(colorFile)) m_watcher.addPath(colorFile);
    if (QFileInfo::exists(themeDir)) m_watcher.addPath(themeDir);
    if (QFileInfo::exists(m_stateDirectory)) m_watcher.addPath(m_stateDirectory);

    const QString newAccent = readAccentColor(colorFile);
    if (newAccent != m_accent) {
        m_accent = newAccent;
        emit changed();
    }
}
