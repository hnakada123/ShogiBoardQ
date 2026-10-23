/// @file josekitableview.cpp
/// @brief 定跡ウィンドウの定跡手の表の実装

#include "josekitableview.h"

#include "josekitablemodel.h"
#include "kifumovedelegate.h"
#include "tablebuttondelegate.h"

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

JosekiTableView::JosekiTableView(JosekiTableModel *model, QWidget *parent)
    : QTableView(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("josekiTable"));
    setModel(m_model);
    auto *moveDelegate = new KifuMoveDelegate(this);
    setItemDelegateForColumn(JosekiTableModel::Move, moveDelegate);
    setItemDelegateForColumn(JosekiTableModel::NextMove, moveDelegate);
    // 「着手」「編集」「削除」はボタンとして描き、押した列で処理を決める
    auto *buttonDelegate = new TableButtonDelegate(this);
    for (int column : {JosekiTableModel::Play, JosekiTableModel::Edit, JosekiTableModel::Delete})
        setItemDelegateForColumn(column, buttonDelegate);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);
    setShowGrid(false);
    setWordWrap(false);
    setMouseTracking(true);
    verticalHeader()->setVisible(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
    const int defaultWidths[JosekiTableModel::ColumnCount - 1] = {40, 50, 120, 120, 50, 50, 70, 50, 80, 60};
    for (int col = 0; col < JosekiTableModel::ColumnCount - 1; ++col) setColumnWidth(col, defaultWidths[col]);
    horizontalHeader()->setStretchLastSection(true);

    m_emptyGuide = new QLabel(viewport());
    m_emptyGuide->setObjectName(QStringLiteral("josekiEmptyGuide"));
    m_emptyGuide->setWordWrap(true);
    m_emptyGuide->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_emptyGuide->setAlignment(Qt::AlignCenter);
    m_emptyGuide->setVisible(false);
    auto *emptyLayout = new QVBoxLayout(viewport());
    emptyLayout->addWidget(m_emptyGuide);

    m_menu = new QMenu(this);
    addMenuAction(tr("着手"), RowAction::Play);
    m_menu->addSeparator();
    addMenuAction(tr("編集..."), RowAction::Edit);
    addMenuAction(tr("削除"), RowAction::Delete);
    m_menu->addSeparator();
    addMenuAction(tr("先頭へ移動"), RowAction::MoveToTop);
    addMenuAction(tr("上へ移動"), RowAction::MoveUp);
    addMenuAction(tr("下へ移動"), RowAction::MoveDown);
    addMenuAction(tr("出現頻度の順に並べる"), RowAction::SortByFrequency);
    addMenuAction(tr("評価値の順に並べる"), RowAction::SortByValue);
    m_menu->addSeparator();
    addMenuAction(tr("指し手をコピー"), RowAction::CopyMove);

    connect(this, &QTableView::clicked, this, &JosekiTableView::onClicked);
    connect(this, &QTableView::doubleClicked, this, &JosekiTableView::onDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &JosekiTableView::onContextMenuRequested);
    connect(m_menu, &QMenu::triggered, this, &JosekiTableView::onMenuTriggered);
}

QAction *JosekiTableView::addMenuAction(const QString &text, RowAction rowAction)
{
    QAction *added = m_menu->addAction(text);
    added->setData(static_cast<int>(rowAction));
    return added;
}

QAction *JosekiTableView::action(RowAction rowAction) const
{
    const QList<QAction *> actions = m_menu->actions();
    for (QAction *candidate : actions) {
        if (!candidate->isSeparator() && candidate->data().toInt() == static_cast<int>(rowAction)) return candidate;
    }
    return nullptr;
}

void JosekiTableView::setActionAvailability(std::function<bool(JosekiTableView::RowAction, int)> available)
{
    m_available = std::move(available);
}

void JosekiTableView::updateMenuActions(int row)
{
    const QList<QAction *> actions = m_menu->actions();
    for (QAction *candidate : actions) {
        if (candidate->isSeparator()) continue;
        const auto rowAction = static_cast<RowAction>(candidate->data().toInt());
        candidate->setEnabled(!m_available || m_available(rowAction, row));
    }
}

void JosekiTableView::setEmptyGuide(const QString &text)
{
    m_emptyGuide->setText(text);
    m_emptyGuide->setVisible(!text.isEmpty());
    if (!text.isEmpty()) m_emptyGuide->raise();
}

void JosekiTableView::onClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    switch (index.column()) {
    case JosekiTableModel::Play:
        // 人間が指せないときは「着手」のボタンを描かない
        if (m_model->humanCanPlay()) emit rowActionRequested(RowAction::Play, index.row());
        break;
    case JosekiTableModel::Edit: emit rowActionRequested(RowAction::Edit, index.row()); break;
    case JosekiTableModel::Delete: emit rowActionRequested(RowAction::Delete, index.row()); break;
    default: break;
    }
}

void JosekiTableView::onDoubleClicked(const QModelIndex &index)
{
    // ボタンの列は1回目のクリックで処理済み
    const int column = index.column();
    if (!index.isValid() || column == JosekiTableModel::Play || column == JosekiTableModel::Edit
        || column == JosekiTableModel::Delete) {
        return;
    }
    emit rowActionRequested(RowAction::Play, index.row());
}

void JosekiTableView::onContextMenuRequested(const QPoint &pos)
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) return;
    m_menuRow = index.row();
    updateMenuActions(m_menuRow);
    selectRow(m_menuRow);
    m_menu->exec(viewport()->mapToGlobal(pos));
}

void JosekiTableView::onMenuTriggered(QAction *triggered)
{
    if (!triggered || m_menuRow < 0) return;
    emit rowActionRequested(static_cast<RowAction>(triggered->data().toInt()), m_menuRow);
}
