#pragma once

#include <QAbstractItemModel>
#include <QVector>

class NoteRepository;
struct NoteData;
struct TagData;

/// Custom tree model that bridges NoteRepository (SQLite) into a QTreeView.
///
/// Modes:
///   Normal  → [All Notes] + [Tags]
///   Search  → [Search Results]  (when filter text is set)
///   Tag     → [Tag: name]       (when tag ID is set)
class NoteTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum NodeType {
        Root        = 0,
        NoteFolder  = 1,
        NoteItem    = 2,
        TagFolder   = 3,
        TagItem     = 4,
        SearchFolder = 5,
        TagLabel    = 6
    };

    explicit NoteTreeModel(NoteRepository *repo, QObject *parent = nullptr);

    /// Reload all data from the repository (respects current filter).
    void refresh();

    /// Set a search filter; empty string restores normal mode.
    void setSearchFilter(const QString &text);

    /// Set a tag filter; -1 restores normal mode.
    void setTagFilter(qint64 tagId);

    /// Current mode: "" = normal, non-empty = searching, tag = filtering
    QString currentFilter() const { return m_searchFilter; }
    qint64 currentTagFilter() const { return m_tagFilterId; }

    // --- QAbstractItemModel interface ---
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    qint64 noteIdForIndex(const QModelIndex &index) const;
    qint64 tagIdForIndex(const QModelIndex &index) const;
    NodeType nodeTypeForIndex(const QModelIndex &index) const;

private:
    struct TreeNode {
        NodeType type = Root;
        qint64   dbId = -1;
        QString  title;
        int      childCount = 0;
        TreeNode *parent = nullptr;
        QVector<TreeNode*> children;
        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode *nodeFromIndex(const QModelIndex &index) const;
    void buildTree();

    NoteRepository *m_repo;
    TreeNode *m_root = nullptr;
    QString m_searchFilter;
    qint64 m_tagFilterId = -1;
};
