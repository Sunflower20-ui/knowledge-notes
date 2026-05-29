#pragma once

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include "search_result.h"

class Database;

// ---- Data structures ----

struct NoteData {
    qint64  id         = -1;
    QString title;
    QString content;
    QString plainText;
    QDateTime createdAt;
    QDateTime updatedAt;
    bool    isDeleted = false;
};

struct TagData {
    qint64  id;
    QString name;
};

struct LinkData {
    qint64  id;
    qint64  sourceNoteId;
    QString targetTitle;
    qint64  targetNoteId = -1;   // -1 means unresolved
};

// ---- Repository ----

class NoteRepository : public QObject
{
    Q_OBJECT

public:
    explicit NoteRepository(Database &db, QObject *parent = nullptr);

    // --- Notes ---

    /// Create a new note. Returns the new note ID, or -1 on failure.
    qint64 createNote(const QString &title = QString(),
                      const QString &content = QString());

    /// Update title and content of an existing note.
    bool updateNote(qint64 id, const QString &title, const QString &content);

    /// Soft-delete a note (sets is_deleted = 1).
    bool deleteNote(qint64 id);

    /// Permanently remove a note and its associated data.
    bool purgeNote(qint64 id);

    /// Restore a soft-deleted note.
    bool restoreNote(qint64 id);

    /// Fetch a single note by ID.
    NoteData getNote(qint64 id) const;

    /// List all non-deleted notes, ordered by updated_at DESC.
    QVector<NoteData> listNotes(int limit = 100, int offset = 0) const;

    /// Search notes by title or content plain-text (LIKE).
    QVector<NoteData> searchNotes(const QString &keyword, int limit = 50) const;

    /// Full-text search via FTS5 with BM25 ranking and <mark> highlighted snippets.
    QVector<SearchResult> searchFts(const QString &keyword, int limit = 20) const;

    // --- Tags ---

    qint64  createTag(const QString &name);
    TagData getTag(qint64 id) const;
    TagData getTagByName(const QString &name) const;
    QVector<TagData> listTags() const;

    bool addTagToNote(qint64 noteId, qint64 tagId);
    bool removeTagFromNote(qint64 noteId, qint64 tagId);
    QVector<TagData> tagsForNote(qint64 noteId) const;
    QVector<NoteData> notesForTag(qint64 tagId) const;

    // --- Wiki links ---

    /// Record a link from sourceNoteId to a note with the given title.
    qint64 addLink(qint64 sourceNoteId, const QString &targetTitle,
                   qint64 targetNoteId = -1);

    /// Remove all outgoing links from sourceNoteId and replace them.
    bool syncLinks(qint64 sourceNoteId,
                   const QVector<QPair<QString, qint64>> &links);

    /// Get all outgoing links from a note.
    QVector<LinkData> outgoingLinks(qint64 noteId) const;

    /// Get all backlinks pointing to a note (by resolved target_note_id).
    QVector<LinkData> backlinks(qint64 noteId) const;

signals:
    void noteCreated(qint64 id);
    void noteUpdated(qint64 id);
    void noteDeleted(qint64 id);

private:
    void indexFts(qint64 noteId, const QString &title, const QString &plainText);
    void deindexFts(qint64 noteId);

    Database &m_db;
};
