/// @file pvtextdelegate.cpp
/// @brief 読み筋の列を描くデリゲートの実装

#include "pvtextdelegate.h"

void PvTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    // QStyledItemDelegate::paint と同じく項目の設定を作り、描くのは ItemTextPainter に任せる
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    m_painter.paint(painter, opt);
}
