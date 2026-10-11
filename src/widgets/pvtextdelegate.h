#ifndef PVTEXTDELEGATE_H
#define PVTEXTDELEGATE_H

/// @file pvtextdelegate.h
/// @brief 読み筋の列を描くデリゲート（長い文字列の描画を軽くする）

#include <QHash>
#include <QStyledItemDelegate>

/**
 * @brief 読み筋のように表示幅より長くなりがちな文字列を描くデリゲート
 *
 * Qt の項目の描画は、右端を「…」で省略する前に、文字列全体の文字配置を何度も計算する。
 * 読み筋は表示幅の数倍の長さになることがあり、思考情報が届くたびの描き直しで重い。
 *
 * 表示幅より十分先で切った文字列を渡しても、省略の位置（表示幅に収まる所まで）は変わらない。
 * そこで、切る位置を文字列ごとに一度だけ計算して覚えておき、描くときは切った文字列を渡す。
 * 切ると行の高さが変わる（別のフォントで描く文字が後ろにだけある）場合は切らない。
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
    QString textForWidth(const QString& text, const QFont& font, int width) const;

private:
    struct Cut {
        QString fontKey;
        int width = 0;
        int length = -1;  ///< 切る位置（-1 なら切らない）
    };
    mutable QHash<QString, Cut> m_cuts;
};

#endif // PVTEXTDELEGATE_H
