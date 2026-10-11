#ifndef PVTEXTDELEGATE_H
#define PVTEXTDELEGATE_H

/// @file pvtextdelegate.h
/// @brief 読み筋の列を描くデリゲート（長い文字列の描画を軽くする）

#include "itemtextcutter.h"

#include <QStyledItemDelegate>

/**
 * @brief 読み筋のように表示幅より長くなりがちな文字列を描くデリゲート
 *
 * 表示幅より十分先で切った文字列を Qt に渡し、省略表示（…）の前の文字配置の計算を減らす
 * （ItemTextCutter）。見た目は全文を渡した場合と同じ。大きさの目安（sizeHint）は全文のまま計算する。
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
        return m_cutter.textForWidth(text, font, width);
    }

private:
    ItemTextCutter m_cutter;
};

#endif // PVTEXTDELEGATE_H
