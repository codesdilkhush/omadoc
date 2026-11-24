#ifndef OMADOC_BACKEND_H
#define OMADOC_BACKEND_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QList>
#include <QSizeF>

class DocumentEngine;
class PageImageProvider;
class PortalFilePicker;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY documentChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QVariantList toc READ toc NOTIFY tocChanged)
    Q_PROPERTY(bool tocVisible READ tocVisible WRITE setTocVisible NOTIFY tocVisibleChanged)
    Q_PROPERTY(bool helpVisible READ helpVisible WRITE setHelpVisible NOTIFY helpVisibleChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged)
    Q_PROPERTY(int searchResultsCount READ searchResultsCount NOTIFY searchChanged)
    Q_PROPERTY(int searchCurrentIndex READ searchCurrentIndex NOTIFY searchChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit Backend(DocumentEngine *engine, PageImageProvider *provider, QObject *parent = nullptr);

    QString filePath() const;
    QString fileName() const;
    QString title() const;
    int pageCount() const;
    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int page);

    double zoom() const { return m_zoom; }
    void setZoom(double z);

    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool dark);

    bool hasDocument() const;
    QVariantList toc() const { return m_toc; }

    bool tocVisible() const { return m_tocVisible; }
    void setTocVisible(bool visible);

    bool helpVisible() const { return m_helpVisible; }
    void setHelpVisible(bool visible);

    QString searchQuery() const { return m_searchQuery; }
    int searchResultsCount() const { return m_searchResults.size(); }
    int searchCurrentIndex() const { return m_searchIndex; }
    int revision() const { return m_revision; }

    Q_INVOKABLE void openFile(const QString &path);
    Q_INVOKABLE void openPortal();
    Q_INVOKABLE void closeDocument();

    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void goToPage(int page);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetZoom();

    Q_INVOKABLE void toggleDarkMode();
    Q_INVOKABLE void toggleToc();
    Q_INVOKABLE void toggleHelp();

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void nextSearchResult();
    Q_INVOKABLE void prevSearchResult();
    Q_INVOKABLE void clearSearch();

    Q_INVOKABLE QString pageImageUrl(int pageIndex, double dpi = 150.0) const;
    Q_INVOKABLE QSizeF pageSize(int pageIndex) const;

signals:
    void documentChanged();
    void currentPageChanged();
    void zoomChanged();
    void darkModeChanged();
    void tocChanged();
    void tocVisibleChanged();
    void helpVisibleChanged();
    void searchChanged();
    void revisionChanged();

private:
    DocumentEngine *m_engine;
    PageImageProvider *m_provider;
    PortalFilePicker *m_picker;

    int m_currentPage = 0;
    double m_zoom = 1.0;
    bool m_darkMode = false;
    bool m_tocVisible = false;
    bool m_helpVisible = false;
    int m_revision = 0;

    QVariantList m_toc;
    QString m_searchQuery;
    QList<int> m_searchResults;
    int m_searchIndex = -1;
};

#endif
