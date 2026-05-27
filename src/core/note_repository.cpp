#include "note_repository.h"
#include "database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

NoteRepository::NoteRepository(Database &db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

// ============================================================================
// Notes
// ============================================================================

qint64 NoteRepository::createNote(const QString &title, const QString &content)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "INSERT INTO notes(title, content, plain_text) "
        "VALUES(:title, :content, :plain)"));
    q.bindValue(QStringLiteral(":title"), title);
    q.bindValue(QStringLiteral(":content"), content);
    // Strip Markdown syntax for plain-text search indexing
    q.bindValue(QStringLiteral(":plain"), content);   // naive for now; Day 4 improves this

    if (!q.exec()) {
        qWarning() << "createNote:" << q.lastError().text();
        return -1;
    }

    const qint64 id = q.lastInsertId().toLongLong();
    emit noteCreated(id);
    return id;
}

bool NoteRepository::updateNote(qint64 id, const QString &title,
                                const QString &content)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE notes SET title = :title, content = :content,"
        "  plain_text = :plain, updated_at = datetime('now')"
        " WHERE id = :id AND is_deleted = 0"));
    q.bindValue(QStringLiteral(":title"), title);
    q.bindValue(QStringLiteral(":content"), content);
    q.bindValue(QStringLiteral(":plain"), content);
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        qWarning() << "updateNote:" << q.lastError().text();
        return false;
    }

    if (q.numRowsAffected() > 0) {
        emit noteUpdated(id);
        return true;
    }
    return false;
}

bool NoteRepository::deleteNote(qint64 id)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE notes SET is_deleted = 1, updated_at = datetime('now') "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        qWarning() << "deleteNote:" << q.lastError().text();
        return false;
    }

    if (q.numRowsAffected() > 0) {
        emit noteDeleted(id);
        return true;
    }
    return false;
}

bool NoteRepository::purgeNote(qint64 id)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("DELETE FROM notes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        qWarning() << "purgeNote:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

bool NoteRepository::restoreNote(qint64 id)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE notes SET is_deleted = 0, updated_at = datetime('now') "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec()) {
        qWarning() << "restoreNote:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

NoteData NoteRepository::getNote(qint64 id) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, title, content, plain_text, created_at, updated_at, is_deleted "
        "FROM notes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec() || !q.next())
        return {};

    NoteData n;
    n.id        = q.value(0).toLongLong();
    n.title     = q.value(1).toString();
    n.content   = q.value(2).toString();
    n.plainText = q.value(3).toString();
    n.createdAt = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
    n.updatedAt = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
    n.isDeleted = q.value(6).toBool();
    return n;
}

QVector<NoteData> NoteRepository::listNotes(int limit, int offset) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, title, content, plain_text, created_at, updated_at, is_deleted "
        "FROM notes WHERE is_deleted = 0 ORDER BY updated_at DESC "
        "LIMIT :limit OFFSET :offset"));
    q.bindValue(QStringLiteral(":limit"), limit);
    q.bindValue(QStringLiteral(":offset"), offset);

    QVector<NoteData> result;
    if (!q.exec()) {
        qWarning() << "listNotes:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        NoteData n;
        n.id        = q.value(0).toLongLong();
        n.title     = q.value(1).toString();
        n.content   = q.value(2).toString();
        n.plainText = q.value(3).toString();
        n.createdAt = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
        n.updatedAt = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        n.isDeleted = q.value(6).toBool();
        result.append(n);
    }
    return result;
}

QVector<NoteData> NoteRepository::searchNotes(const QString &keyword,
                                               int limit) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, title, content, plain_text, created_at, updated_at, is_deleted "
        "FROM notes "
        "WHERE is_deleted = 0 AND (title LIKE :kw OR plain_text LIKE :kw2) "
        "ORDER BY updated_at DESC LIMIT :limit"));
    const QString pattern = QStringLiteral("%%1%").arg(keyword);
    q.bindValue(QStringLiteral(":kw"), pattern);
    q.bindValue(QStringLiteral(":kw2"), pattern);
    q.bindValue(QStringLiteral(":limit"), limit);

    QVector<NoteData> result;
    if (!q.exec()) {
        qWarning() << "searchNotes:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        NoteData n;
        n.id        = q.value(0).toLongLong();
        n.title     = q.value(1).toString();
        n.content   = q.value(2).toString();
        n.plainText = q.value(3).toString();
        n.createdAt = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
        n.updatedAt = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        n.isDeleted = q.value(6).toBool();
        result.append(n);
    }
    return result;
}

// ============================================================================
// Tags
// ============================================================================

qint64 NoteRepository::createTag(const QString &name)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO tags(name) VALUES(:name)"));
    q.bindValue(QStringLiteral(":name"), name);

    if (!q.exec()) {
        qWarning() << "createTag:" << q.lastError().text();
        return -1;
    }

    if (q.numRowsAffected() == 0) {
        // Already existed — look up the existing id
        return getTagByName(name).id;
    }
    return q.lastInsertId().toLongLong();
}

TagData NoteRepository::getTag(qint64 id) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("SELECT id, name FROM tags WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);

    if (!q.exec() || !q.next())
        return {};
    return { q.value(0).toLongLong(), q.value(1).toString() };
}

TagData NoteRepository::getTagByName(const QString &name) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("SELECT id, name FROM tags WHERE name = :name"));
    q.bindValue(QStringLiteral(":name"), name);

    if (!q.exec() || !q.next())
        return {};
    return { q.value(0).toLongLong(), q.value(1).toString() };
}

QVector<TagData> NoteRepository::listTags() const
{
    QSqlQuery q(m_db.handle());
    q.exec(QStringLiteral("SELECT id, name FROM tags ORDER BY name"));

    QVector<TagData> result;
    while (q.next())
        result.append({ q.value(0).toLongLong(), q.value(1).toString() });
    return result;
}

bool NoteRepository::addTagToNote(qint64 noteId, qint64 tagId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO note_tags(note_id, tag_id) VALUES(:nid, :tid)"));
    q.bindValue(QStringLiteral(":nid"), noteId);
    q.bindValue(QStringLiteral(":tid"), tagId);

    if (!q.exec()) {
        qWarning() << "addTagToNote:" << q.lastError().text();
        return false;
    }
    return true;
}

bool NoteRepository::removeTagFromNote(qint64 noteId, qint64 tagId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "DELETE FROM note_tags WHERE note_id = :nid AND tag_id = :tid"));
    q.bindValue(QStringLiteral(":nid"), noteId);
    q.bindValue(QStringLiteral(":tid"), tagId);

    if (!q.exec()) {
        qWarning() << "removeTagFromNote:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

QVector<TagData> NoteRepository::tagsForNote(qint64 noteId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT t.id, t.name FROM tags t "
        "JOIN note_tags nt ON t.id = nt.tag_id "
        "WHERE nt.note_id = :nid ORDER BY t.name"));
    q.bindValue(QStringLiteral(":nid"), noteId);

    QVector<TagData> result;
    if (!q.exec()) return result;
    while (q.next())
        result.append({ q.value(0).toLongLong(), q.value(1).toString() });
    return result;
}

QVector<NoteData> NoteRepository::notesForTag(qint64 tagId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT n.id, n.title, n.content, n.plain_text, n.created_at, n.updated_at, n.is_deleted "
        "FROM notes n JOIN note_tags nt ON n.id = nt.note_id "
        "WHERE nt.tag_id = :tid AND n.is_deleted = 0 "
        "ORDER BY n.updated_at DESC"));
    q.bindValue(QStringLiteral(":tid"), tagId);

    QVector<NoteData> result;
    if (!q.exec()) return result;
    while (q.next()) {
        NoteData n;
        n.id        = q.value(0).toLongLong();
        n.title     = q.value(1).toString();
        n.content   = q.value(2).toString();
        n.plainText = q.value(3).toString();
        n.createdAt = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
        n.updatedAt = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        n.isDeleted = q.value(6).toBool();
        result.append(n);
    }
    return result;
}

// ============================================================================
// Wiki links
// ============================================================================

qint64 NoteRepository::addLink(qint64 sourceNoteId, const QString &targetTitle,
                                qint64 targetNoteId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "INSERT INTO links(source_note_id, target_title, target_note_id) "
        "VALUES(:src, :title, :tid)"));
    q.bindValue(QStringLiteral(":src"), sourceNoteId);
    q.bindValue(QStringLiteral(":title"), targetTitle);
    q.bindValue(QStringLiteral(":tid"), targetNoteId);

    if (!q.exec()) {
        qWarning() << "addLink:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool NoteRepository::syncLinks(qint64 sourceNoteId,
                                const QVector<QPair<QString, qint64>> &links)
{
    QSqlDatabase &db = m_db.handle();
    db.transaction();

    // Remove old links
    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM links WHERE source_note_id = :src"));
        q.bindValue(QStringLiteral(":src"), sourceNoteId);
        if (!q.exec()) {
            qWarning() << "syncLinks delete:" << q.lastError().text();
            db.rollback();
            return false;
        }
    }

    // Insert new links
    for (const auto &[title, targetId] : links) {
        if (addLink(sourceNoteId, title, targetId) < 0) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

QVector<LinkData> NoteRepository::outgoingLinks(qint64 noteId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, source_note_id, target_title, target_note_id "
        "FROM links WHERE source_note_id = :src"));
    q.bindValue(QStringLiteral(":src"), noteId);

    QVector<LinkData> result;
    if (!q.exec()) return result;
    while (q.next()) {
        result.append({
            q.value(0).toLongLong(),
            q.value(1).toLongLong(),
            q.value(2).toString(),
            q.value(3).isNull() ? -1 : q.value(3).toLongLong()
        });
    }
    return result;
}

QVector<LinkData> NoteRepository::backlinks(qint64 noteId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, source_note_id, target_title, target_note_id "
        "FROM links WHERE target_note_id = :tid"));
    q.bindValue(QStringLiteral(":tid"), noteId);

    QVector<LinkData> result;
    if (!q.exec()) return result;
    while (q.next()) {
        result.append({
            q.value(0).toLongLong(),
            q.value(1).toLongLong(),
            q.value(2).toString(),
            q.value(3).isNull() ? -1 : q.value(3).toLongLong()
        });
    }
    return result;
}
