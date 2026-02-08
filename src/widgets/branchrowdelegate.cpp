/// @file branchrowdelegate.cpp
/// @brief 分岐行描画デリゲートクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "branchrowdelegate.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyle>

BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void BranchRowDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    // 選択時のハイライトと衝突しないように、未選択時のみオレンジ背景
    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, QColor(255, 220, 160));
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}
