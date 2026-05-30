#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class NoteRepository;

/// Import Markdown files as notes.
class ImportService : public QObject
{
    Q_OBJECT

public:
    explicit ImportService(NoteRepository *repo, QObject *parent = nullptr);

    /// Import a single .md file. Returns the new note ID, or -1 on failure.
    /// If `folder` is non-empty, the imported note is assigned to that folder.
    qint64 importFile(const QString &filePath, const QString &folder = QString());

    /// Import all .md files from a directory (recursively).
    /// Returns the number of notes created.
    int importDirectory(const QString &dirPath, const QString &folder = QString());

private:
    qint64 importOne(const QString &filePath, const QString &content, const QString &folder);
    QString extractTitle(const QString &content, const QString &fallback);

    NoteRepository *m_repo;
};
