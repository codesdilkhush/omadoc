#ifndef OMADOC_THEME_H
#define OMADOC_THEME_H

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &statePath = {}, QObject *parent = nullptr, const QString &hyprctlExecutable = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }

    static QString readAccentColor(const QString &filePath);

signals:
    void changed();

private:
    void reloadTheme();
    void queryHyprlandRounding();

    QString m_stateDirectory;
    QString m_accent = QStringLiteral("#FFD60A");
    int m_radius = 0;

    QFileSystemWatcher m_watcher;
    QTimer m_debounceTimer;
    QString m_hyprctlPath;
    QProcess m_roundingProcess;
    QTimer m_roundingPollTimer;
};

#endif
