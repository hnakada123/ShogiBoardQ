#ifndef SHOGITYPES_H
#define SHOGITYPES_H

/// @file shogitypes.h
/// @brief 将棋の基本型定義（手番enum、駒enum等）

#include <QString>
#include <QLatin1Char>
#include <optional>

/// 手番を表す型安全な列挙型
enum class Turn { Black, White };

/// Turn → SFEN手番文字列（"b" or "w"）
inline QString turnToSfen(Turn t) { return t == Turn::Black ? QStringLiteral("b") : QStringLiteral("w"); }

/// SFEN手番文字列 → Turn
inline Turn sfenToTurn(const QString& s) { return s == QStringLiteral("b") ? Turn::Black : Turn::White; }

/// 相手番を返す
inline Turn oppositeTurn(Turn t) { return t == Turn::Black ? Turn::White : Turn::Black; }

/// SFEN文字列をパースした結果を保持する構造体
struct SfenComponents {
    QString board;
    QString stand;
    Turn turn;
    int moveNumber;
};

/// bestmove の特殊手を表す列挙型（エンジンからの受信用）
enum class SpecialMove { None, Resign, Win, Draw };

/// bestmove 文字列を SpecialMove に変換する（通常手は None を返す）
inline SpecialMove parseSpecialMove(const QString& s)
{
    if (s.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) return SpecialMove::Resign;
    if (s.compare(QStringLiteral("win"),    Qt::CaseInsensitive) == 0) return SpecialMove::Win;
    if (s.compare(QStringLiteral("draw"),   Qt::CaseInsensitive) == 0) return SpecialMove::Draw;
    return SpecialMove::None;
}

/// gameover コマンドの結果を表す列挙型（エンジンへの送信用）
enum class GameOverResult { Win, Lose, Draw };

/// GameOverResult を USI gameover コマンド用の文字列に変換する
inline QString gameOverResultToString(GameOverResult r)
{
    switch (r) {
    case GameOverResult::Win:  return QStringLiteral("win");
    case GameOverResult::Lose: return QStringLiteral("lose");
    case GameOverResult::Draw: return QStringLiteral("draw");
    }
    return QStringLiteral("draw");
}

// ============================================================
// 駒型定義
// ============================================================

/// 駒を表す型安全な列挙型
/// 文字値は内部SFEN表現に対応（大文字=先手、小文字=後手）
enum class Piece : char {
    // 先手（大文字）
    BlackPawn = 'P', BlackLance = 'L', BlackKnight = 'N', BlackSilver = 'S',
    BlackGold = 'G', BlackBishop = 'B', BlackRook = 'R', BlackKing = 'K',
    BlackPromotedPawn = 'Q', BlackPromotedLance = 'M', BlackPromotedKnight = 'O',
    BlackPromotedSilver = 'T', BlackHorse = 'C', BlackDragon = 'U',
    // 後手（小文字）
    WhitePawn = 'p', WhiteLance = 'l', WhiteKnight = 'n', WhiteSilver = 's',
    WhiteGold = 'g', WhiteBishop = 'b', WhiteRook = 'r', WhiteKing = 'k',
    WhitePromotedPawn = 'q', WhitePromotedLance = 'm', WhitePromotedKnight = 'o',
    WhitePromotedSilver = 't', WhiteHorse = 'c', WhiteDragon = 'u',
    // 空
    None = ' '
};

/// QChar → Piece 変換（無効文字は None）
inline Piece charToPiece(QChar ch)
{
    switch (ch.unicode()) {
    case 'P': return Piece::BlackPawn;
    case 'L': return Piece::BlackLance;
    case 'N': return Piece::BlackKnight;
    case 'S': return Piece::BlackSilver;
    case 'G': return Piece::BlackGold;
    case 'B': return Piece::BlackBishop;
    case 'R': return Piece::BlackRook;
    case 'K': return Piece::BlackKing;
    case 'Q': return Piece::BlackPromotedPawn;
    case 'M': return Piece::BlackPromotedLance;
    case 'O': return Piece::BlackPromotedKnight;
    case 'T': return Piece::BlackPromotedSilver;
    case 'C': return Piece::BlackHorse;
    case 'U': return Piece::BlackDragon;
    case 'p': return Piece::WhitePawn;
    case 'l': return Piece::WhiteLance;
    case 'n': return Piece::WhiteKnight;
    case 's': return Piece::WhiteSilver;
    case 'g': return Piece::WhiteGold;
    case 'b': return Piece::WhiteBishop;
    case 'r': return Piece::WhiteRook;
    case 'k': return Piece::WhiteKing;
    case 'q': return Piece::WhitePromotedPawn;
    case 'm': return Piece::WhitePromotedLance;
    case 'o': return Piece::WhitePromotedKnight;
    case 't': return Piece::WhitePromotedSilver;
    case 'c': return Piece::WhiteHorse;
    case 'u': return Piece::WhiteDragon;
    case ' ': return Piece::None;
    default:  return Piece::None;
    }
}

/// Piece → QChar 変換
inline QChar pieceToChar(Piece p)
{
    return QLatin1Char(static_cast<char>(p));
}

/// 先手の駒か判定
inline bool isBlackPiece(Piece p)
{
    const char c = static_cast<char>(p);
    return c >= 'A' && c <= 'Z';
}

/// 後手の駒か判定
inline bool isWhitePiece(Piece p)
{
    const char c = static_cast<char>(p);
    return c >= 'a' && c <= 'z';
}

/// 成駒か判定
inline bool isPromoted(Piece p)
{
    switch (p) {
    case Piece::BlackPromotedPawn:    case Piece::BlackPromotedLance:
    case Piece::BlackPromotedKnight:  case Piece::BlackPromotedSilver:
    case Piece::BlackHorse:           case Piece::BlackDragon:
    case Piece::WhitePromotedPawn:    case Piece::WhitePromotedLance:
    case Piece::WhitePromotedKnight:  case Piece::WhitePromotedSilver:
    case Piece::WhiteHorse:           case Piece::WhiteDragon:
        return true;
    default:
        return false;
    }
}

/// 駒を成る（成れない駒はそのまま返す）
inline Piece promote(Piece p)
{
    switch (p) {
    case Piece::BlackPawn:   return Piece::BlackPromotedPawn;
    case Piece::BlackLance:  return Piece::BlackPromotedLance;
    case Piece::BlackKnight: return Piece::BlackPromotedKnight;
    case Piece::BlackSilver: return Piece::BlackPromotedSilver;
    case Piece::BlackBishop: return Piece::BlackHorse;
    case Piece::BlackRook:   return Piece::BlackDragon;
    case Piece::WhitePawn:   return Piece::WhitePromotedPawn;
    case Piece::WhiteLance:  return Piece::WhitePromotedLance;
    case Piece::WhiteKnight: return Piece::WhitePromotedKnight;
    case Piece::WhiteSilver: return Piece::WhitePromotedSilver;
    case Piece::WhiteBishop: return Piece::WhiteHorse;
    case Piece::WhiteRook:   return Piece::WhiteDragon;
    default: return p;
    }
}

/// 成駒を元に戻す（非成駒はそのまま返す）
inline Piece demote(Piece p)
{
    switch (p) {
    case Piece::BlackPromotedPawn:    return Piece::BlackPawn;
    case Piece::BlackPromotedLance:   return Piece::BlackLance;
    case Piece::BlackPromotedKnight:  return Piece::BlackKnight;
    case Piece::BlackPromotedSilver:  return Piece::BlackSilver;
    case Piece::BlackHorse:           return Piece::BlackBishop;
    case Piece::BlackDragon:          return Piece::BlackRook;
    case Piece::WhitePromotedPawn:    return Piece::WhitePawn;
    case Piece::WhitePromotedLance:   return Piece::WhiteLance;
    case Piece::WhitePromotedKnight:  return Piece::WhiteKnight;
    case Piece::WhitePromotedSilver:  return Piece::WhiteSilver;
    case Piece::WhiteHorse:           return Piece::WhiteBishop;
    case Piece::WhiteDragon:          return Piece::WhiteRook;
    default: return p;
    }
}

/// 後手駒を先手駒に変換（成り状態を保持、先手駒・Noneはそのまま）
inline Piece toBlack(Piece p)
{
    switch (p) {
    case Piece::WhitePawn:           return Piece::BlackPawn;
    case Piece::WhiteLance:          return Piece::BlackLance;
    case Piece::WhiteKnight:         return Piece::BlackKnight;
    case Piece::WhiteSilver:         return Piece::BlackSilver;
    case Piece::WhiteGold:           return Piece::BlackGold;
    case Piece::WhiteBishop:         return Piece::BlackBishop;
    case Piece::WhiteRook:           return Piece::BlackRook;
    case Piece::WhiteKing:           return Piece::BlackKing;
    case Piece::WhitePromotedPawn:   return Piece::BlackPromotedPawn;
    case Piece::WhitePromotedLance:  return Piece::BlackPromotedLance;
    case Piece::WhitePromotedKnight: return Piece::BlackPromotedKnight;
    case Piece::WhitePromotedSilver: return Piece::BlackPromotedSilver;
    case Piece::WhiteHorse:          return Piece::BlackHorse;
    case Piece::WhiteDragon:         return Piece::BlackDragon;
    default: return p;
    }
}

/// 先手駒を後手駒に変換（成り状態を保持、後手駒・Noneはそのまま）
inline Piece toWhite(Piece p)
{
    switch (p) {
    case Piece::BlackPawn:           return Piece::WhitePawn;
    case Piece::BlackLance:          return Piece::WhiteLance;
    case Piece::BlackKnight:         return Piece::WhiteKnight;
    case Piece::BlackSilver:         return Piece::WhiteSilver;
    case Piece::BlackGold:           return Piece::WhiteGold;
    case Piece::BlackBishop:         return Piece::WhiteBishop;
    case Piece::BlackRook:           return Piece::WhiteRook;
    case Piece::BlackKing:           return Piece::WhiteKing;
    case Piece::BlackPromotedPawn:   return Piece::WhitePromotedPawn;
    case Piece::BlackPromotedLance:  return Piece::WhitePromotedLance;
    case Piece::BlackPromotedKnight: return Piece::WhitePromotedKnight;
    case Piece::BlackPromotedSilver: return Piece::WhitePromotedSilver;
    case Piece::BlackHorse:          return Piece::WhiteHorse;
    case Piece::BlackDragon:         return Piece::WhiteDragon;
    default: return p;
    }
}

#endif // SHOGITYPES_H
