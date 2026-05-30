#pragma once

#include <QWidget>

class QListWidget;
class QTextBrowser;
class QPushButton;
class QLabel;
class VersionRepository;

class VersionHistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VersionHistoryWidget(VersionRepository *repo, QWidget *parent = nullptr);

    void loadNote(qint64 noteId);

signals:
    void versionRestored(qint64 noteId);

private slots:
    void onVersionSelected(int row);
    void onRestoreClicked();
    void onDeleteVersion();
    void onClearHistory();
    void onContextMenu(const QPoint &pos);

private:
    void refreshList();

    VersionRepository *m_repo;
    qint64 m_noteId = -1;

    QLabel *m_headerLabel = nullptr;
    QListWidget *m_versionList = nullptr;
    QTextBrowser *m_diffView = nullptr;
    QPushButton *m_restoreBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;

    QVector<qint64> m_versionIds;
};
