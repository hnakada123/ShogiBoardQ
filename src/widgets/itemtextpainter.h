#ifndef ITEMTEXTPAINTER_H
#define ITEMTEXTPAINTER_H

/// @file itemtextpainter.h
/// @brief 表の項目の長い文字列を、Qt と同じ見た目のまま、配置を覚えて描く

#include "itemtextcutter.h"

#include <QFont>
#include <QSize>
#include <QString>
#include <memory>
#include <unordered_map>

class QPainter;
class QStyle;
class QStyleOptionViewItem;
class QTextLayout;

/**
 * @brief 表の項目（1行・右端を省略する文字列）を描く
 *
 * Qt の項目の描画（QCommonStyle の CE_ItemViewItem）は、セルを描くたびに、文字の配置を
 * 省略（…）の判定・省略後の文字列・描くための配置と何度も計算する。読み筋のように長い文字列では、
 * セルの描画のほとんどがこの計算になる。
 *
 * ここでは枠・背景・選択の色は Qt に描かせ（文字を空にして渡す）、文字だけを Qt と同じ手順で描く。
 * 省略後の文字列と、その文字配置（QTextLayout）、縦の位置を決める行の高さは、文字列ごとに一度だけ
 * 求めて覚える。描く関数（QTextLayout::draw）・位置・文字色・切り抜きは Qt と同じなので、画素も同じになる。
 *
 * - 文字色はパレットの Text（選択中は HighlightedText）。スタイルシートの ::item で文字色を
 *   変えない表で使う（思考表・検討タブ・棋譜解析の表の TableStyles::thinking は、選択中の色を
 *   selection-color と同じにしている）。
 * - Qt に渡すのと同じく、文字列は先に表示幅の十分先で切る（ItemTextCutter）。
 * - 自分で描けない設定（折り返し・右端以外の省略・右から左・アイコンやチェック・フォーカスの枠・
 *   改行や書字方向の制御文字を含む文字列など）は、切った文字列を Qt に渡して描かせる。
 */
class ItemTextPainter
{
public:
    ItemTextPainter();
    ~ItemTextPainter();  // QTextLayout は .cpp でだけ完全な型になる

    /// option（initStyleOption 済み）の項目を描く
    void paint(QPainter* painter, const QStyleOptionViewItem& option) const;

    /// 表示幅 width のセルに text を描くとき、Qt に渡す文字列（切る必要がなければ text のまま）
    QString textForWidth(const QString& text, const QFont& font, int width) const
    {
        return m_cutter.textForWidth(text, font, width);
    }

private:
    /// 文字列ごとに覚えておく、描くための準備
    struct Prepared {
        QFont font;
        int cellWidth = 0;      ///< 表示幅より先で切るときに使ったセルの幅
        int lineWidth = 0;      ///< 文字を並べる幅（省略の判定に使う）
        int alignment = 0;      ///< 項目の文字の配置
        QSize textSize;         ///< Qt が縦の位置を決めるのに使う、切った文字列の配置の大きさ
        std::unique_ptr<QTextLayout> layout;  ///< 省略後の文字列の配置（描くだけにする）
    };

    /// 自分で文字を描ける設定か
    static bool canDrawText(const QStyleOptionViewItem& option);

    const Prepared& prepare(const QStyleOptionViewItem& option, int lineWidth) const;

    ItemTextCutter m_cutter;
    mutable std::unordered_map<QString, Prepared> m_prepared;
};

#endif // ITEMTEXTPAINTER_H
