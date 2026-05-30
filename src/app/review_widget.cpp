#include "review_widget.h"
#include "core/flashcard.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    m_frontLabel = new QLabel(QStringLiteral("准备好开始复习了吗？"));
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
    m_showBtn = new QPushButton(tr("显示答案"));
    m_showBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #45475a; color: #cdd6f4; border: none; "
        "border-radius: 8px; padding: 12px 32px; font-size: 15px; }"
        "QPushButton:hover { background: #585b70; }"
    ));
    connect(m_showBtn, &QPushButton::clicked, this, &ReviewWidget::showAnswer);
    layout->addWidget(m_showBtn, 0, Qt::AlignCenter);

    // Back (answer) — hidden until revealed
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

    // Rating buttons (0–5)
    auto *rateLayout = new QHBoxLayout;
    rateLayout->setSpacing(8);

    const char *labels[]  = {"完全忘了", "似曾相识", "看过才知", "勉强想起", "稍有犹豫", "轻松答对"};
    const char *colors[]  = {"#f38ba8", "#fab387", "#f9e2af", "#a6e3a1", "#89b4fa", "#cba6f7"};

    for (int i = 0; i < 6; i++) {
        m_rateBtns[i] = new QPushButton(QStringLiteral("%1\n%2").arg(i).arg(labels[i]));
        m_rateBtns[i]->setStyleSheet(QStringLiteral(
            "QPushButton { background: %1; color: #1e1e2e; border: none; "
            "border-radius: 8px; padding: 10px 6px; font-size: 12px; font-weight: 600; "
            "min-width: 80px; }"
            "QPushButton:hover { opacity: 0.85; }"
        ).arg(colors[i]));
        m_rateBtns[i]->hide();
        int q = i;
        connect(m_rateBtns[i], &QPushButton::clicked, this, [this, q] { rateCard(q); });
        rateLayout->addWidget(m_rateBtns[i]);
    }
    layout->addLayout(rateLayout);
}

void ReviewWidget::startSession()
{
    auto cards = m_repo->dueCards();
    m_cardIds.clear();
    for (const auto &c : cards) m_cardIds.append(c.id);

    if (m_cardIds.isEmpty()) {
        m_frontLabel->setText(tr("🎉 没有待复习的卡片！"));
        m_showBtn->hide();
        m_countLabel->setText(tr("0 张卡片"));
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
        emit finished();
        return;
    }

    m_currentIndex = index;
    FlashcardData card = m_repo->getCard(m_cardIds[index]);
    m_currentCardId = card.id;
    m_currentFront  = card.front;
    m_currentBack   = card.back;

    // Show front
    m_frontLabel->setText(m_currentFront);
    m_backLabel->hide();
    m_showBtn->show();

    // Hide rating buttons
    for (int i = 0; i < 6; i++) m_rateBtns[i]->hide();

    // State info
    m_stateLabel->setText(
        QStringLiteral("EF: %1  |  间隔: %2 天  |  复习: %3 次")
            .arg(card.ef, 0, 'f', 2)
            .arg(card.interval)
            .arg(card.n));

    // Progress
    m_progress->setValue(index);
    m_countLabel->setText(
        QStringLiteral("%1 / %2").arg(index + 1).arg(m_cardIds.size()));
}

void ReviewWidget::showAnswer()
{
    m_backLabel->setText(m_currentBack);
    m_backLabel->show();
    m_showBtn->hide();

    // Show rating buttons
    for (int i = 0; i < 6; i++) m_rateBtns[i]->show();
}

void ReviewWidget::rateCard(int quality)
{
    m_repo->reviewCard(m_currentCardId, quality);
    showCard(m_currentIndex + 1);
}
