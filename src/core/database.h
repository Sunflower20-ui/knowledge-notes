#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

/**
 * @brief Manages the SQLite database connection, initialization, and migrations.
 *
 * Uses a singleton-like pattern — one database per vault path.
 */
class Database : public QObject
{
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    /// Open (or create) the SQLite database at the given path.
    /// Returns true on success.
    bool open(const QString &dbPath);

    /// Close the current connection.
    void close();

    /// Returns true if the database is currently open.
    bool isOpen() const;

    /// Returns the underlying QSqlDatabase handle.
    QSqlDatabase &handle();
    const QSqlDatabase &handle() const;

    /// Current database file path.
    QString path() const;

    /// Schema version after migrations.
    int schemaVersion() const;

    /// Rebuild the entire FTS index from scratch.
    bool rebuildFtsIndex();

private:
    bool createSchema();
    bool runMigrations();

    QSqlDatabase m_db;
    QString m_path;
    int m_schemaVersion = 0;
};
