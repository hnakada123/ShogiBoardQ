/// @file itemtextdelegate.cpp
/// @brief 項目の文字を、文字配置を覚えて描くデリゲートの実装

#include "itemtextdelegate.h"

void ItemTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    // QStyledItemDelegate::paint と同じく項目の設定を作り、描くのは ItemTextPainter に任せる
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    m_painter.paint(painter, opt);
}
