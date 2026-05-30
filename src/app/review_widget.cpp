#include "review_widget.h"
#include "core/flashcard.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFont>

ReviewWidget::ReviewWidget(FlashcardRepository *repo, QWidget *parent)
    : QWidget(parent), m_repo(repo)
{
    setObjectName(QStringLiteral("reviewWidget"));
    setStyleSheet(QStringLiteral(
        "QWidget#reviewWidget { background: #1e1e2e; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Progress
    m_progress = new QProgressBar;
    m_progress->setObjectName(QStringLiteral("reviewProgress"));
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #313244; border: none; border-radius: 4px; height: 6px; }"
        "QProgressBar::chunk { background: #cba6f7; border-radius: 4px; }"
    ));

    m_countLabel = new QLabel;
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setStyleSheet(QStringLiteral("QLabel { color: #6c7086; font-size: 13px; }"));

    layout->addWidget(m_countLabel);
    layout->addWidget(m_progress);

    layout->addStretch();

    // Front (question)
    m_frontLabel = new QLabel(QString::fromUtf8("\xe7\x82\xb9\xe5\x87\xbb\xe5\xb7\xa5\xe5\x85\xb7\xe6\xa0\x8f\xe3\x80\x8c\xe5\xa4\x8d\xe4\xb9\xa0\xe3\x80\x8d\xe5\xbc\x80\xe5\xa7\x8b"));
    m_frontLabel->setAlignment(Qt::AlignCenter);
    m_frontLabel->setWordWrap(true);
    m_frontLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #cdd6f4; font-size: 20px; padding: 32px; }"
    ));
    layout->addWidget(m_frontLabel);

    // State info (EF, interval)
    m_stateLabel = new QLabel;
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setStyleSheet(QStringLiteral("QLabel { color: #585b70; font-size: 12px; }"));
    layout->addWidget(m_stateLabel);

    // Show Answer button
    m_showBtn = new QPushButton(tr("\xe6\x98\xbe\xe7\xa4\xba\xe7\xad\x94\xe6\xa1\x88"));
    m_showBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #45475a; color: #cdd6f4; border: none; "
        "border-radius: 8px; padding: 12px 32px; font-size: 15px; }"
        "QPushButton:hover { background: #585b70; }"
    ));
    connect(m_showBtn, &QPushButton::clicked, this, &ReviewWidget::showAnswer);
    m_showBtn->hide();
    layout->addWidget(m_showBtn, 0, Qt::AlignCenter);

    // Back (answer)
    m_backLabel = new QLabel;
    m_backLabel->setAlignment(Qt::AlignCenter);
    m_backLabel->setWordWrap(true);
    m_backLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #a6e3a1; font-size: 18px; padding: 16px; "
        "background: #181825; border-radius: 8px; border: 1px solid #45475a; }"
    ));
    m_backLabel->hide();
    layout->addWidget(m_backLabel);

    layout->addStretch();

    // Rating buttons (2x3 grid)
    auto *rateGrid = new QGridLayout;
    rateGrid->setSpacing(6);

    const char *labels[]  = {
        "\xe5\xae\x8c\xe5\x85\xa8\xe5\xbf\x98\xe4\xba\x86",
        "\xe4\xbc\xbc\xe6\x9b\xbe\xe7\x9b\xb8\xe8\xaf\x86",
        "\xe7\x9c\x8b\xe8\xbf\x87\xe6\x89\x8d\xe7\x9f\xa5",
        "\xe5\x8b\x89\xe5\xbc\xba\xe6\x83\xb3\xe8\xb5\xb7",
        "\xe7\xa8\x8d\xe6\x9c\x89\xe7\x8a\xb9\xe8\xb1\xab",
        "\xe8\xbd\xbb\xe6\x9d\xbe\xe7\xad\x94\xe5\xaf\xb9"
    };
    const char *colors[]  = {"#f38ba8", "#fab387", "#f9e2af", "#a6e3a1", "#89b4fa", "#cba6f7"};

    for (int i = 0; i < 6; i++) {
        m_rateBtns[i] = new QPushButton(QStringLiteral("%1\n%2").arg(i).arg(labels[i]));
        m_rateBtns[i]->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: #1e1e2e; border: none; "
            "border-radius: 6px; padding: 6px 4px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { opacity: 0.85; }"
        ).arg(colors[i]));
        m_rateBtns[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_rateBtns[i]->hide();
        int q = i;
        connect(m_rateBtns[i], &QPushButton::clicked, this, [this, q] { rateCard(q); });
        rateGrid->addWidget(m_rateBtns[i], i / 3, i % 3);
    }
    layout->addLayout(rateGrid);
}

void ReviewWidget::startSession()
{
    m_backLabel->hide();
    m_showBtn->show();
    for (int i = 0; i < 6; i++) m_rateBtns[i]->hide();
    m_stateLabel->clear();

    auto cards = m_repo->dueCards();
    m_cardIds.clear();
    for (const auto &c : cards) m_cardIds.append(c.id);

    if (m_cardIds.isEmpty()) {
        m_frontLabel->setText(tr("\xe6\xb2\xa1\xe6\x9c\x89\xe5\xbe\x85\xe5\xa4\x8d\xe4\xb9\xa0\xe7\x9a\x84\xe5\x8d\xa1\xe7\x89\x87"));
        m_showBtn->hide();
        m_countLabel->setText(tr("0 \xe5\xbc\xa0\xe5\x8d\xa1\xe7\x89\x87"));
        m_progress->setMaximum(1);
        m_progress->setValue(1);
        return;
    }

    m_currentIndex = -1;
    m_progress->setMaximum(m_cardIds.size());
    showCard(0);
}

int ReviewWidget::dueCount() const
{
    return m_repo->dueCount();
}

void ReviewWidget::showCard(int index)
{
    if (index < 0 || index >= m_cardIds.size()) {
        m_frontLabel->setText(tr("\xe5\xa4\x8d\xe4\xb9\xa0\xe5\xae\x8c\xe6\x88\x90\xef\xbc\x81"));
        m_backLabel->hide();
        m_showBtn->hide();
        for (int i = 0; i < 6; i++) m_rateBtns[i]->hide();
        m_stateLabel->clear();
        m_countLabel->setText(tr("%1 \xe5\xbc\xa0\xe5\x8d\xa1\xe7\x89\x87").arg(m_cardIds.size()));
        m_progress->setValue(m_progress->maximum());
        emit finished();
        return;
    }

    m_currentIndex = index;
    FlashcardData card = m_repo->getCard(m_cardIds[index]);
    if (card.id < 0) {
        showCard(index + 1);
        return;
    }
    m_currentCardId = card.id;
    m_currentFront  = card.front;
    m_currentBack   = card.back;

    m_frontLabel->setText(m_currentFront);
    m_backLabel->hide();
    m_showBtn->show();

    for (int i = 0; i < 6; i++) m_rateBtns[i]->hide();

    m_stateLabel->setText(
        QStringLiteral("EF: %1  |  \xe9\x97\xb4\xe9\x9a\x94: %2 \xe5\xa4\xa9  |  \xe5\xa4\x8d\xe4\xb9\xa0: %3 \xe6\xac\xa1")
            .arg(card.ef, 0, 'f', 2)
            .arg(card.interval)
            .arg(card.n));

    m_progress->setValue(index);
    m_countLabel->setText(
        QStringLiteral("%1 / %2").arg(index + 1).arg(m_cardIds.size()));
}

void ReviewWidget::showAnswer()
{
    if (m_currentCardId < 0) return;
    m_backLabel->setText(m_currentBack);
    m_backLabel->show();
    m_showBtn->hide();

    for (int i = 0; i < 6; i++) m_rateBtns[i]->show();
}

void ReviewWidget::rateCard(int quality)
{
    m_repo->reviewCard(m_currentCardId, quality);
    showCard(m_currentIndex + 1);
}