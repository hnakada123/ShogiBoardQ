#ifndef ITEMTEXTDELEGATE_H
#define ITEMTEXTDELEGATE_H

/// @file itemtextdelegate.h
/// @brief 項目の文字を、文字配置を覚えて描くデリゲート（見た目は QStyledItemDelegate と同じ）

#include "itemtextpainter.h"

#include <QStyledItemDelegate>

/**
 * @brief QStyledItemDelegate と同じ見た目で、項目の文字を ItemTextPainter に描かせるデリゲート
 *
 * Qt の項目の描画は、セルを描くたびに、文字の大きさを測る・省略（…）を決める・描くための配置と、
 * 文字配置を何度も計算する。ここでは枠・背景・選択の色を Qt に描かせ、文字は配置を一度だけ
 * 求めて描く（同じ文字列を描き直すときは、覚えた配置を使う）。
 *
 * 文字色はパレットの Text（選択中は HighlightedText）を使うので、スタイルシートの ::item で
 * 文字色を変えない表に付ける（ItemTextPainter を参照）。
 */
class ItemTextDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

protected:
    const ItemTextPainter& textPainter() const { return m_painter; }

private:
    ItemTextPainter m_painter;
};

#endif // ITEMTEXTDELEGATE_H
