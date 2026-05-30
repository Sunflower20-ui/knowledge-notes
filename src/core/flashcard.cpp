#include "flashcard.h"
#include "database.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDate>
#include <QDebug>
#include <cmath>

// ============================================================================
// SM-2 Algorithm
// ============================================================================

Sm2::CardState Sm2::review(const CardState &prev, int quality)
{
    CardState next = prev;

    if (quality < 0) quality = 0;
    if (quality > 5) quality = 5;

    if (quality < 3) {
        // Failed — reset
        next.n        = 0;
        next.interval = 1;  // try again tomorrow
    } else {
        // Passed — update easiness factor
        next.ef = prev.ef + (0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02));
        if (next.ef < 1.3) next.ef = 1.3;

        // Calculate next interval
        if (prev.n == 0) {
            next.interval = 1;
        } else if (prev.n == 1) {
            next.interval = 6;
        } else {
            next.interval = static_cast<int>(std::round(prev.interval * next.ef));
        }
        next.n = prev.n + 1;
    }

    // Always at least 1 day
    if (next.interval < 1) next.interval = 1;

    // Next due date = today + interval
    next.dueDate = QDate::currentDate().addDays(next.interval).toString(QStringLiteral("yyyy-MM-dd"));

    return next;
}

// ============================================================================
// FlashcardData
// ============================================================================

bool FlashcardData::isDue() const
{
    return dueDate <= QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

// ============================================================================
// FlashcardRepository
// ============================================================================

FlashcardRepository::FlashcardRepository(Database &db)
    : m_db(db)
{
}

qint64 FlashcardRepository::createCard(qint64 noteId, const QString &front, const QString &back)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "INSERT INTO flashcards(note_id, front, back) "
        "VALUES(:nid, :front, :back)"));
    q.bindValue(QStringLiteral(":nid"), noteId);
    q.bindValue(QStringLiteral(":front"), front);
    q.bindValue(QStringLiteral(":back"), back);

    if (!q.exec()) {
        qWarning() << "createCard:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool FlashcardRepository::updateCard(qint64 cardId, const QString &front, const QString &back)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE flashcards SET front = :front, back = :back, "
        "updated_at = datetime('now') WHERE id = :id"));
    q.bindValue(QStringLiteral(":front"), front);
    q.bindValue(QStringLiteral(":back"), back);
    q.bindValue(QStringLiteral(":id"), cardId);
    return q.exec();
}

bool FlashcardRepository::reviewCard(qint64 cardId, int quality)
{
    FlashcardData card = getCard(cardId);
    if (card.id < 0) return false;

    Sm2::CardState prev;
    prev.n        = card.n;
    prev.ef       = card.ef;
    prev.interval = card.interval;
    prev.dueDate  = card.dueDate;

    Sm2::CardState next = Sm2::review(prev, quality);

    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "UPDATE flashcards SET n = :n, ef = :ef, interval = :iv, "
        "due_date = :due, updated_at = datetime('now') WHERE id = :id"));
    q.bindValue(QStringLiteral(":n"), next.n);
    q.bindValue(QStringLiteral(":ef"), next.ef);
    q.bindValue(QStringLiteral(":iv"), next.interval);
    q.bindValue(QStringLiteral(":due"), next.dueDate);
    q.bindValue(QStringLiteral(":id"), cardId);
    return q.exec();
}

bool FlashcardRepository::deleteCard(qint64 cardId)
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral("DELETE FROM flashcards WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), cardId);
    return q.exec();
}

FlashcardData FlashcardRepository::getCard(qint64 cardId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, front, back, n, ef, interval, due_date, "
        "created_at, updated_at FROM flashcards WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), cardId);

    if (!q.exec() || !q.next())
        return {};

    FlashcardData c;
    c.id        = q.value(0).toLongLong();
    c.noteId    = q.value(1).toLongLong();
    c.front     = q.value(2).toString();
    c.back      = q.value(3).toString();
    c.n         = q.value(4).toInt();
    c.ef        = q.value(5).toDouble();
    c.interval  = q.value(6).toInt();
    c.dueDate   = q.value(7).toString();
    c.createdAt = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
    c.updatedAt = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
    return c;
}

QVector<FlashcardData> FlashcardRepository::cardsForNote(qint64 noteId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, front, back, n, ef, interval, due_date, "
        "created_at, updated_at FROM flashcards "
        "WHERE note_id = :nid ORDER BY created_at"));
    q.bindValue(QStringLiteral(":nid"), noteId);

    QVector<FlashcardData> result;
    if (!q.exec()) return result;

    while (q.next()) {
        FlashcardData c;
        c.id        = q.value(0).toLongLong();
        c.noteId    = q.value(1).toLongLong();
        c.front     = q.value(2).toString();
        c.back      = q.value(3).toString();
        c.n         = q.value(4).toInt();
        c.ef        = q.value(5).toDouble();
        c.interval  = q.value(6).toInt();
        c.dueDate   = q.value(7).toString();
        c.createdAt = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        c.updatedAt = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
        result.append(c);
    }
    return result;
}

QVector<FlashcardData> FlashcardRepository::dueCards(const QString &date) const
{
    QString d = date.isEmpty() ? QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")) : date;

    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT id, note_id, front, back, n, ef, interval, due_date, "
        "created_at, updated_at FROM flashcards "
        "WHERE due_date <= :due ORDER BY due_date, created_at"));
    q.bindValue(QStringLiteral(":due"), d);

    QVector<FlashcardData> result;
    if (!q.exec()) return result;

    while (q.next()) {
        FlashcardData c;
        c.id        = q.value(0).toLongLong();
        c.noteId    = q.value(1).toLongLong();
        c.front     = q.value(2).toString();
        c.back      = q.value(3).toString();
        c.n         = q.value(4).toInt();
        c.ef        = q.value(5).toDouble();
        c.interval  = q.value(6).toInt();
        c.dueDate   = q.value(7).toString();
        c.createdAt = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        c.updatedAt = QDateTime::fromString(q.value(9).toString(), Qt::ISODate);
        result.append(c);
    }
    return result;
}

int FlashcardRepository::dueCount() const
{
    QSqlQuery q(m_db.handle());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM flashcards WHERE due_date <= date('now')"));
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}
