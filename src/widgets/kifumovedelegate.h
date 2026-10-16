#ifndef KIFUMOVEDELEGATE_H
#define KIFUMOVEDELEGATE_H

#include "itemtextdelegate.h"
#include "movefont.h"

/// 指し手専用列の字形を選ぶ（表のモデルの FontRole と同じ規則。MoveFont）。ヘッダー・操作ボタン・文字サイズはUIの設定を使う。
/// 文字は配置を覚えて描く（ItemTextDelegate。見た目は Qt と同じ）。
class KifuMoveDelegate : public ItemTextDelegate
{
public:
    using ItemTextDelegate::ItemTextDelegate;

    // UIと棋譜の書体で高さが違っても、固定高の行で文字を切らない。
    static int textHeight(const QFont& uiFont)
    {
        return qMax(QFontMetrics(uiFont).height(), QFontMetrics(MoveFont::font(uiFont)).height());
    }
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (MoveFont::usesJapaneseFont()) {
            option->font = MoveFont::font(option->font);
            option->fontMetrics = QFontMetrics(option->font);
        }
    }
};

#endif
