#pragma once

#include <QMainWindow>

class QSplitter;
class QTreeView;
class QTextEdit;
class QTextBrowser;
class QTabWidget;
class QLabel;
class QAction;
class QLineEdit;
class QToolBar;
class QListWidget;
class QMenu;
class MarkdownHighlighter;
class NoteTreeModel;
class WikiCompleter;
class NoteRepository;
class FlashcardRepository;
class ReviewWidget;
class VersionRepository;
class VersionHistoryWidget;
struct NoteData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(NoteRepository *repo, FlashcardRepository *fcRepo = nullptr, VersionRepository *verRepo = nullptr, QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onEditorTextChanged();
    void updatePreview();
    void onCursorPositionChanged();
    void onWikiLinkSelected(const QString &title);
    void onTreeClicked(const QModelIndex &index);
    void onSearchTextChanged(const QString &text);

private:
    void setupUi();
    void setupToolBar();
    void setupMenuBar();
    void setupStatusBar();
    void setupShortcuts();
    void applyTheme();
    void setupTreeContextMenu();
    void updateReviewButton();
    void updateVersionHistory(qint64 noteId);
    void createFlashcard();

    void loadNoteIntoEditor(qint64 noteId);
    void saveCurrentNote();
    void syncWikiLinks(qint64 noteId, const QString &content);
    void refreshBacklinks();
    void navigateToNoteByTitle(const QString &title);

    void insertFormatting(const QString &before, const QString &after);
    void toggleFormatting(const QString &marker);

    NoteRepository *m_repo = nullptr;
    qint64 m_currentNoteId = -1;

    // Toolbar
    QToolBar *m_toolBar = nullptr;
    QLineEdit *m_searchBox = nullptr;

    // Left panel
    QTreeView *m_folderTree = nullptr;
    NoteTreeModel *m_treeModel = nullptr;
    QLabel *m_noteCountLabel = nullptr;
    QLabel *m_searchHeader = nullptr;

    // Center
    QTabWidget *m_editorTabs = nullptr;
    QTextEdit *m_editor = nullptr;
    QTextBrowser *m_preview = nullptr;
    MarkdownHighlighter *m_highlighter = nullptr;

    // Wiki autocomplete
    WikiCompleter *m_wikiCompleter = nullptr;
    FlashcardRepository *m_flashcardRepo = nullptr;

    // Right panel
    QTabWidget *m_sidePanel = nullptr;
    QListWidget *m_backlinksList = nullptr;
    ReviewWidget *m_reviewWidget = nullptr;
    VersionRepository *m_versionRepo = nullptr;
    VersionHistoryWidget *m_versionHistoryWidget = nullptr;
    QAction *m_reviewAction = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    // Actions
    QAction *m_newNoteAction = nullptr;
    QAction *m_searchAction = nullptr;
    QAction *m_toggleSidebarAction = nullptr;

    bool m_previewDirty = false;
    bool m_editorDirty = false;
    bool m_saving = false;
};
