#include "version_repository.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStringList>

// ============================================================================
// VersionRepository
// ============================================================================

VersionRepository::VersionRepository(Database &db, QObject *parent)
    : QObject(parent), m_db(db)
{
}

qint64 VersionRepository::saveVersion(qint64 noteId, const QString &title, const QString &content)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "INSERT INTO note_versions (note_id, title, content, version_number) "
        "VALUES (?, ?, ?, "
        "  (SELECT COALESCE(MAX(version_number), 0) + 1 FROM note_versions WHERE note_id = ?)"
        ")"
    ));
    q.addBindValue(noteId);
    q.addBindValue(title);
    q.addBindValue(content);
    q.addBindValue(noteId);

    if (!q.exec()) {
        qWarning() << "VersionRepository::saveVersion:" << q.lastError().text();
        return -1;
    }

    qint64 versionId = q.lastInsertId().toLongLong();
    emit versionSaved(noteId, versionId);
    return versionId;
}

QVector<VersionData> VersionRepository::listVersions(qint64 noteId) const
{
    QVector<VersionData> result;
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, title, content, version_number, created_at "
        "FROM note_versions WHERE note_id = ? "
        "ORDER BY version_number DESC"
    ));
    q.addBindValue(noteId);

    if (!q.exec()) {
        qWarning() << "VersionRepository::listVersions:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        VersionData v;
        v.id            = q.value(0).toLongLong();
        v.noteId        = q.value(1).toLongLong();
        v.title         = q.value(2).toString();
        v.content       = q.value(3).toString();
        v.versionNumber = q.value(4).toInt();
        v.createdAt     = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        result.append(v);
    }
    return result;
}

VersionData VersionRepository::getVersion(qint64 versionId) const
{
    VersionData v;
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, title, content, version_number, created_at "
        "FROM note_versions WHERE id = ?"
    ));
    q.addBindValue(versionId);

    if (q.exec() && q.next()) {
        v.id            = q.value(0).toLongLong();
        v.noteId        = q.value(1).toLongLong();
        v.title         = q.value(2).toString();
        v.content       = q.value(3).toString();
        v.versionNumber = q.value(4).toInt();
        v.createdAt     = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
    }
    return v;
}

VersionData VersionRepository::latestVersion(qint64 noteId) const
{
    VersionData v;
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, title, content, version_number, created_at "
        "FROM note_versions WHERE note_id = ? "
        "ORDER BY version_number DESC LIMIT 1"
    ));
    q.addBindValue(noteId);

    if (q.exec() && q.next()) {
        v.id            = q.value(0).toLongLong();
        v.noteId        = q.value(1).toLongLong();
        v.title         = q.value(2).toString();
        v.content       = q.value(3).toString();
        v.versionNumber = q.value(4).toInt();
        v.createdAt     = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
    }
    return v;
}

int VersionRepository::versionCount(qint64 noteId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM note_versions WHERE note_id = ?"));
    q.addBindValue(noteId);
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

bool VersionRepository::rollbackToVersion(qint64 noteId, qint64 versionId)
{
    VersionData v = getVersion(versionId);
    if (v.id < 0) return false;

    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE notes SET title = ?, content = ?, updated_at = datetime('now') WHERE id = ?"
    ));
    q.addBindValue(v.title);
    q.addBindValue(v.content);
    q.addBindValue(noteId);

    if (!q.exec()) {
        qWarning() << "VersionRepository::rollbackToVersion:" << q.lastError().text();
        return false;
    }

    emit noteRolledBack(noteId, versionId);
    return true;
}

bool VersionRepository::deleteVersion(qint64 versionId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("DELETE FROM note_versions WHERE id = ?"));
    q.addBindValue(versionId);
    return q.exec();
}

bool VersionRepository::deleteVersionsForNote(qint64 noteId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("DELETE FROM note_versions WHERE note_id = ?"));
    q.addBindValue(noteId);
    return q.exec();
}

int VersionRepository::clearHistory(qint64 noteId, int keepLatest)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "DELETE FROM note_versions WHERE note_id = ? AND id NOT IN ("
        "  SELECT id FROM note_versions WHERE note_id = ? "
        "  ORDER BY version_number DESC LIMIT ?"
        ")"
    ));
    q.addBindValue(noteId);
    q.addBindValue(noteId);
    q.addBindValue(keepLatest);

    if (!q.exec()) {
        qWarning() << "VersionRepository::clearHistory:" << q.lastError().text();
        return -1;
    }
    return q.numRowsAffected();
}

// ============================================================================
// DiffUtil
// ============================================================================

QVector<DiffLine> DiffUtil::compute(const QString &oldText, const QString &newText)
{
    QVector<DiffLine> result;

    QStringList oldLines = oldText.split('\n');
    QStringList newLines = newText.split('\n');

    int m = oldLines.size();
    int n = newLines.size();

    // LCS table
    QVector<QVector<int>> dp(m + 1, QVector<int>(n + 1, 0));
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (oldLines[i - 1] == newLines[j - 1])
                dp[i][j] = dp[i - 1][j - 1] + 1;
            else
                dp[i][j] = qMax(dp[i - 1][j], dp[i][j - 1]);
        }
    }

    // Backtrack to build diff
    int i = m, j = n;
    QVector<DiffLine> reversed;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && oldLines[i - 1] == newLines[j - 1]) {
            reversed.append({DiffLine::Unchanged, oldLines[i - 1]});
            i--; j--;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            reversed.append({DiffLine::Added, newLines[j - 1]});
            j--;
        } else {
            reversed.append({DiffLine::Removed, oldLines[i - 1]});
            i--;
        }
    }

    result.resize(reversed.size());
    for (int k = 0; k < reversed.size(); k++)
        result[k] = reversed[reversed.size() - 1 - k];

    return result;
}

QString DiffUtil::toHtml(const QVector<DiffLine> &lines)
{
    QString html = QStringLiteral("<div style='font-family: monospace; font-size: 13px; line-height: 1.6;'>");

    for (const DiffLine &line : lines) {
        QString escaped = line.text.toHtmlEscaped();
        if (escaped.isEmpty()) escaped = QStringLiteral(" ");

        switch (line.type) {
        case DiffLine::Added:
            html += QStringLiteral(
                "<div style='background: #1a3020; color: #a6e3a1; padding: 1px 4px;'>"
                "+ %1</div>"
            ).arg(escaped);
            break;
        case DiffLine::Removed:
            html += QStringLiteral(
                "<div style='background: #301a1a; color: #f38ba8; padding: 1px 4px;'>"
                "- %1</div>"
            ).arg(escaped);
            break;
        default:
            html += QStringLiteral(
                "<div style='padding: 1px 4px;'>  %1</div>"
            ).arg(escaped);
            break;
        }
    }

    html += QStringLiteral("</div>");
    return html;
}
