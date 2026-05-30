#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QProgressBar;
class FlashcardRepository;

/// A full-widget flashcard review session.
///
/// Shows one card at a time:
///   1. Front side (question) — user thinks
///   2. User clicks "Show Answer" → back side revealed
///   3. User rates recall 0–5 (click a button)
///   4. SM-2 state updated, next card shown
///
/// Emits finished() when all due cards are reviewed.
class ReviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReviewWidget(FlashcardRepository *repo, QWidget *parent = nullptr);

    /// Load all due cards and start the session.
    void startSession();

    /// How many cards are due right now.
    int dueCount() const;

signals:
    void finished();

private:
    void showCard(int index);
    void showAnswer();
    void rateCard(int quality);

    FlashcardRepository *m_repo;
    QVector<qint64> m_cardIds;       // due card IDs
    int m_currentIndex = -1;

    // Current card data
    QString m_currentFront;
    QString m_currentBack;
    qint64  m_currentCardId = -1;

    // UI
    QLabel *m_frontLabel = nullptr;
    QLabel *m_backLabel  = nullptr;
    QPushButton *m_showBtn = nullptr;
    QPushButton *m_rateBtns[6] = {};
    QProgressBar *m_progress = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_stateLabel = nullptr;  // shows EF, interval info
};
