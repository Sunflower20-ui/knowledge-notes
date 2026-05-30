#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QPair>

class Database;

// ---- Data structures ----

struct VersionData {
    qint64   id            = -1;
    qint64   noteId        = -1;
    QString  title;
    QString  content;
    int      versionNumber = 0;
    QDateTime createdAt;
};

struct DiffLine {
    enum Type { Unchanged, Added, Removed };
    Type    type;
    QString text;
};

// ---- Repository ----

class VersionRepository : public QObject
{
    Q_OBJECT

public:
    explicit VersionRepository(Database &db, QObject *parent = nullptr);

    /// Save a snapshot of the current note state as a new version.
    /// Returns the version ID, or -1 on failure.
    qint64 saveVersion(qint64 noteId, const QString &title, const QString &content);

    /// List all versions for a note, newest first.
    QVector<VersionData> listVersions(qint64 noteId) const;

    /// Get a specific version by ID.
    VersionData getVersion(qint64 versionId) const;

    /// Get the latest version for a note.
    VersionData latestVersion(qint64 noteId) const;

    /// Number of versions for a note.
    int versionCount(qint64 noteId) const;

    /// Rollback a note to a specific version (updates the notes table).
    bool rollbackToVersion(qint64 noteId, qint64 versionId);

    /// Delete all versions for a note (e.g., when note is purged).
    bool deleteVersion(qint64 versionId);
    bool deleteVersionsForNote(qint64 noteId);
    int clearHistory(qint64 noteId, int keepLatest = 1);

signals:
    void versionSaved(qint64 noteId, qint64 versionId);
    void noteRolledBack(qint64 noteId, qint64 versionId);

private:
    Database &m_db;
};

// ---- Diff utility ----

namespace DiffUtil {

/// Compute a simple line-by-line diff between two texts.
/// Returns a list of DiffLine entries with type and text.
QVector<DiffLine> compute(const QString &oldText, const QString &newText);

/// Convert diff lines to HTML for display.
QString toHtml(const QVector<DiffLine> &lines);

} // namespace DiffUtil