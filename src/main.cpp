#include <QApplication>
#include <QDir>
#include <QDebug>
#include "app/main_window.h"
#include "core/database.h"
#include "core/note_repository.h"

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
            QStringLiteral("Welcome to KnowledgeNotes"),
            QStringLiteral("# Welcome 👋\n\n"
                           "This is your first note. Start writing!\n\n"
                           "- Use `[[wiki links]]` to connect notes\n"
                           "- Tag notes to organize them\n"
                           "- Search with `Ctrl+F`\n\n"
                           "Happy note-taking! 🚀"));
        qInfo() << "Created welcome note, id =" << id;
    }

    MainWindow window;
    window.setWindowTitle("KnowledgeNotes");
    window.resize(1200, 800);
    window.show();

    return app.exec();
}
