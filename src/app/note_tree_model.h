#pragma once

#include <QAbstractItemModel>
#include <QVector>

class NoteRepository;
struct NoteData;
struct TagData;

/// Custom tree model that bridges NoteRepository (SQLite) into a QTreeView.
///
/// Tree structure:
///   [root - invisible]
///     ├── "All Notes"  (folder)
///     │     ├── Note A  (leaf, stores noteId)
///     │     └── Note B
///     └── "Tags"       (folder)
///           ├── tag-1 (3 notes)
///           └── tag-2 (1 note)
///
/// Each node stores its type (Root / NoteFolder / Note / TagFolder / Tag)
/// and the associated database ID in Qt::UserRole.
class NoteTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum NodeType {
        Root        = 0,
        NoteFolder  = 1,
        NoteItem    = 2,
        TagFolder   = 3,
        TagItem     = 4
    };

    explicit NoteTreeModel(NoteRepository *repo, QObject *parent = nullptr);

    /// Reload all data from the repository.
    void refresh();

    // --- QAbstractItemModel interface ---
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /// Convenience: get note/tag ID from an index.
    qint64 noteIdForIndex(const QModelIndex &index) const;
    qint64 tagIdForIndex(const QModelIndex &index) const;
    NodeType nodeTypeForIndex(const QModelIndex &index) const;

private:
    struct TreeNode {
        NodeType type = Root;
        qint64   dbId = -1;        // noteId or tagId (meaningful for NoteItem, TagItem)
        QString  title;
        int      childCount = 0;   // only for folders: how many children
        TreeNode *parent = nullptr;
        QVector<TreeNode*> children;
        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode *nodeFromIndex(const QModelIndex &index) const;
    void buildTree();

    NoteRepository *m_repo;
    TreeNode *m_root = nullptr;
};
