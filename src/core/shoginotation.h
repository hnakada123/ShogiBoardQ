#ifndef SHOGINOTATION_H
#define SHOGINOTATION_H

/// @file shoginotation.h
/// @brief 棋譜の表記（筋・段の数字、駒の名前、CSA の駒種）の対応表
///
/// 対局の棋譜・読み筋・局面図・棋譜の読み書き・通信対局で同じ表を使う。ヘッダーだけで完結する。

#include <QChar>
#include <QString>
#include <QStringView>

#include "shogitypes.h"

namespace ShogiNotation {

// ============================================================
// 筋・段
// ============================================================

/// 全角数字の筋の1文字（1〜9 → '１'〜'９'。範囲外は空の QChar）
inline QChar fileZenkakuChar(int file)
{
    if (file < 1 || file > 9) return {};
    return QChar(u'０' + file);
}

/// 漢数字の段の1文字（1〜9 → '一'〜'九'。範囲外は空の QChar）
inline QChar rankKanjiChar(int rank)
{
    static const char16_t kanji[] = {u'一', u'二', u'三', u'四', u'五', u'六', u'七', u'八', u'九'};
    if (rank < 1 || rank > 9) return {};
    return QChar(kanji[rank - 1]);
}

/// 全角数字の筋（1〜9 → "１"〜"９"。範囲外は空文字列）
inline QString fileZenkaku(int file)
{
    if (file < 1 || file > 9) return {};
    return QString(fileZenkakuChar(file));
}

/// 漢数字の段（1〜9 → "一"〜"九"。範囲外は空文字列）
inline QString rankKanji(int rank)
{
    if (rank < 1 || rank > 9) return {};
    return QString(rankKanjiChar(rank));
}

/// 全角数字を1〜9にする（それ以外は0）
inline int fileFromZenkaku(QChar ch)
{
    const char16_t u = ch.unicode();
    return (u >= u'１' && u <= u'９') ? u - u'０' : 0;
}

/// 漢数字を1〜9にする（それ以外は0）
inline int rankFromKanji(QChar ch)
{
    switch (ch.unicode()) {
    case u'一': return 1; case u'二': return 2; case u'三': return 3;
    case u'四': return 4; case u'五': return 5; case u'六': return 6;
    case u'七': return 7; case u'八': return 8; case u'九': return 9;
    default: return 0;
    }
}

// ============================================================
// 駒の名前
// ============================================================

/// 指し手の表記に使う駒の名前（成香・成桂・成銀は2文字、王将は「玉」、竜王は「龍」）
inline QString movePieceName(Piece piece)
{
    switch (toBlack(piece)) {
    case Piece::BlackPawn:           return QStringLiteral("歩");
    case Piece::BlackLance:          return QStringLiteral("香");
    case Piece::BlackKnight:         return QStringLiteral("桂");
    case Piece::BlackSilver:         return QStringLiteral("銀");
    case Piece::BlackGold:           return QStringLiteral("金");
    case Piece::BlackBishop:         return QStringLiteral("角");
    case Piece::BlackRook:           return QStringLiteral("飛");
    case Piece::BlackKing:           return QStringLiteral("玉");
    case Piece::BlackPromotedPawn:   return QStringLiteral("と");
    case Piece::BlackPromotedLance:  return QStringLiteral("成香");
    case Piece::BlackPromotedKnight: return QStringLiteral("成桂");
    case Piece::BlackPromotedSilver: return QStringLiteral("成銀");
    case Piece::BlackHorse:          return QStringLiteral("馬");
    case Piece::BlackDragon:         return QStringLiteral("龍");
    default:                         return {};
    }
}

/// 局面図（BOD）・盤面の1文字の駒の名前（成香は「杏」、成桂は「圭」、成銀は「全」）
inline QString boardPieceName(Piece piece)
{
    switch (toBlack(piece)) {
    case Piece::BlackPromotedLance:  return QStringLiteral("杏");
    case Piece::BlackPromotedKnight: return QStringLiteral("圭");
    case Piece::BlackPromotedSilver: return QStringLiteral("全");
    default:                         return movePieceName(piece);
    }
}

/// SFEN の駒文字（先後どちらでも）と成りの有無から駒の名前（指し手の表記）
inline QString movePieceName(QChar sfenLetter, bool promoted)
{
    const Piece base = toBlack(charToPiece(sfenLetter.toUpper()));
    return movePieceName(promoted ? promote(base) : base);
}

/// 駒の名前（成香・杏・竜・王などの別表記も読む）を先手の駒にする。読めなければ None
inline Piece pieceFromName(QStringView name)
{
    static const struct { const char16_t* text; Piece piece; } table[] = {
        {u"成香", Piece::BlackPromotedLance}, {u"成桂", Piece::BlackPromotedKnight},
        {u"成銀", Piece::BlackPromotedSilver}, {u"歩", Piece::BlackPawn}, {u"香", Piece::BlackLance},
        {u"桂", Piece::BlackKnight}, {u"銀", Piece::BlackSilver}, {u"金", Piece::BlackGold},
        {u"角", Piece::BlackBishop}, {u"飛", Piece::BlackRook}, {u"玉", Piece::BlackKing},
        {u"王", Piece::BlackKing}, {u"と", Piece::BlackPromotedPawn}, {u"杏", Piece::BlackPromotedLance},
        {u"仝", Piece::BlackPromotedLance}, {u"圭", Piece::BlackPromotedKnight},
        {u"今", Piece::BlackPromotedKnight}, {u"全", Piece::BlackPromotedSilver},
        {u"馬", Piece::BlackHorse}, {u"龍", Piece::BlackDragon}, {u"竜", Piece::BlackDragon},
    };
    for (const auto& entry : table) {
        if (name == QStringView(entry.text)) return entry.piece;
    }
    return Piece::None;
}

// ============================================================
// CSA の駒種
// ============================================================

/// CSA の駒種（FU KY KE GI KI KA HI OU TO NY NK NG UM RY）。先後は持たない
inline QString csaPieceCode(Piece piece)
{
    switch (toBlack(piece)) {
    case Piece::BlackPawn:           return QStringLiteral("FU");
    case Piece::BlackLance:          return QStringLiteral("KY");
    case Piece::BlackKnight:         return QStringLiteral("KE");
    case Piece::BlackSilver:         return QStringLiteral("GI");
    case Piece::BlackGold:           return QStringLiteral("KI");
    case Piece::BlackBishop:         return QStringLiteral("KA");
    case Piece::BlackRook:           return QStringLiteral("HI");
    case Piece::BlackKing:           return QStringLiteral("OU");
    case Piece::BlackPromotedPawn:   return QStringLiteral("TO");
    case Piece::BlackPromotedLance:  return QStringLiteral("NY");
    case Piece::BlackPromotedKnight: return QStringLiteral("NK");
    case Piece::BlackPromotedSilver: return QStringLiteral("NG");
    case Piece::BlackHorse:          return QStringLiteral("UM");
    case Piece::BlackDragon:         return QStringLiteral("RY");
    default:                         return {};
    }
}

/// CSA の駒種を指定した側の駒にする。読めなければ None
inline Piece pieceFromCsaCode(QStringView code, Turn side = Turn::Black)
{
    static const struct { const char16_t* code; Piece piece; } table[] = {
        {u"FU", Piece::BlackPawn}, {u"KY", Piece::BlackLance}, {u"KE", Piece::BlackKnight},
        {u"GI", Piece::BlackSilver}, {u"KI", Piece::BlackGold}, {u"KA", Piece::BlackBishop},
        {u"HI", Piece::BlackRook}, {u"OU", Piece::BlackKing}, {u"TO", Piece::BlackPromotedPawn},
        {u"NY", Piece::BlackPromotedLance}, {u"NK", Piece::BlackPromotedKnight},
        {u"NG", Piece::BlackPromotedSilver}, {u"UM", Piece::BlackHorse}, {u"RY", Piece::BlackDragon},
    };
    for (const auto& entry : table) {
        if (code == QStringView(entry.code)) return side == Turn::Black ? entry.piece : toWhite(entry.piece);
    }
    return Piece::None;
}

} // namespace ShogiNotation

#endif // SHOGINOTATION_H
