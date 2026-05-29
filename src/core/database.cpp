#include "database.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString &dbPath)
{
    if (m_db.isOpen()) {
        if (m_path == dbPath)
            return true;
        close();
    }

    // Ensure parent directory exists
    QFileInfo fi(dbPath);
    QDir().mkpath(fi.absolutePath());

    // Use a unique connection name based on the path hash
    const QString connName = QStringLiteral("knowledge_notes_%1")
        .arg(QString::number(qHash(dbPath), 16));

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Database::open failed:" << m_db.lastError().text();
        return false;
    }

    // Enable WAL mode for better concurrent read performance
    {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
        q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    }

    m_path = dbPath;

    if (!createSchema())
        return false;

    if (!runMigrations())
        return false;

    qInfo() << "Database opened:" << dbPath;
    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        const QString connName = m_db.connectionName();
        m_db.close();
        m_db = QSqlDatabase();           // reset copy before removing
        QSqlDatabase::removeDatabase(connName);
    }
    m_path.clear();
    m_schemaVersion = 0;
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

QSqlDatabase &Database::handle()
{
    return m_db;
}

const QSqlDatabase &Database::handle() const
{
    return m_db;
}

QString Database::path() const
{
    return m_path;
}

int Database::schemaVersion() const
{
    return m_schemaVersion;
}

bool Database::createSchema()
{
    QSqlQuery q(m_db);

    // ---- Schema version tracking ----
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_version ("
            "  version INTEGER NOT NULL"
            ")"))) {
        qWarning() << "createSchema: schema_version" << q.lastError().text();
        return false;
    }

    // ---- Notes ----
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS notes ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title       TEXT    NOT NULL DEFAULT '',"
            "  content     TEXT    NOT NULL DEFAULT '',"
            "  plain_text  TEXT    NOT NULL DEFAULT '',"
            "  created_at  TEXT    NOT NULL DEFAULT (datetime('now')),"
            "  updated_at  TEXT    NOT NULL DEFAULT (datetime('now')),"
            "  is_deleted  INTEGER NOT NULL DEFAULT 0"
            ")"))) {
        qWarning() << "createSchema: notes" << q.lastError().text();
        return false;
    }

    // Index for listing non-deleted notes, newest first
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_notes_updated "
        "ON notes(updated_at) WHERE is_deleted = 0"));

    // Index for title lookups (used by wiki-link autocomplete)
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_notes_title "
        "ON notes(title) WHERE is_deleted = 0"));

    // ---- Tags ----
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tags ("
            "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT    NOT NULL UNIQUE"
            ")"))) {
        qWarning() << "createSchema: tags" << q.lastError().text();
        return false;
    }

    // ---- Note–tag junction ----
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS note_tags ("
            "  note_id INTEGER NOT NULL REFERENCES notes(id) ON DELETE CASCADE,"
            "  tag_id  INTEGER NOT NULL REFERENCES tags(id)  ON DELETE CASCADE,"
            "  PRIMARY KEY (note_id, tag_id)"
            ")"))) {
        qWarning() << "createSchema: note_tags" << q.lastError().text();
        return false;
    }

    // ---- Wiki links ----
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS links ("
            "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  source_note_id   INTEGER NOT NULL REFERENCES notes(id) ON DELETE CASCADE,"
            "  target_title     TEXT    NOT NULL,"
            "  target_note_id   INTEGER REFERENCES notes(id) ON DELETE SET NULL"
            ")"))) {
        qWarning() << "createSchema: links" << q.lastError().text();
        return false;
    }

    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_links_source ON links(source_note_id)"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_links_target ON links(target_note_id)"));

    return true;
}

bool Database::runMigrations()
{
    // Read current version
    {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_version"));
        if (q.next())
            m_schemaVersion = q.value(0).toInt();
    }

    // ---- Migration v1 → v2: add FTS5 full-text search ----
    if (m_schemaVersion < 2) {
        QSqlQuery q(m_db);

        // Create the FTS5 virtual table (independent mode — no content-sync).
        // unicode61 tokenizer handles Chinese/Unicode; prefix='2' enables
        // prefix queries for auto-complete.
        if (!q.exec(QStringLiteral(
                "CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5("
                "  title, content,"
                "  tokenize='unicode61',"
                "  prefix='2'"
                ")"))) {
            qWarning() << "Migration v2: create notes_fts" << q.lastError().text();
            return false;
        }

        // Seed existing notes into the FTS index
        if (!q.exec(QStringLiteral(
                "INSERT INTO notes_fts(rowid, title, content) "
                "SELECT id, title, plain_text FROM notes WHERE is_deleted = 0"))) {
            qWarning() << "Migration v2: seed FTS" << q.lastError().text();
            return false;
        }

        q.exec(QStringLiteral("INSERT INTO schema_version(version) VALUES(2)"));
        m_schemaVersion = 2;

        qInfo() << "Migrated to schema v2: FTS5 index created";
    }

    // Seed version row if empty
    if (m_schemaVersion == 0) {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("INSERT INTO schema_version(version) VALUES(1)"));
        m_schemaVersion = 1;
    }

    qInfo() << "Database schema version:" << m_schemaVersion;
    return true;
}

bool Database::rebuildFtsIndex()
{
    QSqlQuery q(m_db);

    // Clear and rebuild from scratch
    if (!q.exec(QStringLiteral("DELETE FROM notes_fts"))) {
        qWarning() << "rebuildFtsIndex: delete" << q.lastError().text();
        return false;
    }

    if (!q.exec(QStringLiteral(
            "INSERT INTO notes_fts(rowid, title, content) "
            "SELECT id, title, plain_text FROM notes WHERE is_deleted = 0"))) {
        qWarning() << "rebuildFtsIndex: insert" << q.lastError().text();
        return false;
    }

    qInfo() << "FTS index rebuilt";
    return true;
}
