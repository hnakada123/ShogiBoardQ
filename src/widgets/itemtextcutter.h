#ifndef ITEMTEXTCUTTER_H
#define ITEMTEXTCUTTER_H

/// @file itemtextcutter.h
/// @brief 表の項目の長い文字列を、省略表示（…）の結果を変えずに短く切る

#include <QFont>
#include <QHash>
#include <QString>

class QStyleOptionViewItem;

/**
 * @brief 表の項目に描く長い文字列を、表示幅より十分先で切る
 *
 * Qt の項目の描画は、右端を「…」で省略する前に、文字列全体の文字配置を何度も計算する。
 * 読み筋や棋譜のコメントは表示幅の数倍の長さになることがあり、描き直すたびに重い。
 *
 * 表示幅より十分先で切った文字列を渡しても、省略の位置（表示幅に収まる所まで）は変わらない。
 * 切る位置は文字列ごとに一度だけ計算して覚えておく。
 * 切ると行の高さが変わる（別のフォントで描く文字が後ろにだけある）場合や、改行・書字方向の
 * 制御文字を含む場合は切らない。
 */
class ItemTextCutter
{
public:
    /// 項目を描く設定の文字列を、Qt に渡す長さに切る。
    /// 折り返す・右端以外を省略する・右から左へ書く場合は、結果が変わりうるので切らない。
    void apply(QStyleOptionViewItem& option) const;

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

#endif // ITEMTEXTCUTTER_H
