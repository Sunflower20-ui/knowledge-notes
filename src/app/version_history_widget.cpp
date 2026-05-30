#include "version_history_widget.h"
#include "core/version_repository.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QDateTime>

VersionHistoryWidget::VersionHistoryWidget(VersionRepository *repo, QWidget *parent)
    : QWidget(parent), m_repo(repo)
{
    setObjectName(QStringLiteral("versionHistoryWidget"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // Header
    m_headerLabel = new QLabel(QStringLiteral("\u5386\u53f2\u7248\u672c"));
    m_headerLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #cdd6f4; font-size: 15px; font-weight: bold; }"
    ));
    layout->addWidget(m_headerLabel);

    // Version list
    m_versionList = new QListWidget;
    m_versionList->setStyleSheet(QStringLiteral(
        "QListWidget { background: #181825; color: #cdd6f4; border: 1px solid #45475a; "
        "border-radius: 6px; font-size: 13px; }"
        "QListWidget::item { padding: 6px 8px; }"
        "QListWidget::item:selected { background: #45475a; }"
        "QListWidget::item:hover { background: #313244; }"
    ));
    m_versionList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_versionList, &QListWidget::currentRowChanged, this, &VersionHistoryWidget::onVersionSelected);
    connect(m_versionList, &QListWidget::customContextMenuRequested, this, &VersionHistoryWidget::onContextMenu);
    layout->addWidget(m_versionList, 1);

    // Buttons row
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);

    m_restoreBtn = new QPushButton(QStringLiteral("\u6062\u590d\u6b64\u7248\u672c"));
    m_restoreBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #45475a; color: #cdd6f4; border: none; "
        "border-radius: 6px; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background: #585b70; }"
    ));
    connect(m_restoreBtn, &QPushButton::clicked, this, &VersionHistoryWidget::onRestoreClicked);
    btnLayout->addWidget(m_restoreBtn);

    m_clearBtn = new QPushButton(QStringLiteral("\u6e05\u7a7a\u5386\u53f2"));
    m_clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3a1f1f; color: #f38ba8; border: none; "
        "border-radius: 6px; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background: #502525; }"
    ));
    connect(m_clearBtn, &QPushButton::clicked, this, &VersionHistoryWidget::onClearHistory);
    btnLayout->addWidget(m_clearBtn);

    layout->addLayout(btnLayout);

    // Diff view
    m_diffView = new QTextBrowser;
    m_diffView->setStyleSheet(QStringLiteral(
        "QTextBrowser { background: #11111b; color: #cdd6f4; border: 1px solid #45475a; "
        "border-radius: 6px; font-size: 13px; }"
    ));
    m_diffView->setOpenExternalLinks(false);
    layout->addWidget(m_diffView, 2);
}

void VersionHistoryWidget::loadNote(qint64 noteId)
{
    m_noteId = noteId;
    refreshList();
}

void VersionHistoryWidget::refreshList()
{
    m_versionList->clear();
    m_versionIds.clear();
    m_diffView->clear();

    if (!m_repo || m_noteId <= 0) {
        m_headerLabel->setText(QStringLiteral("\u5386\u53f2\u7248\u672c"));
        return;
    }

    QVector<VersionData> versions = m_repo->listVersions(m_noteId);
    m_headerLabel->setText(QStringLiteral("\u5386\u53f2\u7248\u672c (%1)").arg(versions.size()));

    for (const VersionData &v : versions) {
        QString label = QStringLiteral("v%1  %2")
            .arg(v.versionNumber)
            .arg(v.createdAt.toString("MM-dd hh:mm"));
        m_versionList->addItem(label);
        m_versionIds.append(v.id);
    }
}

void VersionHistoryWidget::onVersionSelected(int row)
{
    if (row < 0 || row >= m_versionIds.size()) {
        m_diffView->clear();
        return;
    }

    qint64 versionId = m_versionIds[row];
    VersionData version = m_repo->getVersion(versionId);
    VersionData latest = m_repo->latestVersion(m_noteId);

    QVector<DiffLine> diff = DiffUtil::compute(version.content, latest.content);
    m_diffView->setHtml(DiffUtil::toHtml(diff));
}

void VersionHistoryWidget::onRestoreClicked()
{
    int row = m_versionList->currentRow();
    if (row < 0 || row >= m_versionIds.size()) return;
    if (!m_repo || m_noteId <= 0) return;

    auto answer = QMessageBox::question(this,
        QStringLiteral("\u6062\u590d\u7248\u672c"),
        QStringLiteral("\u786e\u5b9a\u8981\u6062\u590d\u5230\u9009\u4e2d\u7684\u5386\u53f2\u7248\u672c\u5417\uff1f\u5f53\u524d\u5185\u5bb9\u5c06\u88ab\u8986\u76d6\u3002"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    qint64 versionId = m_versionIds[row];
    if (m_repo->rollbackToVersion(m_noteId, versionId)) {
        refreshList();
        m_diffView->setHtml(QStringLiteral(
            "<div style='color: #a6e3a1; padding: 16px; text-align: center;'>"
            "\u2705 \u5df2\u6062\u590d\u5230\u5386\u53f2\u7248\u672c</div>"));
        emit versionRestored(m_noteId);
    }
}

void VersionHistoryWidget::onDeleteVersion()
{
    int row = m_versionList->currentRow();
    if (row < 0 || row >= m_versionIds.size()) return;

    auto answer = QMessageBox::question(this,
        QStringLiteral("\u5220\u9664\u7248\u672c"),
        QStringLiteral("\u786e\u5b9a\u8981\u5220\u9664\u8fd9\u4e2a\u5386\u53f2\u7248\u672c\u5417\uff1f\u6b64\u64cd\u4f5c\u4e0d\u53ef\u64a4\u9500\u3002"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    m_repo->deleteVersion(m_versionIds[row]);
    refreshList();
}

void VersionHistoryWidget::onClearHistory()
{
    if (!m_repo || m_noteId <= 0) return;

    auto answer = QMessageBox::question(this,
        QStringLiteral("\u6e05\u7a7a\u5386\u53f2"),
        QStringLiteral("\u786e\u5b9a\u8981\u6e05\u7a7a\u6240\u6709\u5386\u53f2\u7248\u672c\u5417\uff1f\u5c06\u4fdd\u7559\u6700\u65b0\u4e00\u4e2a\u7248\u672c\u3002"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    m_repo->clearHistory(m_noteId, 1);
    refreshList();
}

void VersionHistoryWidget::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_versionList->itemAt(pos);
    if (!item) return;
    m_versionList->setCurrentItem(item);

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #1e1e2e; color: #cdd6f4; border: 1px solid #45475a; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background: #45475a; }"
    ));

    QAction *deleteAction = menu.addAction(QStringLiteral("\u5220\u9664\u6b64\u7248\u672c"));
    connect(deleteAction, &QAction::triggered, this, &VersionHistoryWidget::onDeleteVersion);

    menu.exec(m_versionList->viewport()->mapToGlobal(pos));
}
