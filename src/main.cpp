#include <QApplication>
#include <QDir>
#include <QDebug>
#include "app/main_window.h"
#include "core/database.h"
#include "core/note_repository.h"
#include "core/flashcard.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("KnowledgeNotes");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("KnowledgeNotes");

    // --- Day 2: Initialize SQLite database in project root ---
    const QString dbPath = QStringLiteral(PROJECT_ROOT "/data/knowledge_notes.db");
    QDir().mkpath(QStringLiteral(PROJECT_ROOT "/data"));

    Database database;
    if (!database.open(dbPath)) {
        qCritical() << "Failed to open database at" << dbPath;
        return 1;
    }

    qInfo() << "Database location:" << dbPath;

    NoteRepository repo(database);

    // Quick sanity: create a welcome note if the vault is empty
    if (repo.listNotes(1).isEmpty()) {
        qint64 id = repo.createNote(
            QStringLiteral("欢迎使用知识笔记"),
            QStringLiteral("# 欢迎 👋\n\n"
                           "这是你的第一篇笔记，开始写作吧！\n\n"
                           "- 使用 `[[wiki 链接]]` 连接笔记\n"
                           "- 用标签整理笔记\n"
                           "- `Ctrl+F` 搜索\n\n"
                           "祝你笔记愉快！ 🚀"));
        qInfo() << "Created welcome note, id =" << id;
    }

    FlashcardRepository flashcardRepo(database);
    MainWindow window(&repo, &flashcardRepo);
    window.show();

    return app.exec();
}
