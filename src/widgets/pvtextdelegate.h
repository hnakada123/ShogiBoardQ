#ifndef PVTEXTDELEGATE_H
#define PVTEXTDELEGATE_H

/// @file pvtextdelegate.h
/// @brief 読み筋の列を描くデリゲート（長い文字列の描画を軽くする）

#include "itemtextpainter.h"

#include <QStyledItemDelegate>

/**
 * @brief 読み筋のように表示幅より長くなりがちな文字列を描くデリゲート
 *
 * 省略表示（…）した後の文字列と、その文字配置を文字列ごとに覚えて描く（ItemTextPainter）。
 * 枠・背景・選択の色は Qt が描き、見た目は Qt の項目の描画と同じ。
 * 大きさの目安（sizeHint）は全文のまま計算する。
 */
class PvTextDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// 表示幅 width のセルに text を描くとき、Qt に渡す文字列（切る必要がなければ text のまま）
    QString textForWidth(const QString& text, const QFont& font, int width) const
    {
        return m_painter.textForWidth(text, font, width);
    }

private:
    ItemTextPainter m_painter;
};

#endif // PVTEXTDELEGATE_H
