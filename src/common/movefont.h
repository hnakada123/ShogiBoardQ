#ifndef MOVEFONT_H
#define MOVEFONT_H

/// @file movefont.h
/// @brief 指し手・読み筋の文字の書体（表のモデルの FontRole と KifuMoveDelegate が同じ規則を使う）

#include <QFont>
#include <QVariant>

#include "applicationfonts.h"
#include "kifupresentation.h"

namespace MoveFont {

/// 日本語表記なら、指し手を日本語棋譜の書体で書く
inline bool usesJapaneseFont()
{
    return KifuPresentation::options().notation == KifuPresentation::Notation::Japanese;
}

/// 指し手・読み筋の書体（日本語表記なら base の大きさ・太さを保った日本語棋譜の書体、英語表記なら base）
inline QFont font(const QFont& base)
{
    return usesJapaneseFont() ? ApplicationFonts::japaneseFont(base) : base;
}

/// 表のモデルの FontRole に返す値（英語表記なら値なし＝表の書体のまま）
inline QVariant role()
{
    return usesJapaneseFont() ? QVariant(ApplicationFonts::japaneseFont(QFont())) : QVariant();
}

} // namespace MoveFont

#endif // MOVEFONT_H
