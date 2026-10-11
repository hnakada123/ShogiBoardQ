/// @file pvtextdelegate.cpp
/// @brief 読み筋の列を描くデリゲートの実装

#include "pvtextdelegate.h"

#include <QApplication>
#include <QStyle>

void PvTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    // QStyledItemDelegate::paint と同じ手順で、渡す文字列だけを切る
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    m_cutter.apply(opt);
    const QWidget* widget = opt.widget;
    const QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
}
