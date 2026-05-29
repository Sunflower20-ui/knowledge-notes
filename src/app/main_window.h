#pragma once

#include <QMainWindow>

class QSplitter;
class QTreeView;
class QTextEdit;
class QTabWidget;
class QLabel;
class QAction;
class QLineEdit;
class QToolBar;
class MarkdownHighlighter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onEditorTextChanged();
    void updatePreview();

private:
    void setupUi();
    void setupToolBar();
    void setupMenuBar();
    void setupStatusBar();
    void setupShortcuts();
    void applyTheme();

    // Formatting helpers
    void insertFormatting(const QString &before, const QString &after);
    void toggleFormatting(const QString &marker);

    // Toolbar
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_searchBox = nullptr;

    // Left panel: folder tree + tags
    QTreeView *m_folderTree = nullptr;

    // Center: editor + preview tabs
    QTabWidget *m_editorTabs = nullptr;
    QTextEdit *m_editor = nullptr;
    QTextEdit *m_preview = nullptr;
    MarkdownHighlighter *m_highlighter = nullptr;

    // Right panel: backlinks / outline
    QTabWidget *m_sidePanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    // Actions
    QAction *m_newNoteAction = nullptr;
    QAction *m_searchAction = nullptr;
    QAction *m_toggleSidebarAction = nullptr;

    // Preview update throttle
    bool m_previewDirty = false;
};
