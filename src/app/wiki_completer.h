#pragma once

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class NoteRepository;

/// A frameless popup that appears below the cursor when the user types [[
/// in the editor. It shows matching note titles from the repository.
///
/// Usage:
///   1. Call showAt(globalCursorPos) to display it.
///   2. Call updateSuggestions(partialText, allTitles) to filter the list.
///   3. User presses Enter or clicks an item → linkSelected(title) emitted.
///   4. Call selectNext() / selectPrevious() for keyboard navigation.
class WikiCompleter : public QWidget
{
    Q_OBJECT

public:
    explicit WikiCompleter(NoteRepository *repo, QWidget *parent = nullptr);

    /// Show the popup at a screen-global position (typically below cursor).
    void showAt(QPoint globalPos);

    /// Filter suggestions by partial text typed after [[.
    void updateSuggestions(const QString &partial);

    /// Select the next / previous item in the list.
    void selectNext();
    void selectPrevious();

    /// Return the currently selected title, or empty string.
    QString currentTitle() const;

signals:
    /// Emitted when the user confirms a selection (Enter or click).
    void linkSelected(const QString &title);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget *m_list = nullptr;
    NoteRepository *m_repo = nullptr;
};
