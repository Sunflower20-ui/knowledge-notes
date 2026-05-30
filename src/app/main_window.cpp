#include "main_window.h"
#include "markdown_highlighter.h"
#include "note_tree_model.h"
#include "core/note_repository.h"

#include <QSplitter>
#include <QTreeView>
#include <QTextEdit>
#include <QTabWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QLineEdit>
#include <QDir>
#include <QFile>
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
#include <QDebug>

// ============================================================================
// Construction
// ============================================================================

MainWindow::MainWindow(NoteRepository *repo, QWidget *parent)
    : QMainWindow(parent), m_repo(repo)
{
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
// Note loading
// ============================================================================

void MainWindow::loadNoteIntoEditor(qint64 noteId)
{
    if (noteId < 0) return;

    // Save current note before switching
    if (m_currentNoteId > 0) {
        m_repo->updateNote(m_currentNoteId,
            m_editor->document()->toPlainText().section('\n', 0, 0).trimmed(),
            m_editor->toPlainText());
    }

    NoteData note = m_repo->getNote(noteId);
    if (note.id != noteId) return;

    m_currentNoteId = noteId;
    m_editor->setPlainText(note.content);
    m_previewDirty = true;
    updatePreview();

    statusBar()->showMessage(
        QStringLiteral("笔记 #%1 — %2").arg(noteId).arg(note.title));
}

void MainWindow::onTreeClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    qint64 noteId = m_treeModel->noteIdForIndex(index);
    if (noteId > 0) {
        loadNoteIntoEditor(noteId);
    }
}

// ============================================================================
// Keyboard Shortcuts
// ============================================================================

void MainWindow::setupShortcuts()
{
    // Bold: Ctrl+B
    auto *boldShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), m_editor);
    connect(boldShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("**"));
    });

    // Italic: Ctrl+I
    auto *italicShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+I")), m_editor);
    connect(italicShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("*"));
    });

    // Strikethrough
    auto *strikeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), m_editor);
    connect(strikeShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("~~"));
    });

    // Inline code
    auto *codeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), m_editor);
    connect(codeShortcut, &QShortcut::activated, this, [this] {
        toggleFormatting(QStringLiteral("`"));
    });

    // Tab → 4 spaces
    auto *tabShortcut = new QShortcut(QKeySequence(QStringLiteral("Tab")), m_editor);
    connect(tabShortcut, &QShortcut::activated, this, [this] {
        m_editor->insertPlainText(QStringLiteral("    "));
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

    // Ctrl+S → save current note
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] {
        if (m_currentNoteId > 0) {
            m_repo->updateNote(m_currentNoteId,
                m_editor->document()->toPlainText().section('\n', 0, 0).trimmed(),
                m_editor->toPlainText());
            m_treeModel->refresh();
            statusBar()->showMessage(tr("已保存"), 2000);
        }
    });

    // Ctrl+Shift+P → toggle preview
    auto *previewShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this);
    connect(previewShortcut, &QShortcut::activated, this, [this] {
        m_editorTabs->setCurrentIndex(m_editorTabs->currentIndex() == 0 ? 1 : 0);
    });
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
// Preview
// ============================================================================

void MainWindow::onEditorTextChanged()
{
    if (!m_previewDirty) {
        m_previewDirty = true;
        QTimer::singleShot(300, this, &MainWindow::updatePreview);
    }
}

void MainWindow::updatePreview()
{
    m_previewDirty = false;

    QString markdown = m_editor->toPlainText();
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
        "a { color: #89b4fa; }"
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

    // New Note
    QAction *newNoteBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/new_note.svg")), tr("新建"));
    newNoteBtn->setToolTip(tr("新建笔记 (Ctrl+N)"));
    connect(newNoteBtn, &QAction::triggered, this, [this] {
        // Save current
        if (m_currentNoteId > 0) {
            m_repo->updateNote(m_currentNoteId,
                m_editor->document()->toPlainText().section('\n', 0, 0).trimmed(),
                m_editor->toPlainText());
        }
        qint64 id = m_repo->createNote(QStringLiteral("未命名"), QString());
        m_treeModel->refresh();
        if (id > 0) loadNoteIntoEditor(id);
    });

    m_toolBar->addSeparator();

    // Bold
    QAction *boldBtn = m_toolBar->addAction(QIcon(), QStringLiteral("B"));
    boldBtn->setToolTip(tr("加粗 (Ctrl+B)"));
    QFont boldFont; boldFont.setBold(true); boldBtn->setFont(boldFont);
    connect(boldBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("**")); });

    // Italic
    QAction *italicBtn = m_toolBar->addAction(QIcon(), QStringLiteral("I"));
    italicBtn->setToolTip(tr("斜体 (Ctrl+I)"));
    QFont italicFont; italicFont.setItalic(true); italicBtn->setFont(italicFont);
    connect(italicBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("*")); });

    // Strikethrough
    QAction *strikeBtn = m_toolBar->addAction(QIcon(), QStringLiteral("S"));
    strikeBtn->setToolTip(tr("删除线 (Ctrl+Shift+S)"));
    QFont strikeFont; strikeFont.setStrikeOut(true); strikeBtn->setFont(strikeFont);
    connect(strikeBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("~~")); });

    // Inline code
    QAction *codeBtn = m_toolBar->addAction(QIcon(), QStringLiteral("<>"));
    codeBtn->setToolTip(tr("行内代码 (Ctrl+K)"));
    connect(codeBtn, &QAction::triggered, this, [this] { toggleFormatting(QStringLiteral("`")); });

    m_toolBar->addSeparator();

    // Link
    QAction *linkBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/link.svg")), tr("链接"));
    linkBtn->setToolTip(tr("插入链接 (Ctrl+L)"));
    connect(linkBtn, &QAction::triggered, this, [this] {
        insertFormatting(QStringLiteral("["), QStringLiteral("](url)"));
    });

    // Search box
    m_searchBox = new QLineEdit;
    m_searchBox->setPlaceholderText(tr("搜索笔记..."));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setMaximumWidth(240);
    m_toolBar->addWidget(m_searchBox);

    QAction *searchBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/search.svg")), tr("搜索"));
    connect(searchBtn, &QAction::triggered, this, [this] {
        m_searchBox->setFocus();
        m_searchBox->selectAll();
    });

    m_toolBar->addSeparator();

    // Sidebar toggle
    m_toggleSidebarAction = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/folder.svg")), tr("侧栏"));
    m_toggleSidebarAction->setCheckable(true);
    m_toggleSidebarAction->setChecked(true);
    connect(m_toggleSidebarAction, &QAction::triggered, this, [this] {
        m_folderTree->parentWidget()->setVisible(!m_folderTree->parentWidget()->isVisible());
        m_toggleSidebarAction->setChecked(m_folderTree->parentWidget()->isVisible());
    });

    QAction *toggleRightBtn = m_toolBar->addAction(
        QIcon(QStringLiteral(":/icons/outline.svg")), tr("大纲"));
    toggleRightBtn->setCheckable(true);
    toggleRightBtn->setChecked(true);
    connect(toggleRightBtn, &QAction::triggered, this, [this] {
        m_sidePanel->setVisible(!m_sidePanel->isVisible());
    });
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

    // Header
    auto *headerLabel = new QLabel(tr("  笔记"));
    headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #6c7086; font-size: 10px; font-weight: 700; "
        "letter-spacing: 1px; padding: 12px 12px 6px 12px; background: transparent; }"));

    // Tree view — now powered by NoteTreeModel
    m_treeModel = new NoteTreeModel(m_repo, this);
    m_folderTree = new QTreeView;
    m_folderTree->setModel(m_treeModel);
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setAnimated(true);
    m_folderTree->setIndentation(16);
    m_folderTree->setObjectName(QStringLiteral("folderTree"));
    m_folderTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_folderTree->setExpandsOnDoubleClick(false);
    // Expand the two top-level folders by default
    m_folderTree->expand(m_treeModel->index(0, 0, QModelIndex()));
    m_folderTree->expand(m_treeModel->index(1, 0, QModelIndex()));

    // Click handler
    connect(m_folderTree, &QTreeView::clicked, this, &MainWindow::onTreeClicked);

    // Note count footer
    m_noteCountLabel = new QLabel;
    m_noteCountLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #585b70; font-size: 11px; padding: 8px 12px; "
        "background: transparent; border-top: 1px solid #313244; }"));
    auto updateCount = [this] {
        int count = m_treeModel->rowCount(m_treeModel->index(0, 0, QModelIndex()));
        m_noteCountLabel->setText(QStringLiteral("  %1 notes").arg(count));
    };
    updateCount();
    // Refresh count when model refreshes
    connect(m_treeModel, &QAbstractItemModel::modelReset, this, updateCount);

    leftLayout->addWidget(headerLabel);
    leftLayout->addWidget(m_folderTree);
    leftLayout->addWidget(m_noteCountLabel);

    // ========== Center: editor + preview ==========
    m_editorTabs = new QTabWidget;
    m_editorTabs->setObjectName(QStringLiteral("editorTabs"));

    m_editor = new QTextEdit;
    m_editor->setPlaceholderText(QStringLiteral("Select a note or create a new one..."));
    m_editor->setTabStopDistance(32);
    m_editor->setObjectName(QStringLiteral("editor"));
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editorTabs->addTab(m_editor,
        QIcon(QStringLiteral(":/icons/edit.svg")),
        QStringLiteral(" Editor "));

    m_highlighter = new MarkdownHighlighter(m_editor->document());

    m_preview = new QTextEdit;
    m_preview->setReadOnly(true);
    m_preview->setPlaceholderText(QStringLiteral("Preview updates as you type..."));
    m_preview->setObjectName(QStringLiteral("preview"));
    m_editorTabs->addTab(m_preview,
        QIcon(QStringLiteral(":/icons/preview.svg")),
        QStringLiteral(" Preview "));

    connect(m_editor, &QTextEdit::textChanged, this, &MainWindow::onEditorTextChanged);
    updatePreview();

    // ========== Right: side panel ==========
    m_sidePanel = new QTabWidget;
    m_sidePanel->setMaximumWidth(300);
    m_sidePanel->setMinimumWidth(180);
    m_sidePanel->setObjectName(QStringLiteral("sidePanel"));

    auto *backlinksPlaceholder = new QLabel(QStringLiteral("Backlinks\nwill appear here..."));
    backlinksPlaceholder->setAlignment(Qt::AlignCenter);
    backlinksPlaceholder->setWordWrap(true);
    backlinksPlaceholder->setStyleSheet(QStringLiteral(
        "QLabel { color: #6c7086; font-size: 13px; padding: 24px; background: transparent; }"));
    m_sidePanel->addTab(backlinksPlaceholder,
        QIcon(QStringLiteral(":/icons/backlinks.svg")),
        QStringLiteral(" Backlinks "));

    auto *outlinePlaceholder = new QLabel(QStringLiteral("Outline / TOC\nwill appear here..."));
    outlinePlaceholder->setAlignment(Qt::AlignCenter);
    outlinePlaceholder->setWordWrap(true);
    outlinePlaceholder->setStyleSheet(QStringLiteral(
        "QLabel { color: #6c7086; font-size: 13px; padding: 24px; background: transparent; }"));
    m_sidePanel->addTab(outlinePlaceholder,
        QIcon(QStringLiteral(":/icons/outline.svg")),
        QStringLiteral(" Outline "));

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
        if (m_currentNoteId > 0) {
            m_repo->updateNote(m_currentNoteId,
                m_editor->document()->toPlainText().section('\n', 0, 0).trimmed(),
                m_editor->toPlainText());
        }
        qint64 id = m_repo->createNote(QStringLiteral("未命名"), QString());
        m_treeModel->refresh();
        if (id > 0) loadNoteIntoEditor(id);
    });

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
            m_toggleSidebarAction->setChecked(m_folderTree->parentWidget()->isVisible());
        });
    viewMenu->addAction(tr("切换预览(&P)"), QKeySequence(QStringLiteral("Ctrl+Shift+P")),
        this, [this] { m_editorTabs->setCurrentIndex(m_editorTabs->currentIndex() == 0 ? 1 : 0); });

    // --- Help ---
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("关于(&A)"), this, [] {});
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("就绪 — Ctrl+N 新建  Ctrl+S 保存  Ctrl+B 加粗  Ctrl+I 斜体"));
}
