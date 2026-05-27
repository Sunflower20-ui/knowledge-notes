#pragma once

#include <QMainWindow>

class QSplitter;
class QTreeView;
class QTextEdit;
class QTabWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();

    // Left panel: folder tree + tags
    QTreeView *m_folderTree = nullptr;

    // Center: editor + preview tabs
    QTabWidget *m_editorTabs = nullptr;
    QTextEdit *m_editor = nullptr;
    QTextEdit *m_preview = nullptr;

    // Right panel: backlinks / info
    QTabWidget *m_sidePanel = nullptr;

    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_rightSplitter = nullptr;
};
