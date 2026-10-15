/// @file fmvconverter.cpp
/// @brief Qt型 ←→ エンジン内部型の境界変換

#include "fmvconverter.h"

#include <cctype>

namespace fmv {

namespace {

constexpr char kEmpty = ' ';

/// Piece enum → {Color, PieceType} マッピング
struct PieceMapping {
    Color color;
    PieceType type;
};

/// Piece charから内部PieceTypeへの変換（Color付き）
bool mapPieceChar(char c, Color& outColor, PieceType& outType) noexcept
{
    if (c >= 'A' && c <= 'Z') {
        outColor = Color::Black;
    } else if (c >= 'a' && c <= 'z') {
        outColor = Color::White;
    } else {
        return false;
    }

    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (upper) {
    case 'P': outType = PieceType::Pawn;      return true;
    case 'L': outType = PieceType::Lance;     return true;
    case 'N': outType = PieceType::Knight;    return true;
    case 'S': outType = PieceType::Silver;    return true;
    case 'G': outType = PieceType::Gold;      return true;
    case 'B': outType = PieceType::Bishop;    return true;
    case 'R': outType = PieceType::Rook;      return true;
    case 'K': outType = PieceType::King;      return true;
    case 'Q': outType = PieceType::ProPawn;   return true;
    case 'M': outType = PieceType::ProLance;  return true;
    case 'O': outType = PieceType::ProKnight; return true;
    case 'T': outType = PieceType::ProSilver; return true;
    case 'C': outType = PieceType::Horse;     return true;
    case 'U': outType = PieceType::Dragon;    return true;
    default: return false;
    }
}

/// Piece enum値からHandTypeへの変換
HandType pieceToHandType(char c) noexcept
{
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    // 成駒を元に戻す
    switch (upper) {
    case 'Q': upper = 'P'; break;
    case 'M': upper = 'L'; break;
    case 'O': upper = 'N'; break;
    case 'T': upper = 'S'; break;
    case 'C': upper = 'B'; break;
    case 'U': upper = 'R'; break;
    default: break;
    }

    switch (upper) {
    case 'P': return HandType::Pawn;
    case 'L': return HandType::Lance;
    case 'N': return HandType::Knight;
    case 'S': return HandType::Silver;
    case 'G': return HandType::Gold;
    case 'B': return HandType::Bishop;
    case 'R': return HandType::Rook;
    default: return HandType::HandTypeNb;
    }
}

/// ShogiMoveから内部PieceTypeを推定
PieceType pieceToPieceType(Piece p) noexcept
{
    char c = static_cast<char>(p);
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (upper) {
    case 'P': return PieceType::Pawn;
    case 'L': return PieceType::Lance;
    case 'N': return PieceType::Knight;
    case 'S': return PieceType::Silver;
    case 'G': return PieceType::Gold;
    case 'B': return PieceType::Bishop;
    case 'R': return PieceType::Rook;
    case 'K': return PieceType::King;
    case 'Q': return PieceType::ProPawn;
    case 'M': return PieceType::ProLance;
    case 'O': return PieceType::ProKnight;
    case 'T': return PieceType::ProSilver;
    case 'C': return PieceType::Horse;
    case 'U': return PieceType::Dragon;
    default: return PieceType::PieceTypeNb;
    }
}

/// 打てる駒種を先手の生駒にする（打てない駒種は None）
Piece pieceTypeToDropPiece(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::Pawn:   return Piece::BlackPawn;
    case PieceType::Lance:  return Piece::BlackLance;
    case PieceType::Knight: return Piece::BlackKnight;
    case PieceType::Silver: return Piece::BlackSilver;
    case PieceType::Gold:   return Piece::BlackGold;
    case PieceType::Bishop: return Piece::BlackBishop;
    case PieceType::Rook:   return Piece::BlackRook;
    default: return Piece::None;
    }
}

/// 成り可能な駒種かどうか
bool isPromotable(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::Pawn:
    case PieceType::Lance:
    case PieceType::Knight:
    case PieceType::Silver:
    case PieceType::Bishop:
    case PieceType::Rook:
        return true;
    default:
        return false;
    }
}

} // namespace

bool Converter::toEnginePosition(EnginePosition& out,
                                 const QList<Piece>& boardData,
                                 const QMap<Piece, int>& pieceStand)
{
    out.clear();

    // 盤面変換
    int boardSize = std::min(static_cast<int>(boardData.size()), kSquareNb);
    for (int i = 0; i < boardSize; ++i) {
        out.board[static_cast<std::size_t>(i)] = static_cast<char>(boardData[static_cast<qsizetype>(i)]);
    }

    // 持ち駒変換
    for (auto it = pieceStand.constBegin(); it != pieceStand.constEnd(); ++it) {
        char c = static_cast<char>(it.key());
        Color color = (c >= 'A' && c <= 'Z') ? Color::Black : Color::White;
        HandType ht = pieceToHandType(c);
        if (ht != HandType::HandTypeNb) {
            int ci = static_cast<int>(color);
            out.hand[ci][static_cast<std::size_t>(ht)] = std::max(0, it.value());
        }
    }

    // ビットボード構築
    out.rebuildBitboards();
    return true;
}

bool Converter::toEngineMove(ConvertedMove& out,
                             Turn turn,
                             const EnginePosition& pos,
                             const UsiMove& move)
{
    out = {};
    if (!move.to.isValid()) {
        return false;
    }
    const Square toSq = toSquare(move.to.file - 1, move.to.rank - 1);

    const Color sideColor = toColor(turn);
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(turn);
        const PieceType pt = pieceToPieceType(piece);
        const HandType ht = pieceToHandType(static_cast<char>(piece));
        if (pt == PieceType::PieceTypeNb || ht == HandType::HandTypeNb) {
            return false;
        }
        if (pos.hand[static_cast<std::size_t>(sideColor)][static_cast<std::size_t>(ht)] <= 0) {
            return false;
        }
        out.nonPromote.kind = MoveKind::Drop;
        out.nonPromote.from = kInvalidSquare;
        out.nonPromote.to = toSq;
        out.nonPromote.piece = pt;
        out.nonPromote.promote = false;
        out.hasPromoteVariant = false;
        return true;
    }

    // 盤上移動
    if (!move.from.isValid()) {
        return false;
    }
    const Square fromSq = toSquare(move.from.file - 1, move.from.rank - 1);

    // 盤上の駒から実体を取得
    const char boardChar = pos.board[fromSq];
    if (boardChar == kEmpty) {
        return false;
    }

    Color pieceColor{};
    PieceType pieceType{};
    if (!mapPieceChar(boardChar, pieceColor, pieceType) || pieceColor != sideColor) {
        return false;
    }

    out.nonPromote.kind = MoveKind::Board;
    out.nonPromote.from = fromSq;
    out.nonPromote.to = toSq;
    out.nonPromote.piece = pieceType;
    out.nonPromote.promote = false;

    // 成りバリアント
    if (isPromotable(pieceType)) {
        const int fromRank = squareRank(fromSq);
        const int toRank = squareRank(toSq);
        const bool inPromotionZone = (sideColor == Color::Black)
            ? (fromRank <= 2 || toRank <= 2)
            : (fromRank >= 6 || toRank >= 6);
        if (inPromotionZone) {
            out.promote = out.nonPromote;
            out.promote.promote = true;
            // piece は元の駒種のまま（成りフラグで区別）
            out.hasPromoteVariant = true;
        }
    }

    return true;
}

Color Converter::toColor(Turn t) noexcept
{
    return (t == Turn::Black) ? Color::Black : Color::White;
}

UsiMove Converter::toUsiMove(const Move& m)
{
    UsiMove move;
    move.to = BoardSquare{squareFile(m.to) + 1, squareRank(m.to) + 1};
    if (m.kind == MoveKind::Drop) {
        move.drop = pieceTypeToDropPiece(m.piece);
        return move;
    }
    move.from = BoardSquare{squareFile(m.from) + 1, squareRank(m.from) + 1};
    move.promote = m.promote;
    return move;
}

} // namespace fmv
