#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class NoteRepository;
struct NoteData;

/// Export notes as Markdown files to the filesystem.
class ExportService : public QObject
{
    Q_OBJECT

public:
    explicit ExportService(NoteRepository *repo, QObject *parent = nullptr);

    /// Export a single note by ID to a .md file.
    /// Returns the output file path, or empty on failure.
    QString exportNote(qint64 noteId, const QString &dirPath);

    /// Export all non-deleted notes to a directory.
    /// Returns the list of created file paths.
    QStringList exportAll(const QString &dirPath);

    /// Export notes in a specific folder.
    QStringList exportFolder(const QString &folder, const QString &dirPath);

private:
    QString writeNoteFile(const NoteData &note, const QString &dirPath);
    QString sanitizeFileName(const QString &title);

    NoteRepository *m_repo;
};
