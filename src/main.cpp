#include <QApplication>
#include <QDir>
#include <QDebug>
#include "app/main_window.h"
#include "core/database.h"
#include "core/note_repository.h"
#include "core/flashcard.h"
#include "core/version_repository.h"

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
            QString::fromUtf8("\xe6\xac\xa2\xe8\xbf\x8e\xe4\xbd\xbf\xe7\x94\xa8\xe7\x9f\xa5\xe8\xaf\x86\xe7\xac\x94\xe8\xae\xb0"),
            QString::fromUtf8("# \xe6\xac\xa2\xe8\xbf\x8e \xf0\x9f\x91\x8b\n\n"
                           "\xe8\xbf\x99\xe6\x98\xaf\xe4\xbd\xa0\xe7\x9a\x84\xe7\xac\xac\xe4\xb8\x80\xe7\xaf\x87\xe7\xac\x94\xe8\xae\xb0\xef\xbc\x8c\xe5\xbc\x80\xe5\xa7\x8b\xe5\x86\x99\xe4\xbd\x9c\xe5\x90\xa7\xef\xbc\x81\n\n"
                           "- \xe4\xbd\xbf\xe7\x94\xa8 `[[wiki \xe9\x93\xbe\xe6\x8e\xa5]]` \xe8\xbf\x9e\xe6\x8e\xa5\xe7\xac\x94\xe8\xae\xb0\n"
                           "- \xe7\x94\xa8\xe6\xa0\x87\xe7\xad\xbe\xe6\x95\xb4\xe7\x90\x86\xe7\xac\x94\xe8\xae\xb0\n"
                           "- `Ctrl+F` \xe6\x90\x9c\xe7\xb4\xa2\n\n"
                           "\xe7\xa5\x9d\xe4\xbd\xa0\xe7\xac\x94\xe8\xae\xb0\xe6\x84\x89\xe5\xbf\xab\xef\xbc\x81\xf0\x9f\x9a\x80"));
        qInfo() << "Created welcome note, id =" << id;
    }

    FlashcardRepository flashcardRepo(database);
    VersionRepository versionRepo(database);
    MainWindow window(&repo, &flashcardRepo, &versionRepo);
    window.show();

    return app.exec();
}