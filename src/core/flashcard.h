#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>

// ============================================================================
// SM-2 Spaced Repetition Algorithm (SuperMemo 2)
// ============================================================================
//
// After each review, the user rates their recall on a 0–5 scale:
//   0 – complete blackout
//   1 – incorrect, but correct answer felt familiar
//   2 – incorrect, but correct answer was easy to recall once seen
//   3 – correct, but required significant effort
//   4 – correct, after some hesitation
//   5 – perfect recall with no hesitation
//
// The algorithm updates:
//   n        — repetition count
//   ef       — easiness factor (≥ 1.3)
//   interval — days until next review
//   due_date — when the card is due next
// ============================================================================

namespace Sm2 {

struct CardState {
    int     n        = 0;      // repetition count
    double  ef       = 2.5;    // easiness factor
    int     interval = 0;      // current interval in days
    QString dueDate;            // "YYYY-MM-DD"
};

/// Apply one review with quality q (0–5). Returns updated state.
CardState review(const CardState &prev, int quality);

} // namespace Sm2

// ============================================================================
// Flashcard data
// ============================================================================

struct FlashcardData {
    qint64   id        = -1;
    qint64   noteId    = -1;
    QString  front;
    QString  back;
    int      n         = 0;
    double   ef        = 2.5;
    int      interval  = 0;
    QString  dueDate;
    QDateTime createdAt;
    QDateTime updatedAt;

    /// Convenience: is this card due for review today?
    bool isDue() const;
};

// ============================================================================
// Flashcard repository (extends NoteRepository's database)
// ============================================================================

class Database;
class QSqlDatabase;

class FlashcardRepository
{
public:
    explicit FlashcardRepository(Database &db);

    /// Create a new flashcard linked to a note.
    qint64 createCard(qint64 noteId, const QString &front, const QString &back);

    /// Update a card's front/back text.
    bool updateCard(qint64 cardId, const QString &front, const QString &back);

    /// Apply a review result (quality 0-5) — updates SM-2 state and due date.
    bool reviewCard(qint64 cardId, int quality);

    /// Delete a flashcard.
    bool deleteCard(qint64 cardId);

    /// Get a single card by ID.
    FlashcardData getCard(qint64 cardId) const;

    /// List all flashcards for a given note.
    QVector<FlashcardData> cardsForNote(qint64 noteId) const;

    /// List all flashcards due on or before a given date (default: today).
    QVector<FlashcardData> dueCards(const QString &date = QString()) const;

    /// Count of due cards.
    int dueCount() const;

private:
    Database &m_db;
};
