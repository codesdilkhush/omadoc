#ifndef OMADOC_BACKEND_H
#define OMADOC_BACKEND_H

#include <QObject>
#include <QString>
#include <QStringList>
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
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QVariantList toc READ toc NOTIFY tocChanged)
    Q_PROPERTY(bool tocVisible READ tocVisible WRITE setTocVisible NOTIFY tocVisibleChanged)
    Q_PROPERTY(bool helpVisible READ helpVisible WRITE setHelpVisible NOTIFY helpVisibleChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY searchChanged)
    Q_PROPERTY(int searchResultsCount READ searchResultsCount NOTIFY searchChanged)
    Q_PROPERTY(int searchCurrentIndex READ searchCurrentIndex NOTIFY searchChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(int fitMode READ fitMode WRITE setFitMode NOTIFY fitModeChanged)
    Q_PROPERTY(int activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QVariantList annotations READ annotations NOTIFY annotationsChanged)
    Q_PROPERTY(int annotationCount READ annotationCount NOTIFY annotationsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

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

    int rotation() const { return m_rotation; }
    void setRotation(int rot);

    int fitMode() const { return m_fitMode; }
    void setFitMode(int mode);

    int activeTool() const { return m_activeTool; }
    void setActiveTool(int tool);

    QVariantList annotations() const { return m_annotations; }
    int annotationCount() const { return m_annotations.size(); }
    Q_INVOKABLE QVariantList pageAnnotations(int pageIndex) const;

    Q_INVOKABLE void addHighlight(int page, double x, double y, double width, double height, const QString &color = QString());
    Q_INVOKABLE void deleteAnnotation(const QString &id);
    Q_INVOKABLE void clearAnnotationsForPage(int page);
    Q_INVOKABLE void clearAllAnnotations();

    QString lastError() const { return m_lastError; }
    Q_INVOKABLE void clearError();

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
    QStringList recentFiles() const { return m_recentFiles; }

    Q_INVOKABLE void openFile(const QString &path);
    Q_INVOKABLE void openRecent(int index);
    Q_INVOKABLE void openPortal();
    Q_INVOKABLE void saveDocument();
    Q_INVOKABLE void saveAsDocument();
    Q_INVOKABLE void closeDocument();
    Q_INVOKABLE void clearRecents();

    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void goToPage(int page);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void fitToWidth(double viewportWidth);
    Q_INVOKABLE void fitToPage(double viewportWidth, double viewportHeight);

    Q_INVOKABLE void toggleDarkMode();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void rotateCounterClockwise();
    Q_INVOKABLE void toggleToc();
    Q_INVOKABLE void toggleHelp();

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void nextSearchResult();
    Q_INVOKABLE void prevSearchResult();
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE QVariantList searchHighlights(int pageIndex) const;

    Q_INVOKABLE QString pageImageUrl(int pageIndex, double dpi = 150.0, int rot = -1, int dark = -1) const;
    Q_INVOKABLE QSizeF pageSize(int pageIndex) const;
    Q_INVOKABLE double pageAspectRatio(int pageIndex) const;

signals:
    void documentChanged();
    void currentPageChanged();
    void zoomChanged();
    void darkModeChanged();
    void rotationChanged();
    void fitModeChanged();
    void activeToolChanged();
    void annotationsChanged();
    void tocChanged();
    void tocVisibleChanged();
    void helpVisibleChanged();
    void searchChanged();
    void revisionChanged();
    void recentFilesChanged();
    void errorOccurred(const QString &message);
    void statusMessage(const QString &message);

private:
    void clearDocumentState();
    void addRecentFile(const QString &path);
    void loadRecents();
    void saveRecents();
    void loadAnnotations();
    void saveAnnotations();
    QString annotationFilePath() const;
    QString annotationFilePathFor(const QString &docPath) const;

    DocumentEngine *m_engine;
    PageImageProvider *m_provider;
    PortalFilePicker *m_picker;

    int m_currentPage = 0;
    double m_zoom = 1.0;
    bool m_darkMode = false;
    int m_rotation = 0;
    int m_fitMode = 0; // 0 = Free, 1 = FitWidth, 2 = FitPage
    int m_activeTool = 0; // 0 = Select/Pan, 1 = Highlight
    bool m_tocVisible = false;
    bool m_helpVisible = false;
    int m_revision = 0;

    QString m_lastError;
    QStringList m_recentFiles;
    QVariantList m_annotations;
    QVariantList m_toc;
    QString m_searchQuery;
    QList<int> m_searchResults;
    int m_searchIndex = -1;
};

#endif
