#include "main_window.h"

#include <QSplitter>
#include <QTreeView>
#include <QTextEdit>
#include <QTabWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileSystemModel>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenuBar();
    setupStatusBar();
}

void MainWindow::setupUi()
{
    // --- Left: folder tree ---
    m_folderTree = new QTreeView;
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setMaximumWidth(280);
    m_folderTree->setMinimumWidth(180);
    m_folderTree->setAnimated(true);
    m_folderTree->setIndentation(16);

    // Placeholder model — will be replaced with real vault model on Day 5
    m_folderTree->setModel(new QFileSystemModel);

    // --- Center: editor + preview tabs ---
    m_editorTabs = new QTabWidget;

    m_editor = new QTextEdit;
    m_editor->setPlaceholderText("Start writing...");
    m_editor->setTabStopDistance(32);
    m_editorTabs->addTab(m_editor, "Editor");

    m_preview = new QTextEdit;
    m_preview->setReadOnly(true);
    m_preview->setPlaceholderText("Markdown preview will appear here...");
    m_editorTabs->addTab(m_preview, "Preview");

    // --- Right: side panel (backlinks / outline / info) ---
    m_sidePanel = new QTabWidget;
    m_sidePanel->setMaximumWidth(300);
    m_sidePanel->setMinimumWidth(180);

    auto *backlinksPlaceholder = new QLabel("Backlinks will appear here...");
    backlinksPlaceholder->setAlignment(Qt::AlignCenter);
    backlinksPlaceholder->setWordWrap(true);
    m_sidePanel->addTab(backlinksPlaceholder, "Backlinks");

    auto *outlinePlaceholder = new QLabel("Outline / TOC will appear here...");
    outlinePlaceholder->setAlignment(Qt::AlignCenter);
    outlinePlaceholder->setWordWrap(true);
    m_sidePanel->addTab(outlinePlaceholder, "Outline");

    // --- Splitter layout ---
    m_rightSplitter = new QSplitter(Qt::Horizontal);
    m_rightSplitter->addWidget(m_editorTabs);
    m_rightSplitter->addWidget(m_sidePanel);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 1);

    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->addWidget(m_folderTree);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_mainSplitter);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    fileMenu->addAction(tr("&New Note"), this, [] {
        // TODO: Day 2 — create new note via storage layer
    }, QKeySequence::New);

    fileMenu->addAction(tr("&Open Vault..."), this, [] {
        // TODO: Day 5 — vault / folder management
    }, QKeySequence::Open);

    fileMenu->addSeparator();

    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Toggle &Sidebar"), this, [this] {
        m_folderTree->setVisible(!m_folderTree->isVisible());
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [] {
        // TODO: About dialog
    });
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}
