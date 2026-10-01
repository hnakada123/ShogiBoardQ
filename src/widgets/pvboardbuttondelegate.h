/// @file pvboardbuttondelegate.h
/// @brief 読み筋の盤面表示セルを控えめなボタンとして描画する

#ifndef PVBOARDBUTTONDELEGATE_H
#define PVBOARDBUTTONDELEGATE_H

#include "tablestyles.h"

#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>

class PvBoardButtonDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        const QString label = cell.text;
        cell.text.clear();
        const QStyle* style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);

        const bool emphasized = cell.state & (QStyle::State_MouseOver | QStyle::State_Selected
                                               | QStyle::State_HasFocus);
        const QRect rect = cell.rect.adjusted(4, 2, -4, -2);
        painter->save();
        painter->setClipRect(cell.rect);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(emphasized ? TableStyles::selectionAccent() : QColor(0xcc, 0xd4, 0xdc));
        painter->setBrush(emphasized ? TableStyles::selectionBackground() : QColor(0xf7, 0xf8, 0xfa));
        painter->drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        painter->setFont(cell.font);
        painter->setPen(TableStyles::selectionText());
        painter->drawText(rect, Qt::AlignCenter,
                          cell.fontMetrics.elidedText(label, Qt::ElideRight, rect.width() - 4));
        painter->restore();
    }
};

#endif // PVBOARDBUTTONDELEGATE_H
