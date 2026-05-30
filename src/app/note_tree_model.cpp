#include "note_tree_model.h"
#include "core/note_repository.h"

NoteTreeModel::NoteTreeModel(NoteRepository *repo, QObject *parent)
    : QAbstractItemModel(parent), m_repo(repo)
{
    m_root = new TreeNode;
    buildTree();
}

void NoteTreeModel::refresh()
{
    beginResetModel();
    delete m_root;
    m_root = new TreeNode;
    buildTree();
    endResetModel();
}

// ============================================================================
// Tree construction: pull data from NoteRepository and build the TreeNode tree
// ============================================================================

void NoteTreeModel::buildTree()
{
    // --- "All Notes" folder ---
    auto *noteFolder = new TreeNode;
    noteFolder->type  = NoteFolder;
    noteFolder->title = QStringLiteral("所有笔记");
    noteFolder->parent = m_root;

    const QVector<NoteData> notes = m_repo->listNotes(500, 0);
    for (const NoteData &n : notes) {
        auto *child = new TreeNode;
        child->type  = NoteItem;
        child->dbId  = n.id;
        child->title = n.title.isEmpty() ? QStringLiteral("未命名") : n.title;
        child->parent = noteFolder;
        noteFolder->children.append(child);
    }
    noteFolder->childCount = noteFolder->children.size();
    m_root->children.append(noteFolder);

    // --- "Tags" folder ---
    auto *tagFolder = new TreeNode;
    tagFolder->type  = TagFolder;
    tagFolder->title = QStringLiteral("标签");
    tagFolder->parent = m_root;

    const QVector<TagData> tags = m_repo->listTags();
    for (const TagData &t : tags) {
        auto *child = new TreeNode;
        child->type  = TagItem;
        child->dbId  = t.id;
        child->title = t.name;
        child->parent = tagFolder;
        tagFolder->children.append(child);
    }
    tagFolder->childCount = tagFolder->children.size();
    m_root->children.append(tagFolder);

    m_root->childCount = m_root->children.size();
}

// ============================================================================
// QAbstractItemModel interface
// ============================================================================

QModelIndex NoteTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeNode *parentNode = nodeFromIndex(parent);
    if (!parentNode || row >= parentNode->children.size())
        return QModelIndex();

    TreeNode *child = parentNode->children.at(row);
    return createIndex(row, column, child);
}

QModelIndex NoteTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeNode *node = static_cast<TreeNode*>(index.internalPointer());
    TreeNode *parentNode = node->parent;

    if (!parentNode || parentNode == m_root)
        return QModelIndex();

    // Find parent's row position under grandparent
    TreeNode *grandparent = parentNode->parent;
    if (!grandparent)
        return QModelIndex();

    int row = grandparent->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int NoteTreeModel::rowCount(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    return node ? node->children.size() : 0;
}

int NoteTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NoteTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    TreeNode *node = static_cast<TreeNode*>(index.internalPointer());

    if (role == Qt::DisplayRole)
        return node->title;

    if (role == Qt::UserRole)
        return node->dbId;

    // DecorationRole: icons for folders
    if (role == Qt::DecorationRole) {
        // We'll handle icons in the view's delegate or via QIcon in main_window
        return {};
    }

    return {};
}

// ============================================================================
// Convenience accessors
// ============================================================================

NoteTreeModel::TreeNode *NoteTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<TreeNode*>(index.internalPointer());
    return m_root;
}

qint64 NoteTreeModel::noteIdForIndex(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return (node && node->type == NoteItem) ? node->dbId : -1;
}

qint64 NoteTreeModel::tagIdForIndex(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return (node && node->type == TagItem) ? node->dbId : -1;
}

NoteTreeModel::NodeType NoteTreeModel::nodeTypeForIndex(const QModelIndex &index) const
{
    TreeNode *node = nodeFromIndex(index);
    return node ? node->type : Root;
}
