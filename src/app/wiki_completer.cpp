#include "wiki_completer.h"
#include "core/note_repository.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QApplication>
#include <QScreen>
#include <QDebug>

WikiCompleter::WikiCompleter(NoteRepository *repo, QWidget *parent)
    : QWidget(parent, Qt::Popup)          // Popup: auto-closes on outside click
    , m_repo(repo)
{
    setObjectName(QStringLiteral("wikiCompleter"));
    setFixedWidth(320);
    setMaximumHeight(280);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_list = new QListWidget;
    m_list->setObjectName(QStringLiteral("completerList"));
    m_list->setFocusPolicy(Qt::StrongFocus);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget { background: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; "
        "border-radius: 8px; font-size: 13px; outline: none; padding: 4px; }"
        "QListWidget::item { padding: 7px 12px; border-radius: 4px; }"
        "QListWidget::item:hover { background: #313244; }"
        "QListWidget::item:selected { background: #45475a; color: #cba6f7; }"
    ));

    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, &WikiCompleter::onItemClicked);
}

void WikiCompleter::showAt(QPoint globalPos)
{
    // Ensure the popup stays within screen bounds
    QScreen *screen = QApplication::screenAt(globalPos);
    if (!screen) screen = QApplication::primaryScreen();
    QRect avail = screen->availableGeometry();

    int popupH = qMin(maximumHeight(), m_list->sizeHintForRow(0) * m_list->count() + 12);
    if (m_list->count() == 0) popupH = 0;

    // Position: prefer below cursor; go above if not enough room
    int x = globalPos.x();
    int y = globalPos.y() + 20;
    if (y + popupH > avail.bottom())
        y = globalPos.y() - popupH - 10;

    if (x + width() > avail.right())
        x = avail.right() - width();

    setGeometry(x, y, width(), popupH);
    show();
    m_list->setFocus();
}

void WikiCompleter::updateSuggestions(const QString &partial)
{
    m_list->clear();

    if (partial.isEmpty()) {
        hide();
        return;
    }

    // Find matching note titles
    QVector<NoteData> notes = m_repo->searchNotes(partial, 20);
    for (const NoteData &note : notes) {
        auto *item = new QListWidgetItem(
            QIcon(),                    // could use note icon here
            note.title.isEmpty() ? QStringLiteral("未命名") : note.title
        );
        item->setData(Qt::UserRole, note.id);
        m_list->addItem(item);
        if (m_list->count() >= 8) break;
    }

    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
        // Resize height to fit items
        int h = m_list->sizeHintForRow(0) * m_list->count() + 12;
        setFixedHeight(qMin(h, 280));
        show();
        m_list->setFocus();
    } else {
        hide();
    }
}

void WikiCompleter::selectNext()
{
    int row = m_list->currentRow();
    if (row < m_list->count() - 1)
        m_list->setCurrentRow(row + 1);
}

void WikiCompleter::selectPrevious()
{
    int row = m_list->currentRow();
    if (row > 0)
        m_list->setCurrentRow(row - 1);
}

QString WikiCompleter::currentTitle() const
{
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->text() : QString();
}

void WikiCompleter::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        selectPrevious();
        return;
    case Qt::Key_Down:
        selectNext();
        return;
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab: {
        QString title = currentTitle();
        if (!title.isEmpty()) {
            emit linkSelected(title);
            hide();
        }
        return;
    }
    case Qt::Key_Escape:
        hide();
        return;
    default:
        break;
    }
    // Forward other keys to the parent (editor)
    event->ignore();
    QWidget::keyPressEvent(event);
}

void WikiCompleter::focusOutEvent(QFocusEvent *event)
{
    hide();
    QWidget::focusOutEvent(event);
}

void WikiCompleter::onItemClicked(QListWidgetItem *item)
{
    if (item) {
        emit linkSelected(item->text());
        hide();
    }
}
