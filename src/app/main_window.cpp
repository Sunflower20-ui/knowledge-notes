#include "main_window.h"
#include "markdown_highlighter.h"
#include "note_tree_model.h"
#include "wiki_completer.h"
#include "core/note_repository.h"
#include "core/flashcard.h"
#include "review_widget.h"
#include "core/version_repository.h"
#include "version_history_widget.h"
#include "core/export_service.h"
#include "core/import_service.h"

#include <QSplitter>
#include <QTreeView>
#include <QTextEdit>
#include <QTextBrowser>
#include <QTabWidget>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QLineEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QVBoxLayout>
#include <QWidget>
#include <QApplication>
#include <QAction>
#include <QKeySequence>
#include <QShortcut>
#include <QTextDocument>
#include <QTextCursor>
#include <QTimer>
#include <QRegularExpression>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QScrollBar>
#include <QDebug>
#include <QDateTime>

// ============================================================================
// Construction
// ============================================================================

MainWindow::MainWindow(NoteRepository *repo, FlashcardRepository *fcRepo, VersionRepository *verRepo, QWidget *parent)
    : QMainWindow(parent), m_repo(repo), m_flashcardRepo(fcRepo), m_versionRepo(verRepo)
{
    m_exportService = new ExportService(m_repo, this);
    m_importService = new ImportService(m_repo, this);
    applyTheme();
    setupUi();
    setupToolBar();
    setupMenuBar();
    setupStatusBar();
    setupShortcuts();
}

// ============================================================================
// Theme & Style
// ============================================================================

void MainWindow::applyTheme()
{
    setWindowTitle(QStringLiteral("知识笔记"));
    resize(1280, 800);
    setMinimumSize(800, 500);

    setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.svg")));

    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString style = QString::fromUtf8(styleFile.readAll());
        qApp->setStyleSheet(style);
        styleFile.close();
    }

    QApplication::setApplicationName(QStringLiteral("KnowledgeNotes"));
    QApplication::setApplicationDisplayName(QStringLiteral("KnowledgeNotes"));
    QApplication::setOrganizationName(QStringLiteral("KnowledgeNotes"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
}

// ============================================================================
// Note loading / saving
// ============================================================================

void MainWindow::loadNoteIntoEditor(qint64 noteId)
{
    if (noteId < 0) return;

    NoteData note = m_repo->getNote(noteId);
    if (note.id != noteId) return;

    m_currentNoteId = noteId;
    m_saving = true;
    m_editor->setPlainText(note.content);
    m_saving = false;
    m_editorDirty = false;

    m_previewDirty = true;
    updatePreview();
    refreshBacklinks();
    updateVersionHistory(noteId);

    statusBar()->showMessage(
        QStringLiteral("笔记 #%1 — %2").arg(noteId).arg(note.title));
}

void MainWindow::saveCurrentNote()
{
    if (m_currentNoteId <= 0) return;
    if (m_saving) return;

    QString content = m_editor->toPlainText();
    // Title = first non-empty line, trimmed
    QString title;
    for (const QString &line : content.split('\n')) {
        QString t = line.trimmed();
        // Strip leading # markers
        while (t.startsWith('#')) t = t.mid(1).trimmed();
        if (!t.isEmpty()) { title = t; break; }
    }
    if (title.isEmpty()) title = QStringLiteral("未命名");

    m_repo->updateNote(m_currentNoteId, title, content);
    if (m_versionRepo && m_editorDirty)
        m_versionRepo->saveVersion(m_currentNoteId, title, content);
    m_editorDirty = false;

    // Parse and sync [[wiki links]]
    syncWikiLinks(m_currentNoteId, content);

    // Reset to normal mode so the updated note appears in the tree
    if (!m_treeModel->currentFilter().isEmpty() || m_treeModel->currentTagFilter() > 0) {
        m_treeModel->setSearchFilter(QString());
    } else {
        m_treeModel->refresh();
    }
}

void MainWindow::syncWikiLinks(qint64 noteId, const QString &content)
{
    // Extract all [[...]] patterns
    static QRegularExpression wikiRegex(QStringLiteral("\\[\\[([^\\]\\|]+)(?:\\|[^\\]]+)?\\]\\]"));
    QVector<QPair<QString, qint64>> links;
    QSet<QString> seen;   // deduplicate

    QRegularExpressionMatchIterator it = wikiRegex.globalMatch(content);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString targetTitle = match.captured(1).trimmed();
        if (targetTitle.isEmpty() || seen.contains(targetTitle)) continue;
        seen.insert(targetTitle);

        // Try to resolve the title to a note ID
        QVector<NoteData> found = m_repo->searchNotes(targetTitle, 1);
        qint64 targetId = -1;
        if (!found.isEmpty() && found[0].title.compare(targetTitle, Qt::CaseInsensitive) == 0)
            targetId = found[0].id;

        links.append({targetTitle, targetId});
    }

    m_repo->syncLinks(noteId, links);
}

// ============================================================================
// Backlinks panel
// ============================================================================

void MainWindow::refreshBacklinks()
{
    m_backlinksList->clear();

    if (m_currentNoteId <= 0) return;

    QVector<LinkData> blinks = m_repo->backlinks(m_currentNoteId);
    if (blinks.isEmpty()) {
        auto *item = new QListWidgetItem(QStringLiteral("（暂无反向链接）"));
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor("#6c7086"));
        m_backlinksList->addItem(item);
        return;
    }

    for (const LinkData &link : blinks) {
        NoteData sourceNote = m_repo->getNote(link.sourceNoteId);
        QString display = sourceNote.title.isEmpty()
            ? QStringLiteral("未命名 #%1").arg(link.sourceNoteId)
            : sourceNote.title;
        auto *item = new QListWidgetItem(
            QIcon(QStringLiteral(":/icons/link.svg")), display);
        item->setData(Qt::UserRole, link.sourceNoteId);
        m_backlinksList->addItem(item);
    }
}

void MainWindow::navigateToNoteByTitle(const QString &title)
{
    QVector<NoteData> found = m_repo->searchNotes(title, 1);
    if (!found.isEmpty() && found[0].title.compare(title, Qt::CaseInsensitive) == 0) {
        loadNoteIntoEditor(found[0].id);
        m_editorTabs->setCurrentIndex(0); // switch to editor
    } else {
        // Create a new note with this title as a placeholder
        qint64 id = m_repo->createNote(title,
            QStringLiteral("# %1\n\n").arg(title));
        m_treeModel->refresh();
        if (id > 0) loadNoteIntoEditor(id);
    }
}

// ============================================================================
// Wiki link autocomplete
// ============================================================================

// ============================================================================
// Search
// ============================================================================

void MainWindow::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_treeModel->setSearchFilter(QString());
        m_folderTree->expandAll();
        // Update count for normal mode
        int count = m_treeModel->rowCount(m_treeModel->index(0, 0, QModelIndex()));
        if (count > 0) {
            auto idx = m_treeModel->index(0, 0, QModelIndex());
            if (m_treeModel->nodeTypeForIndex(idx) == NoteTreeModel::NoteFolder) {
                m_noteCountLabel->setText(QStringLiteral("  %1 篇笔记").arg(m_treeModel->rowCount(idx)));
            }
        }
    } else {
        m_treeModel->setSearchFilter(text);
        m_folderTree->expandAll();
        m_noteCountLabel->setText(QStringLiteral("  搜索中..."));
    }
}

// ============================================================================
// Wiki link autocomplete
// ============================================================================

void MainWindow::onCursorPositionChanged()
{
    QTextCursor cursor = m_editor->textCursor();
    int pos = cursor.position();

    // Get the text from start of current block to cursor
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    QString textBeforeCursor = cursor.selectedText();

    // Look for the last [[ that hasn't been closed with ]]
    int lastOpen = textBeforeCursor.lastIndexOf(QStringLiteral("[["));
    if (lastOpen < 0) {
        m_wikiCompleter->hide();
        return;
    }

    // Check there's no closing ]] between [[ and cursor
    QString afterOpen = textBeforeCursor.mid(lastOpen + 2);
    if (afterOpen.contains(QStringLiteral("]]"))) {
        m_wikiCompleter->hide();
        return;
    }

    // Get the partial text after [[
    QString partial = afterOpen;

    // Show autocomplete
    m_wikiCompleter->updateSuggestions(partial);

    if (m_wikiCompleter->isVisible()) {
        // Position below cursor
        QRect cr = m_editor->cursorRect();
        QPoint globalPos = m_editor->mapToGlobal(cr.bottomLeft());
        m_wikiCompleter->showAt(globalPos);
    }
}

void MainWindow::onWikiLinkSelected(const QString &title)
{
    QTextCursor cursor = m_editor->textCursor();
    int cursorPos = cursor.position();

    // Find the text from start of block to cursor
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    QString textBefore = cursor.selectedText();
    int lastOpen = textBefore.lastIndexOf(QStringLiteral("[["));

    if (lastOpen < 0) return;

    // Compute absolute position of the opening [[
    int blockStart = cursorPos - textBefore.length();
    int openPos = blockStart + lastOpen;

    // Select from [[ to current cursor position and replace
    QTextCursor replaceCursor(m_editor->document());
    replaceCursor.setPosition(openPos);
    replaceCursor.setPosition(cursorPos, QTextCursor::KeepAnchor);
    replaceCursor.insertText(QStringLiteral("[[%1]]").arg(title));

    m_editor->setTextCursor(replaceCursor);
    m_editor->setFocus();
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

void MainWindow::setupShortcuts()
{
    // Ctrl+B → Bold
    auto *boldShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), m_editor);
    connect(boldShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("**"));
    });

    // Ctrl+I → Italic
    auto *italicShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+I")), m_editor);
    connect(italicShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("*"));
    });

    // Ctrl+Shift+S → Strikethrough
    auto *strikeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), m_editor);
    connect(strikeShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("~~"));
    });

    // Ctrl+K → Inline code
    auto *codeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), m_editor);
    connect(codeShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("`"));
    });

    // Tab → 4 spaces  (but allow completer to handle it)
    auto *tabShortcut = new QShortcut(QKeySequence(QStringLiteral("Tab")), m_editor);
    connect(tabShortcut, &QShortcut::activated, this, [this] {
        if (m_wikiCompleter->isVisible()) {
            m_wikiCompleter->selectNext();
        } else {
            m_editor->insertPlainText(QStringLiteral("    "));
        }
    });

    // Shift+Tab → outdent
    auto *shiftTabShortcut = new QShortcut(QKeySequence(QStringLiteral("Shift+Tab")), m_editor);
    connect(shiftTabShortcut, &QShortcut::activated, this, [this] {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();
        if (line.startsWith(QStringLiteral("    "))) {
            cursor.removeSelectedText();
            cursor.insertText(line.mid(4));
        }
    });

    // Ctrl+L → insert link
    auto *linkShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), m_editor);
    connect(linkShortcut, &QShortcut::activated, this, [this] {
        insertFormatting(QStringLiteral("["), QStringLiteral("](url)"));
    });

    // Ctrl+S → save
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] {
        m_searchBox->clear();
        m_treeModel->setSearchFilter(QString());
        saveCurrentNote();
        statusBar()->showMessage(tr("已保存"), 2000);
    });

    // Ctrl+Shift+P → toggle preview
    auto *previewShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this);
    connect(previewShortcut, &QShortcut::activated, this, [this] {
        m_editorTabs->setCurrentIndex(m_editorTabs->currentIndex() == 0 ? 1 : 0);
    });
    updateReviewButton();
}

// ============================================================================
// Formatting helpers
// ============================================================================

void MainWindow::insertFormatting(const QString &before, const QString &after)
{
    QTextCursor cursor = m_editor->textCursor();
    QString selected = cursor.selectedText();

    if (selected.isEmpty()) {
        cursor.insertText(before + after);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, after.length());
    } else {
        cursor.insertText(before + selected + after);
    }
    m_editor->setTextCursor(cursor);
}

void MainWindow::toggleFormatting(const QString &marker)
{
    QTextCursor cursor = m_editor->textCursor();
    QString selected = cursor.selectedText();
    int markerLen = marker.length();

    if (selected.isEmpty()) {
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();
        if (!word.isEmpty()) {
            cursor.insertText(marker + word + marker);
            return;
        }
        cursor.insertText(marker + marker);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, markerLen);
        m_editor->setTextCursor(cursor);
        return;
    }

    if (selected.startsWith(marker) && selected.endsWith(marker) && selected.length() >= markerLen * 2) {
        cursor.insertText(selected.mid(markerLen, selected.length() - markerLen * 2));
    } else {
        cursor.insertText(marker + selected + marker);
    }
}

// ============================================================================
// Preview (with [[wiki link]] support)
// ============================================================================

void MainWindow::onEditorTextChanged()
{
    if (m_saving) return;
    m_editorDirty = true;
    if (!m_previewDirty) {
        m_previewDirty = true;
        QTimer::singleShot(300, this, &MainWindow::updatePreview);
    }
}

void MainWindow::updatePreview()
{
    m_previewDirty = false;

    QString markdown = m_editor->toPlainText();

    // Convert [[wiki links]] to Markdown links for preview rendering
    // [[Title]] → [Title](wiki://Title)
    // [[Title|Alias]] → [Alias](wiki://Title)
    static QRegularExpression wikiRegex(QStringLiteral("\\[\\[([^\\]\\|]+)(?:\\|([^\\]]+))?\\]\\]"));
    QString result;
    int lastEnd = 0;
    QRegularExpressionMatchIterator wit = wikiRegex.globalMatch(markdown);
    while (wit.hasNext()) {
        QRegularExpressionMatch m = wit.next();
        QString title = m.captured(1).trimmed();
        QString alias = m.captured(2).trimmed();
        if (alias.isEmpty()) alias = title;
        // Copy text before the match
        result += markdown.mid(lastEnd, m.capturedStart() - lastEnd);
        // Add converted link
        result += QStringLiteral("[%1](wiki://%2)").arg(alias, title);
        lastEnd = m.capturedEnd();
    }
    result += markdown.mid(lastEnd);
    markdown = result;

    QTextDocument doc;
    doc.setMarkdown(markdown);

    doc.setDefaultStyleSheet(QStringLiteral(
        "body { color: #cdd6f4; font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif; "
        "font-size: 15px; line-height: 1.7; }"
        "h1 { color: #cba6f7; font-size: 24px; border-bottom: 2px solid #45475a; padding-bottom: 8px; }"
        "h2 { color: #cba6f7; font-size: 20px; border-bottom: 1px solid #45475a; padding-bottom: 6px; }"
        "h3 { color: #cba6f7; font-size: 17px; }"
        "h4, h5, h6 { color: #cba6f7; }"
        "code { background: #313244; color: #a6e3a1; padding: 2px 6px; border-radius: 4px; "
        "font-family: 'Consolas', 'Cascadia Code', monospace; font-size: 13px; }"
        "pre { background: #181825; padding: 16px; border-radius: 8px; border: 1px solid #313244; }"
        "pre code { background: transparent; padding: 0; }"
        "blockquote { border-left: 3px solid #cba6f7; padding-left: 16px; color: #a6adc8; margin: 8px 0; }"
        "a { color: #89b4fa; text-decoration: none; }"
        "a[href^='wiki://'] { color: #a6e3a1; border-bottom: 1px dashed #a6e3a1; }"
        "table { border-collapse: collapse; width: 100%; }"
        "th, td { border: 1px solid #45475a; padding: 8px 12px; text-align: left; }"
        "th { background: #313244; }"
        "hr { border: none; border-top: 1px solid #45475a; margin: 16px 0; }"
        "img { max-width: 100%; border-radius: 8px; }"
        "ul, ol { padding-left: 24px; }"
        "li { margin: 4px 0; }"
    ));

    m_preview->setHtml(doc.toHtml());
}

// ============================================================================
// Toolbar
// ============================================================================

void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar(tr("Main"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setObjectName(QStringLiteral("mainToolBar"));
    m_toolBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #181825; border-bottom: 1px solid #313244; "
        "padding: 4px 8px; spacing: 4px; }"
        "QToolButton { padding: 6px 8px; border-radius: 6px; color: #cdd6f4; font-size: 12px; }"
        "QToolButton:hover { background: #313244; }"
        "QToolButton:pressed { background: #45475a; }"
        "QLineEdit { background: #313244; color: #cdd6f4; border: 1px solid #45475a; "
        "border-radius: 8px; padding: 5px 12px; font-size: 13px; min-width: 200px; }"
        "QLineEdit:focus { border-color: #cba6f7; }"));

    QAction *newNoteBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/new_note.svg")), tr("新建"));
    newNoteBtn->setToolTip(tr("新建笔记 (Ctrl+N)"));
    connect(newNoteBtn, &QAction::triggered, this, [this] {
        saveCurrentNote();
        m_treeModel->setSearchFilter(QString());
        m_searchBox->clear();
        qint64 id = m_repo->createNote(QStringLiteral("未命名"), QStringLiteral(""));
        m_treeModel->refresh();
        if (id > 0) loadNoteIntoEditor(id);
    });

    m_toolBar->addSeparator();

    QAction *boldBtn = m_toolBar->addAction(QIcon(), QStringLiteral("B"));
    QFont boldFont; boldFont.setBold(true); boldBtn->setFont(boldFont);
    connect(boldBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("**")); });

    QAction *italicBtn = m_toolBar->addAction(QIcon(), QStringLiteral("I"));
    QFont italicFont; italicFont.setItalic(true); italicBtn->setFont(italicFont);
    connect(italicBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("*")); });

    QAction *strikeBtn = m_toolBar->addAction(QIcon(), QStringLiteral("S"));
    QFont strikeFont; strikeFont.setStrikeOut(true); strikeBtn->setFont(strikeFont);
    connect(strikeBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("~~")); });

    QAction *codeBtn = m_toolBar->addAction(QIcon(), QStringLiteral("<>"));
    connect(codeBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("`")); });

    m_toolBar->addSeparator();

    QAction *linkBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/link.svg")), tr("链接"));
    connect(linkBtn, &QAction::triggered, this, [this] {
        insertFormatting(QStringLiteral("["), QStringLiteral("](url)"));
    });

    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText(tr("搜索笔记..."));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setMaximumWidth(240);
    m_toolBar->addWidget(m_searchBox);
    connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);

    QAction *searchBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/search.svg")), tr("搜索"));
    connect(searchBtn, &QAction::triggered, this, [this] {
        m_searchBox->setFocus(); m_searchBox->selectAll();
    });

    m_toolBar->addSeparator();


    // ========== Review button (Day 8: flashcard spaced repetition) ==========
    if (m_flashcardRepo) {
        m_toolBar->addSeparator();
        m_reviewAction = m_toolBar->addAction(
            QIcon(QStringLiteral(":/icons/search.svg")), tr("复习"));
        m_reviewAction->setToolTip(tr("闪卡复习"));
        connect(m_reviewAction, &QAction::triggered, this, [this] {
            if (m_reviewWidget) {
                m_sidePanel->setVisible(true);
                m_sidePanel->setCurrentWidget(m_reviewWidget);
                m_reviewWidget->startSession();
            }
        });
    }

    // Flashcard creation button
    if (m_flashcardRepo) {
        QAction *flashcardBtn = m_toolBar->addAction(
            QIcon(QStringLiteral(":/icons/new_note.svg")), tr("闪卡"));
        flashcardBtn->setToolTip(tr("从选中文本创建闪卡"));
        connect(flashcardBtn, &QAction::triggered, this, &MainWindow::createFlashcard);
    }

    m_toggleSidebarAction = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/folder.svg")), tr("侧栏"));
    m_toggleSidebarAction->setCheckable(true);
    m_toggleSidebarAction->setChecked(true);
    connect(m_toggleSidebarAction, &QAction::triggered, this, [this] {
        m_folderTree->parentWidget()->setVisible(!m_folderTree->parentWidget()->isVisible());
    });

    QAction *toggleRightBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/outline.svg")), tr("大纲"));
    toggleRightBtn->setCheckable(true);
    toggleRightBtn->setChecked(true);
    connect(toggleRightBtn, &QAction::triggered, this, [this] {
        m_sidePanel->setVisible(!m_sidePanel->isVisible());
    });
    updateReviewButton();
}

// ============================================================================
// UI Layout
// ============================================================================

void MainWindow::setupUi()
{
    // ========== Left panel: note tree ==========
    QWidget *leftPanel = new QWidget;
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setStyleSheet(QStringLiteral(
        "QWidget#leftPanel { background: #181825; border-right: 1px solid #313244; }"));

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    auto *headerLabel = new QLabel(tr("  笔记"));
    headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #6c7086; font-size: 10px; font-weight: 700; "
        "letter-spacing: 1px; padding: 12px 12px 6px 12px; background: transparent; }"));

    m_treeModel = new NoteTreeModel(m_repo, this);
    m_folderTree = new QTreeView;
    m_folderTree->setModel(m_treeModel);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setAnimated(true);
    m_folderTree->setIndentation(16);
    m_folderTree->setObjectName(QStringLiteral("folderTree"));
    m_folderTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_folderTree->setExpandsOnDoubleClick(false);
    m_folderTree->expand(m_treeModel->index(0, 0, QModelIndex()));
    m_folderTree->expand(m_treeModel->index(1, 0, QModelIndex()));

        connect(m_folderTree, &QTreeView::clicked, this, &MainWindow::onTreeClicked);

    // Right-click context menu for notes
    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_folderTree, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex idx = m_folderTree->indexAt(pos);
        QString folderName = m_treeModel->folderNameForIndex(idx);
        qint64 noteId = m_treeModel->noteIdForIndex(idx);

        // Folder context menu (skip virtual folders)
        if (!folderName.isEmpty() && folderName != QStringLiteral("(未分类)")) {
            QMenu menu;
            menu.addAction(tr("重命名文件夹"), this, [this, folderName] {
                bool ok;
                QString newName = QInputDialog::getText(this, tr("重命名文件夹"),
                    tr("新名称:"), QLineEdit::Normal, folderName, &ok);
                if (ok && !newName.trimmed().isEmpty() && newName.trimmed() != folderName) {
                    m_repo->renameFolder(folderName, newName.trimmed());
                    m_treeModel->refresh();
                }
            });
            menu.addSeparator();
            QAction *delFolderAct = menu.addAction(tr("删除文件夹"));
            connect(delFolderAct, &QAction::triggered, this, [this, folderName] {
                auto ret = QMessageBox::question(this, tr("删除文件夹"),
                    tr("确定删除文件夹「%1」及其中所有笔记吗？\n此操作不可撤销。").arg(folderName),
                    QMessageBox::Yes | QMessageBox::No);
                if (ret == QMessageBox::Yes) {
                    int deleted = m_repo->deleteFolder(folderName);
                    m_treeModel->refresh();
                    if (m_currentNoteId > 0 && m_repo->getNote(m_currentNoteId).isDeleted) {
                        m_editor->clear();
                        m_currentNoteId = -1;
                        refreshBacklinks();
                    }
                }
            });
            menu.exec(m_folderTree->viewport()->mapToGlobal(pos));
            return;
        }

        // Note context menu
        if (noteId <= 0) return;

        QMenu menu;
        NoteData note = m_repo->getNote(noteId);

        // Add tag submenu
        QMenu *tagMenu = menu.addMenu(tr("添加标签"));
        QVector<TagData> allTags = m_repo->listTags();
        QVector<TagData> noteTags = m_repo->tagsForNote(noteId);
        QSet<qint64> noteTagIds;
        for (const auto &t : noteTags) noteTagIds.insert(t.id);

        for (const TagData &tag : allTags) {
            QAction *act = tagMenu->addAction(tag.name);
            act->setCheckable(true);
            act->setChecked(noteTagIds.contains(tag.id));
            connect(act, &QAction::triggered, this, [this, noteId, tagId = tag.id] {
                m_repo->addTagToNote(noteId, tagId);
                m_treeModel->refresh();
            });
        }
        tagMenu->addSeparator();
        QAction *newTagAct = tagMenu->addAction(tr("+ 新建标签..."));
        connect(newTagAct, &QAction::triggered, this, [this, noteId] {
            bool ok;
            QString name = QInputDialog::getText(this, tr("新建标签"), tr("标签名:"), QLineEdit::Normal, QString(), &ok);
            if (ok && !name.trimmed().isEmpty()) {
                qint64 tagId = m_repo->createTag(name.trimmed());
                if (tagId > 0) {
                    m_repo->addTagToNote(noteId, tagId);
                    m_treeModel->refresh();
                }
            }
        });

        // Move to folder submenu
        QMenu *folderMenu = menu.addMenu(tr("移动到文件夹"));
        QStringList folders = m_repo->listFolders();
        for (const QString &f : folders) {
            QAction *act = folderMenu->addAction(f);
            connect(act, &QAction::triggered, this, [this, noteId, f] {
                m_repo->setNoteFolder(noteId, f);
                m_treeModel->refresh();
            });
        }
        folderMenu->addSeparator();
        QAction *newFolderAct = folderMenu->addAction(tr("+ 新建文件夹..."));
        connect(newFolderAct, &QAction::triggered, this, [this, noteId] {
            bool ok;
            QString name = QInputDialog::getText(this, tr("新建文件夹"), tr("文件夹名:"), QLineEdit::Normal, QString(), &ok);
            if (ok && !name.trimmed().isEmpty()) {
                m_repo->setNoteFolder(noteId, name.trimmed());
                m_treeModel->refresh();
            }
        });

                // Remove tag submenu
        if (!noteTags.isEmpty()) {
            QMenu *rmMenu = menu.addMenu(tr("移除标签"));
            for (const TagData &tag : noteTags) {
                QAction *act = rmMenu->addAction(tag.name);
                connect(act, &QAction::triggered, this, [this, noteId, tagId = tag.id] {
                    m_repo->removeTagFromNote(noteId, tagId);
                    m_treeModel->refresh();
                });
            }
        }

        menu.addSeparator();

        // Delete note
        QAction *delAct = menu.addAction(tr("删除笔记"));
        connect(delAct, &QAction::triggered, this, [this, noteId] {
            auto ret = QMessageBox::question(this, tr("删除笔记"),
                tr("确定删除「%1」吗？").arg(m_repo->getNote(noteId).title),
                QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                m_repo->deleteNote(noteId);
                m_treeModel->refresh();
                if (m_currentNoteId == noteId) {
                    m_editor->clear();
                    m_currentNoteId = -1;
                    refreshBacklinks();
                }
            }
        });

        menu.exec(m_folderTree->viewport()->mapToGlobal(pos));
    });

    m_noteCountLabel = new QLabel;
    m_noteCountLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #585b70; font-size: 11px; padding: 8px 12px; "
        "background: transparent; border-top: 1px solid #313244; }"));
    auto updateCount = [this] {
        QModelIndex root = m_treeModel->index(0, 0, QModelIndex());
        int count = 0;
        if (root.isValid()) {
            NoteTreeModel::NodeType t = m_treeModel->nodeTypeForIndex(root);
            if (t == NoteTreeModel::NoteFolder || t == NoteTreeModel::SearchFolder || t == NoteTreeModel::TagLabel)
                count = m_treeModel->rowCount(root);
        }
        m_noteCountLabel->setText(QStringLiteral("  %1 篇笔记").arg(count));
    };
    updateCount();
    connect(m_treeModel, &QAbstractItemModel::modelReset, this, updateCount);

    leftLayout->addWidget(headerLabel);
    leftLayout->addWidget(m_folderTree);
    leftLayout->addWidget(m_noteCountLabel);

    // ========== Center: editor + preview ==========
    m_editorTabs = new QTabWidget;
    m_editorTabs->setObjectName(QStringLiteral("editorTabs"));

    m_editor = new QTextEdit;
    m_editor->setPlaceholderText(tr("选择一篇笔记，或新建笔记开始写作..."));
    m_editor->setTabStopDistance(32);
    m_editor->setObjectName(QStringLiteral("editor"));
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editorTabs->addTab(m_editor,
        QIcon(QStringLiteral(":/icons/edit.svg")),
        QStringLiteral(" 编辑 "));

    m_highlighter = new MarkdownHighlighter(m_editor->document());

    m_preview = new QTextBrowser;
    m_preview->setReadOnly(true);
    m_preview->setOpenExternalLinks(false);
    m_preview->setPlaceholderText(tr("实时预览将在这里显示..."));
    m_preview->setObjectName(QStringLiteral("preview"));
    m_editorTabs->addTab(m_preview,
        QIcon(QStringLiteral(":/icons/preview.svg")),
        QStringLiteral(" 预览 "));

    connect(m_editor, &QTextEdit::textChanged, this, &MainWindow::onEditorTextChanged);
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::onCursorPositionChanged);
    updatePreview();

    // ========== Wiki autocomplete popup ==========
    m_wikiCompleter = new WikiCompleter(m_repo, this);
    connect(m_wikiCompleter, &WikiCompleter::linkSelected, this, &MainWindow::onWikiLinkSelected);

    // ========== Preview: click on wiki:// links to navigate ==========
    connect(m_preview, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.scheme() == QStringLiteral("wiki")) {
            navigateToNoteByTitle(url.host() + url.path().mid(1)); // wiki://Title → Title
        }
    });

    // ========== Right: side panel ==========
    m_sidePanel = new QTabWidget;
    m_sidePanel->setMaximumWidth(300);
    m_sidePanel->setMinimumWidth(180);
    m_sidePanel->setObjectName(QStringLiteral("sidePanel"));

    // Backlinks tab — now functional
    m_backlinksList = new QListWidget;
    m_backlinksList->setObjectName(QStringLiteral("backlinksList"));
    m_backlinksList->setStyleSheet(QStringLiteral(
        "QListWidget { background: #181825; color: #cdd6f4; border: none; "
        "font-size: 13px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 4px; }"
        "QListWidget::item:hover { background: #313244; }"
        "QListWidget::item:selected { background: #45475a; }"
    ));
    refreshBacklinks();
    // Click a backlink to navigate
    connect(m_backlinksList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        qint64 noteId = item->data(Qt::UserRole).toLongLong();
        if (noteId > 0) loadNoteIntoEditor(noteId);
    });

    m_sidePanel->addTab(m_backlinksList,
        QIcon(QStringLiteral(":/icons/backlinks.svg")),
        QStringLiteral(" 反向链接 "));


    // ========== Review tab (Day 8: flashcard spaced repetition) ==========
    if (m_flashcardRepo) {
        m_reviewWidget = new ReviewWidget(m_flashcardRepo);
        connect(m_reviewWidget, &ReviewWidget::finished, this, [this] {
            updateReviewButton();
        });
        m_sidePanel->addTab(m_reviewWidget,
            QIcon(QStringLiteral(":/icons/search.svg")),
            QStringLiteral(" 复习 "));
        updateReviewButton();
    }
    // ========== Version history tab (Day 9: note versioning) ==========
    if (m_versionRepo) {
        m_versionHistoryWidget = new VersionHistoryWidget(m_versionRepo);
        connect(m_versionHistoryWidget, &VersionHistoryWidget::versionRestored, this, [this](qint64 noteId) {
            loadNoteIntoEditor(noteId);
            m_treeModel->refresh();
        });
        m_sidePanel->addTab(m_versionHistoryWidget,
            QIcon(QStringLiteral(":/icons/edit.svg")),
            QStringLiteral(" 历史 "));
    }


    auto *outlinePlaceholder = new QLabel(tr("大纲 / 目录\n将在这里显示..."));
    outlinePlaceholder->setAlignment(Qt::AlignCenter);
    outlinePlaceholder->setWordWrap(true);
    outlinePlaceholder->setStyleSheet(QStringLiteral(
        "QLabel { color: #6c7086; font-size: 13px; padding: 24px; background: transparent; }"));
    m_sidePanel->addTab(outlinePlaceholder,
        QIcon(QStringLiteral(":/icons/outline.svg")),
        QStringLiteral(" 大纲 "));

    // ========== Splitter layout ==========
    m_rightSplitter = new QSplitter(Qt::Horizontal);
    m_rightSplitter->addWidget(m_editorTabs);
    m_rightSplitter->addWidget(m_sidePanel);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 1);

    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->addWidget(leftPanel);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({260, 1020});

    setCentralWidget(m_mainSplitter);
}

// ============================================================================
// Menu Bar
// ============================================================================

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    m_newNoteAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/icons/new_note.svg")), tr("新建笔记(&N)"));
    m_newNoteAction->setShortcut(QKeySequence::New);
    connect(m_newNoteAction, &QAction::triggered, this, [this] {
        saveCurrentNote();
        m_treeModel->setSearchFilter(QString());
        m_searchBox->clear();
        qint64 id = m_repo->createNote(QStringLiteral("未命名"), QStringLiteral(""));
        m_treeModel->refresh();
        if (id > 0) loadNoteIntoEditor(id);
    });

    // --- Export / Import ---
    QMenu *exportMenu = fileMenu->addMenu(tr("\u5bfc\u51fa(&E)"));
    exportMenu->addAction(tr("\u5bfc\u51fa\u5f53\u524d\u7b14\u8bb0..."), this, &MainWindow::onExportCurrentNote);
    exportMenu->addAction(tr("\u5bfc\u51fa\u5168\u90e8\u7b14\u8bb0..."), this, &MainWindow::onExportAllNotes);
    QMenu *importMenu = fileMenu->addMenu(tr("\u5bfc\u5165(&I)"));
    importMenu->addAction(tr("\u5bfc\u5165 Markdown \u6587\u4ef6..."), this, &MainWindow::onImportMarkdownFiles);
    importMenu->addAction(tr("\u5bfc\u5165\u6587\u4ef6\u5939..."), this, &MainWindow::onImportMarkdownDirectory);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), QKeySequence::Quit, this, &QWidget::close);

    // --- Edit ---
    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    m_searchAction = editMenu->addAction(
        QIcon(QStringLiteral(":/icons/search.svg")), tr("搜索(&S)..."));
    m_searchAction->setShortcut(QKeySequence::Find);
    connect(m_searchAction, &QAction::triggered, this, [this] {
        m_searchBox->setFocus(); m_searchBox->selectAll();
    });
    editMenu->addSeparator();
    editMenu->addAction(tr("加粗(&B)"), QKeySequence(QStringLiteral("Ctrl+B")),
        this, [this] { toggleFormatting(QStringLiteral("**")); });
    editMenu->addAction(tr("斜体(&I)"), QKeySequence(QStringLiteral("Ctrl+I")),
        this, [this] { toggleFormatting(QStringLiteral("*")); });
    editMenu->addAction(tr("删除线(&S)"), QKeySequence(QStringLiteral("Ctrl+Shift+S")),
        this, [this] { toggleFormatting(QStringLiteral("~~")); });
    editMenu->addAction(tr("行内代码(&C)"), QKeySequence(QStringLiteral("Ctrl+K")),
        this, [this] { toggleFormatting(QStringLiteral("`")); });
    editMenu->addAction(tr("插入链接(&L)"), QKeySequence(QStringLiteral("Ctrl+L")),
        this, [this] { insertFormatting(QStringLiteral("["), QStringLiteral("](url)")); });

    // --- View ---
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    viewMenu->addAction(
        QIcon(QStringLiteral(":/icons/folder.svg")),
        tr("切换侧栏(&S)"), QKeySequence(QStringLiteral("Ctrl+\\")), this, [this] {
            m_folderTree->parentWidget()->setVisible(!m_folderTree->parentWidget()->isVisible());
        });
    viewMenu->addAction(tr("切换预览(&P)"), QKeySequence(QStringLiteral("Ctrl+Shift+P")),
        this, [this] { m_editorTabs->setCurrentIndex(m_editorTabs->currentIndex() == 0 ? 1 : 0); });


    viewMenu->addSeparator();
    viewMenu->addAction(tr("闪卡复习(&R)"), QKeySequence(QStringLiteral("Ctrl+R")),
        this, [this] {
            if (m_reviewWidget) {
                m_sidePanel->setVisible(true);
                m_sidePanel->setCurrentWidget(m_reviewWidget);
                m_reviewWidget->startSession();
            }
        });

    // --- Help ---
    // --- Tools ---
    QMenu *toolsMenu = menuBar()->addMenu(tr("\u5de5\u5177(&T)"));
    toolsMenu->addAction(tr("\u5907\u4efd\u6570\u636e\u5e93(&B)..."), this, &MainWindow::onBackupDatabase);
    toolsMenu->addAction(tr("\u6062\u590d\u6570\u636e\u5e93(&R)..."), this, &MainWindow::onRestoreDatabase);
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("关于(&A)"), this, [] {});
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("就绪 — Ctrl+N 新建  Ctrl+S 保存  [[ 插入链接"));
}


// ============================================================================
// Review button
// ============================================================================

void MainWindow::updateReviewButton()
{
    if (!m_reviewWidget || !m_sidePanel) return;

    int due = m_reviewWidget->dueCount();
    QString text = due > 0
        ? QStringLiteral(" 复习 (%1) ").arg(due)
        : QStringLiteral(" 复习 ");

    int reviewIdx = m_sidePanel->indexOf(m_reviewWidget);
    if (reviewIdx >= 0)
        m_sidePanel->setTabText(reviewIdx, text);

    if (m_reviewAction) {
        m_reviewAction->setToolTip(due > 0
            ? tr("闪卡复习 — %1 张待复习").arg(due)
            : tr("闪卡复习 — 暂无待复习卡片"));
    }
}

// ============================================================================
// Tree click → load note
// ============================================================================

void MainWindow::onTreeClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // Tag click → filter by tag
    qint64 tagId = m_treeModel->tagIdForIndex(index);
    if (tagId > 0) {
        saveCurrentNote();
        m_treeModel->setTagFilter(tagId);
        m_folderTree->expandAll();
        m_searchBox->clear();
        m_noteCountLabel->setText(QStringLiteral("  按标签筛选"));
        return;
    }

    // Note click → save current then load
    qint64 noteId = m_treeModel->noteIdForIndex(index);
    if (noteId > 0) {
        saveCurrentNote();
        loadNoteIntoEditor(noteId);
        return;
    }

    // FolderItem click — toggle expand/collapse
    if (m_treeModel->folderNameForIndex(index).isEmpty() == false) {
        if (m_folderTree->isExpanded(index))
            m_folderTree->collapse(index);
        else
            m_folderTree->expand(index);
        return;
    }

    // Folder click// Folder click → restore normal
    NoteTreeModel::NodeType type = m_treeModel->nodeTypeForIndex(index);
    if (type == NoteTreeModel::NoteFolder || type == NoteTreeModel::SearchFolder || type == NoteTreeModel::TagLabel) {
        saveCurrentNote();
        m_treeModel->setSearchFilter(QString());
        m_folderTree->expandAll();
    }
}

// ============================================================================
// Version history
// ============================================================================

void MainWindow::updateVersionHistory(qint64 noteId)
{
    if (m_versionHistoryWidget)
        m_versionHistoryWidget->loadNote(noteId);
}

// ============================================================================
// Flashcard creation
// ============================================================================

void MainWindow::createFlashcard()
{
    if (!m_flashcardRepo || m_currentNoteId <= 0) return;

    QString selected = m_editor->textCursor().selectedText().trimmed();
    if (selected.isEmpty()) {
        bool ok;
        QString front = QInputDialog::getText(this,
            QStringLiteral("新建闪卡"), QStringLiteral("正面（问题）:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || front.trimmed().isEmpty()) return;

        QString back = QInputDialog::getText(this,
            QStringLiteral("新建闪卡"), QStringLiteral("背面（答案）:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || back.trimmed().isEmpty()) return;

        qint64 id = m_flashcardRepo->createCard(m_currentNoteId, front.trimmed(), back.trimmed());
        if (id > 0)
            statusBar()->showMessage(tr("闪卡已创建"), 2000);
    } else {
        bool ok;
        QString back = QInputDialog::getText(this,
            QStringLiteral("新建闪卡"), QStringLiteral("背面（答案）:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || back.trimmed().isEmpty()) return;

        qint64 id = m_flashcardRepo->createCard(m_currentNoteId, selected, back.trimmed());
        if (id > 0)
            statusBar()->showMessage(tr("闪卡已创建"), 2000);
    }
    updateReviewButton();
}


// ============================================================================
// Day 10: Export / Import / Backup
// ============================================================================

void MainWindow::onExportCurrentNote()
{
    if (m_currentNoteId <= 0) {
        statusBar()->showMessage(tr("\u6ca1\u6709\u6253\u5f00\u7684\u7b14\u8bb0"), 3000);
        return;
    }

    NoteData note = m_repo->getNote(m_currentNoteId);
    QString defaultName = note.title;
    if (defaultName.isEmpty()) defaultName = QStringLiteral("note");
    defaultName += QStringLiteral(".md");

    QString filePath = QFileDialog::getSaveFileName(this,
        tr("\u5bfc\u51fa\u5f53\u524d\u7b14\u8bb0"),
        defaultName,
        tr("Markdown \u6587\u4ef6 (*.md)"));
    if (filePath.isEmpty()) return;

    QString result = m_exportService->exportNote(m_currentNoteId,
        QFileInfo(filePath).absolutePath());
    if (!result.isEmpty())
        statusBar()->showMessage(tr("\u5df2\u5bfc\u51fa: %1").arg(result), 3000);
}

void MainWindow::onExportAllNotes()
{
    QString dirPath = QFileDialog::getExistingDirectory(this,
        tr("\u5bfc\u51fa\u5168\u90e8\u7b14\u8bb0\u5230\u6587\u4ef6\u5939"));
    if (dirPath.isEmpty()) return;

    saveCurrentNote();
    QStringList files = m_exportService->exportAll(dirPath);

    QMessageBox::information(this,
        tr("\u5bfc\u51fa\u5b8c\u6210"),
        tr("\u5df2\u5bfc\u51fa %1 \u7bc7\u7b14\u8bb0\u5230:\n%2")
            .arg(files.size()).arg(dirPath));
    statusBar()->showMessage(tr("\u5df2\u5bfc\u51fa %1 \u7bc7\u7b14\u8bb0").arg(files.size()), 5000);
}

void MainWindow::onImportMarkdownFiles()
{
    QStringList filePaths = QFileDialog::getOpenFileNames(this,
        tr("\u5bfc\u5165 Markdown \u6587\u4ef6"),
        QString(),
        tr("Markdown \u6587\u4ef6 (*.md)"));
    if (filePaths.isEmpty()) return;

    int count = 0;
    for (const QString &fp : filePaths) {
        qint64 id = m_importService->importFile(fp);
        if (id > 0) count++;
    }

    m_treeModel->refresh();
    QMessageBox::information(this,
        tr("\u5bfc\u5165\u5b8c\u6210"),
        tr("\u5df2\u5bfc\u5165 %1 / %2 \u4e2a\u6587\u4ef6").arg(count).arg(filePaths.size()));
    statusBar()->showMessage(tr("\u5df2\u5bfc\u5165 %1 \u7bc7\u7b14\u8bb0").arg(count), 5000);
}

void MainWindow::onImportMarkdownDirectory()
{
    QString dirPath = QFileDialog::getExistingDirectory(this,
        tr("\u5bfc\u5165\u6587\u4ef6\u5939\u4e2d\u7684 Markdown \u6587\u4ef6"));
    if (dirPath.isEmpty()) return;

    int count = m_importService->importDirectory(dirPath);
    m_treeModel->refresh();
    QMessageBox::information(this,
        tr("\u5bfc\u5165\u5b8c\u6210"),
        tr("\u5df2\u4ece\u6587\u4ef6\u5939\u5bfc\u5165 %1 \u7bc7\u7b14\u8bb0").arg(count));
    statusBar()->showMessage(tr("\u5df2\u5bfc\u5165 %1 \u7bc7\u7b14\u8bb0").arg(count), 5000);
}

void MainWindow::onBackupDatabase()
{
    QString filePath = QFileDialog::getSaveFileName(this,
        tr("\u5907\u4efd\u6570\u636e\u5e93"),
        QStringLiteral("knowledge_notes_backup_%1.db")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        tr("SQLite \u6570\u636e\u5e93 (*.db)"));
    if (filePath.isEmpty()) return;

    QString dbPath = QStringLiteral(PROJECT_ROOT "/data/knowledge_notes.db");
    if (QFile::copy(dbPath, filePath)) {
        QMessageBox::information(this,
            tr("\u5907\u4efd\u5b8c\u6210"),
            tr("\u6570\u636e\u5e93\u5df2\u5907\u4efd\u5230:\n%1").arg(filePath));
        statusBar()->showMessage(tr("\u6570\u636e\u5e93\u5df2\u5907\u4efd"), 3000);
    } else {
        QMessageBox::warning(this,
            tr("\u5907\u4efd\u5931\u8d25"),
            tr("\u65e0\u6cd5\u590d\u5236\u6570\u636e\u5e93\u6587\u4ef6"));
    }
}

void MainWindow::onRestoreDatabase()
{
    auto ret = QMessageBox::warning(this,
        tr("\u6062\u590d\u6570\u636e\u5e93"),
        tr("\u8b66\u544a\uff1a\u6062\u590d\u5c06\u8986\u76d6\u5f53\u524d\u6240\u6709\u6570\u636e\uff01\n\n"
           "\u8bf7\u786e\u4fdd\u5df2\u5907\u4efd\u5f53\u524d\u6570\u636e\u3002\n"
           "\u7a0b\u5e8f\u5c06\u5728\u6062\u590d\u540e\u81ea\u52a8\u91cd\u542f\u3002"),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString filePath = QFileDialog::getOpenFileName(this,
        tr("\u9009\u62e9\u5907\u4efd\u6587\u4ef6"),
        QString(),
        tr("SQLite \u6570\u636e\u5e93 (*.db)"));
    if (filePath.isEmpty()) return;

    QString dbPath = QStringLiteral(PROJECT_ROOT "/data/knowledge_notes.db");
    if (QFile::copy(filePath, dbPath)) {
        QMessageBox::information(this,
            tr("\u6062\u590d\u5b8c\u6210"),
            tr("\u6570\u636e\u5e93\u5df2\u6062\u590d\uff0c\u7a0b\u5e8f\u5c06\u91cd\u542f\u3002"));
        qApp->exit(773);
    } else {
        QMessageBox::warning(this,
            tr("\u6062\u590d\u5931\u8d25"),
            tr("\u65e0\u6cd5\u6062\u590d\u6570\u636e\u5e93\u6587\u4ef6"));
    }
}