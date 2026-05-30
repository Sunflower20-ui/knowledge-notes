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

void NoteTreeModel::setSearchFilter(const QString &text)
{
    m_searchFilter = text.trimmed();
    m_tagFilterId = -1;
    refresh();
}

void NoteTreeModel::setTagFilter(qint64 tagId)
{
    m_tagFilterId = tagId;
    m_searchFilter.clear();
    refresh();
}

// ============================================================================
// Tree construction
// ============================================================================

void NoteTreeModel::buildTree()
{
    // --- Mode 1: Search ---
    if (!m_searchFilter.isEmpty()) {
        auto *folder = new TreeNode;
        folder->type  = SearchFolder;
        folder->title = QStringLiteral("搜索结果: %1").arg(m_searchFilter);
        folder->parent = m_root;

        // Use FTS5 for full-text search
        QVector<::SearchResult> results = m_repo->searchFts(m_searchFilter, 30);
        // Also try LIKE search as fallback
        if (results.isEmpty()) {
            QVector<NoteData> notes = m_repo->searchNotes(m_searchFilter, 30);
            for (const NoteData &n : notes) {
                auto *child = new TreeNode;
                child->type  = NoteItem;
                child->dbId  = n.id;
                child->title = n.title.isEmpty() ? QStringLiteral("未命名") : n.title;
                child->parent = folder;
                folder->children.append(child);
            }
        } else {
            for (const ::SearchResult &sr : results) {
                auto *child = new TreeNode;
                child->type  = NoteItem;
                child->dbId  = sr.noteId;
                child->title = sr.title.isEmpty() ? QStringLiteral("未命名") : sr.title;
                child->parent = folder;
                folder->children.append(child);
            }
        }
        folder->childCount = folder->children.size();
        m_root->children.append(folder);
        m_root->childCount = 1;
        return;
    }

    // --- Mode 2: Tag filter ---
    if (m_tagFilterId > 0) {
        TagData tag = m_repo->getTag(m_tagFilterId);
        auto *folder = new TreeNode;
        folder->type  = TagLabel;
        folder->title = QStringLiteral("标签: %1").arg(tag.name.isEmpty() ? QStringLiteral("?") : tag.name);
        folder->parent = m_root;

        QVector<NoteData> notes = m_repo->notesForTag(m_tagFilterId);
        for (const NoteData &n : notes) {
            auto *child = new TreeNode;
            child->type  = NoteItem;
            child->dbId  = n.id;
            child->title = n.title.isEmpty() ? QStringLiteral("未命名") : n.title;
            child->parent = folder;
            folder->children.append(child);
        }
        folder->childCount = folder->children.size();
        m_root->children.append(folder);
        m_root->childCount = 1;
        return;
    }

    // --- Mode 3: Normal ---
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

    auto *tagFolder = new TreeNode;
    tagFolder->type  = TagFolder;
    tagFolder->title = QStringLiteral("标签");
    tagFolder->parent = m_root;

    const QVector<TagData> tags = m_repo->listTags();
    for (const TagData &t : tags) {
        auto *child = new TreeNode;
        child->type  = TagItem;
        child->dbId  = t.id;
        child->title = QStringLiteral("%1 (%2)").arg(t.name).arg(m_repo->notesForTag(t.id).size());
        child->parent = tagFolder;
        tagFolder->children.append(child);
    }
    tagFolder->childCount = tagFolder->children.size();
    m_root->children.append(tagFolder);

    m_root->childCount = m_root->children.size();
}

// ============================================================================
// QAbstractItemModel
// ============================================================================

QModelIndex NoteTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeNode *parentNode = nodeFromIndex(parent);
    if (!parentNode || row >= parentNode->children.size())
        return QModelIndex();

    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex NoteTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();
    TreeNode *node = static_cast<TreeNode*>(index.internalPointer());
    TreeNode *parentNode = node->parent;
    if (!parentNode || parentNode == m_root) return QModelIndex();
    TreeNode *grandparent = parentNode->parent;
    if (!grandparent) return QModelIndex();
    int row = grandparent->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int NoteTreeModel::rowCount(const QModelIndex &parent) const
{
    TreeNode *node = nodeFromIndex(parent);
    return node ? node->children.size() : 0;
}

int NoteTreeModel::columnCount(const QModelIndex &) const { return 1; }

QVariant NoteTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};
    TreeNode *node = static_cast<TreeNode*>(index.internalPointer());

    if (role == Qt::DisplayRole)
        return node->title;
    if (role == Qt::UserRole)
        return node->dbId;

    return {};
}

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
