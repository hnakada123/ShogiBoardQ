#ifndef KIFUMOVEDELEGATE_H
#define KIFUMOVEDELEGATE_H

#include "applicationfonts.h"
#include "kifupresentation.h"
#include <QStyledItemDelegate>

/// 指し手専用列の字形を選ぶ。ヘッダー・操作ボタン・文字サイズはUIの設定を使う。
class KifuMoveDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    // UIと棋譜の書体で高さが違っても、固定高の行で文字を切らない。
    static int textHeight(const QFont& uiFont)
    {
        const int height = QFontMetrics(uiFont).height();
        return KifuPresentation::options().notation == KifuPresentation::Notation::Japanese
            ? qMax(height, QFontMetrics(ApplicationFonts::japaneseFont(uiFont)).height()) : height;
    }
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (KifuPresentation::options().notation == KifuPresentation::Notation::Japanese) {
            option->font = ApplicationFonts::japaneseFont(option->font);
            option->fontMetrics = QFontMetrics(option->font);
        }
    }
};

#endif
