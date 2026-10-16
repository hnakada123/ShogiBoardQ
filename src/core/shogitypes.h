#ifndef SHOGITYPES_H
#define SHOGITYPES_H

/// @file shogitypes.h
/// @brief 将棋の基本型定義（手番enum、駒enum等）

#include <QString>
#include <QLatin1Char>
#include <QDebug>
#include <optional>
#include <cstdint>

/// 手番を表す型安全な列挙型
enum class Turn { Black, White };

/// Turn → SFEN手番文字列（"b" or "w"）
inline QString turnToSfen(Turn t) { return t == Turn::Black ? QStringLiteral("b") : QStringLiteral("w"); }

/// SFEN手番文字列 → Turn
inline Turn sfenToTurn(const QString& s) { return s == QStringLiteral("b") ? Turn::Black : Turn::White; }

/// 相手の手番
inline Turn opponent(Turn t) { return t == Turn::Black ? Turn::White : Turn::Black; }

/// ログ出力用（"Black" / "White"）
inline QDebug operator<<(QDebug debug, Turn t)
{
    QDebugStateSaver saver(debug);
    return debug.nospace() << (t == Turn::Black ? "Black" : "White");
}

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

/**
 * @brief 駒の種類（色を持たない）
 *
 * 並びは合法手生成（fmv::PieceType）と同じ。生駒の8種（玉まで）の後に、成駒6種を元の駒と同じ順に置く。
 * 持ち駒の欄の番号は、生駒の種類の値（歩0〜玉7）をそのまま使う。
 */
enum class PieceKind : std::uint8_t {
    Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, King,
    ProPawn, ProLance, ProKnight, ProSilver, Horse, Dragon,
};
/// 駒の種類の数
inline constexpr int kPieceKindCount = 14;
/// 持ち駒（駒台）に置ける駒の種類の数（歩〜玉。局面編集では玉も駒台に置ける）
inline constexpr int kHandKindCount = 8;

/**
 * @brief 駒（色と種類）
 *
 * 値は「種類の値（PieceKind）＋ 後手なら 16」。成る・戻す・先後の反転は値の計算で行う。
 * None は駒のないマス。SFEN の文字との変換は charToPiece・pieceLetter・pieceSfen で行う。
 */
enum class Piece : std::uint8_t {
    // 先手
    BlackPawn = 0, BlackLance, BlackKnight, BlackSilver, BlackGold, BlackBishop, BlackRook, BlackKing,
    BlackPromotedPawn, BlackPromotedLance, BlackPromotedKnight, BlackPromotedSilver, BlackHorse, BlackDragon,
    // 後手
    WhitePawn = 16, WhiteLance, WhiteKnight, WhiteSilver, WhiteGold, WhiteBishop, WhiteRook, WhiteKing,
    WhitePromotedPawn, WhitePromotedLance, WhitePromotedKnight, WhitePromotedSilver, WhiteHorse, WhiteDragon,
    // 空
    None = 0xFF
};

namespace PieceCode {
inline constexpr std::uint8_t kWhiteBit = 16;   ///< 後手の駒の印
inline constexpr std::uint8_t kKindMask = 15;   ///< 種類の部分
inline constexpr std::uint8_t value(Piece p) { return static_cast<std::uint8_t>(p); }
} // namespace PieceCode

/// 色と種類から駒を作る
constexpr Piece makePiece(Turn side, PieceKind kind)
{
    return static_cast<Piece>(static_cast<std::uint8_t>(kind) | (side == Turn::White ? PieceCode::kWhiteBit : 0));
}

/// 駒の種類（None に対しては呼ばない）
constexpr PieceKind pieceKind(Piece p)
{
    return static_cast<PieceKind>(PieceCode::value(p) & PieceCode::kKindMask);
}

/// 先手の駒か判定
constexpr bool isBlackPiece(Piece p)
{
    return p != Piece::None && (PieceCode::value(p) & PieceCode::kWhiteBit) == 0;
}

/// 後手の駒か判定
constexpr bool isWhitePiece(Piece p)
{
    return p != Piece::None && (PieceCode::value(p) & PieceCode::kWhiteBit) != 0;
}

/// 駒の持ち主（None に対しては呼ばない）
constexpr Turn pieceSide(Piece p) { return isWhitePiece(p) ? Turn::White : Turn::Black; }

/// 成駒か判定
constexpr bool isPromoted(Piece p)
{
    return p != Piece::None && pieceKind(p) >= PieceKind::ProPawn;
}

/// 成れる駒か（歩・香・桂・銀・角・飛の生駒）
constexpr bool isPromotable(Piece p)
{
    if (p == Piece::None) return false;
    const PieceKind kind = pieceKind(p);
    return kind <= PieceKind::Rook && kind != PieceKind::Gold;
}

/// 駒を成る（成れない駒はそのまま返す）。歩〜銀は 8、角・飛は 7 先が成駒
constexpr Piece promote(Piece p)
{
    if (!isPromotable(p)) return p;
    return static_cast<Piece>(PieceCode::value(p) + (pieceKind(p) <= PieceKind::Silver ? 8 : 7));
}

/// 成駒を元に戻す（非成駒はそのまま返す）
constexpr Piece demote(Piece p)
{
    if (!isPromoted(p)) return p;
    return static_cast<Piece>(PieceCode::value(p) - (pieceKind(p) <= PieceKind::ProSilver ? 8 : 7));
}

/// 後手駒を先手駒に変換（成り状態を保持、先手駒・Noneはそのまま）
constexpr Piece toBlack(Piece p)
{
    return p == Piece::None ? p : static_cast<Piece>(PieceCode::value(p) & ~PieceCode::kWhiteBit);
}

/// 先手駒を後手駒に変換（成り状態を保持、後手駒・Noneはそのまま）
constexpr Piece toWhite(Piece p)
{
    return p == Piece::None ? p : static_cast<Piece>(PieceCode::value(p) | PieceCode::kWhiteBit);
}

/// 先後を入れ替えた駒（Noneはそのまま）
constexpr Piece flipSide(Piece p)
{
    return p == Piece::None ? p : static_cast<Piece>(PieceCode::value(p) ^ PieceCode::kWhiteBit);
}

/// 指定の側の駒にする（成り状態を保持）
constexpr Piece toSide(Piece p, Turn side) { return side == Turn::Black ? toBlack(p) : toWhite(p); }

/// 取った駒を持ち駒にしたときの駒（相手の色の生駒）
constexpr Piece capturedToHand(Piece captured) { return flipSide(demote(captured)); }

/// SFEN の駒の文字（PLNSGBRK。大文字は先手、小文字は後手）を生駒にする。それ以外は None
inline Piece charToPiece(QChar ch)
{
    PieceKind kind;
    switch (ch.toUpper().unicode()) {
    case u'P': kind = PieceKind::Pawn; break;
    case u'L': kind = PieceKind::Lance; break;
    case u'N': kind = PieceKind::Knight; break;
    case u'S': kind = PieceKind::Silver; break;
    case u'G': kind = PieceKind::Gold; break;
    case u'B': kind = PieceKind::Bishop; break;
    case u'R': kind = PieceKind::Rook; break;
    case u'K': kind = PieceKind::King; break;
    default: return Piece::None;
    }
    return makePiece(ch.isLower() ? Turn::White : Turn::Black, kind);
}

/// 駒の SFEN の文字（成駒は元の駒の文字。大文字は先手、小文字は後手。None は空白）
inline QChar pieceLetter(Piece p)
{
    if (p == Piece::None) return QLatin1Char(' ');
    static constexpr char kLetters[] = "PLNSGBRK";
    const char letter = kLetters[static_cast<int>(pieceKind(demote(p)))];
    return QLatin1Char(isWhitePiece(p) ? static_cast<char>(letter - 'A' + 'a') : letter);
}

/// 駒の SFEN の表記（成駒は "+P" の形。None は空）
inline QString pieceSfen(Piece p)
{
    if (p == Piece::None) return {};
    return isPromoted(p) ? QStringLiteral("+") + pieceLetter(p) : QString(pieceLetter(p));
}

/// ログ出力用（SFEN の表記。None は "-"）
inline QDebug operator<<(QDebug debug, Piece p)
{
    QDebugStateSaver saver(debug);
    return debug.noquote().nospace() << (p == Piece::None ? QStringLiteral("-") : pieceSfen(p));
}

#endif // SHOGITYPES_H
