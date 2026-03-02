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

/// 成駒→元駒 (case-preserving)
char toBasePieceChar(char c) noexcept
{
    switch (c) {
    case 'Q': return 'P';  case 'q': return 'p';
    case 'M': return 'L';  case 'm': return 'l';
    case 'O': return 'N';  case 'o': return 'n';
    case 'T': return 'S';  case 't': return 's';
    case 'C': return 'B';  case 'c': return 'b';
    case 'U': return 'R';  case 'u': return 'r';
    default: return c;
    }
}

/// 駒台座標のRank（EngineMoveValidator互換）
int expectedHandRank(const EngineMoveValidator::Turn ownerTurn, char rawPiece) noexcept
{
    char piece = toBasePieceChar(rawPiece);

    if (ownerTurn == EngineMoveValidator::BLACK) {
        switch (piece) {
        case 'P': return 0;
        case 'L': return 1;
        case 'N': return 2;
        case 'S': return 3;
        case 'G': return 4;
        case 'B': return 5;
        case 'R': return 6;
        default: return -1;
        }
    }

    switch (piece) {
    case 'r': return 2;
    case 'b': return 3;
    case 'g': return 4;
    case 's': return 5;
    case 'n': return 6;
    case 'l': return 7;
    case 'p': return 8;
    default: return -1;
    }
}

/// 駒charが手番に属するか
bool belongsToTurn(const EngineMoveValidator::Turn turn, char c) noexcept
{
    if (turn == EngineMoveValidator::BLACK) {
        return c >= 'A' && c <= 'Z';
    }
    return c >= 'a' && c <= 'z';
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
                             const EngineMoveValidator::Turn& turn,
                             const EnginePosition& pos,
                             const ShogiMove& in)
{
    out = {};

    int toX = in.toSquare.x();
    int toY = in.toSquare.y();
    if (toX < 0 || toX >= kBoardSize || toY < 0 || toY >= kBoardSize) {
        return false;
    }
    Square toSq = toSquare(toX, toY);

    // capturedPiece が指定されている場合は盤面実体と一致を要求
    char capturedChar = static_cast<char>(in.capturedPiece);
    if (capturedChar != kEmpty) {
        char boardCaptured = pos.board[toSq];
        if (capturedChar != boardCaptured) {
            return false;
        }
    }

    int fromX = in.fromSquare.x();
    int fromY = in.fromSquare.y();

    // 打ち判定
    // EngineMoveValidator::BLACK_HAND_FILE = 9, WHITE_HAND_FILE = 10 は
    // ShogiMove の 0-indexed 座標系（盤上 0〜8、先手持ち駒 9、後手持ち駒 10）に対応する。
    // ShogiBoard/UI 座標系の 1-indexed（先手駒台 10、後手駒台 11）とは異なる。
    bool isDrop = (fromX == EngineMoveValidator::BLACK_HAND_FILE
                   || fromX == EngineMoveValidator::WHITE_HAND_FILE);

    if (isDrop) {
        // 手番と駒台ファイル座標の一致チェック
        int expectedFile = (turn == EngineMoveValidator::BLACK) ? EngineMoveValidator::BLACK_HAND_FILE
                                                                : EngineMoveValidator::WHITE_HAND_FILE;
        if (fromX != expectedFile) {
            return false;
        }

        char pieceChar = static_cast<char>(in.movingPiece);

        // 駒が手番に属するかチェック
        if (!belongsToTurn(turn, pieceChar)) {
            return false;
        }

        // 駒台Rank座標の一致チェック
        int expRank = expectedHandRank(turn, pieceChar);
        if (expRank < 0 || fromY != expRank) {
            return false;
        }

        PieceType pt = pieceToPieceType(in.movingPiece);
        if (pt == PieceType::PieceTypeNb) {
            return false;
        }

        // 持ち駒を実際に持っているかチェック
        Color sideColor = toColor(turn);
        HandType ht = pieceToHandType(pieceChar);
        if (ht == HandType::HandTypeNb) {
            return false;
        }
        int ci = static_cast<int>(sideColor);
        if (pos.hand[ci][static_cast<std::size_t>(ht)] <= 0) {
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
    if (fromX < 0 || fromX >= kBoardSize || fromY < 0 || fromY >= kBoardSize) {
        return false;
    }
    Square fromSq = toSquare(fromX, fromY);

    // 盤上の駒から実体を取得
    char boardChar = pos.board[fromSq];
    if (boardChar == kEmpty) {
        return false;
    }

    Color pieceColor{};
    PieceType pieceType{};
    if (!mapPieceChar(boardChar, pieceColor, pieceType)) {
        return false;
    }

    // 手番チェック
    Color expectedColor = toColor(turn);
    if (pieceColor != expectedColor) {
        return false;
    }

    // movingPiece が盤上の駒と一致するかチェック
    if (boardChar != static_cast<char>(in.movingPiece)) {
        return false;
    }

    out.nonPromote.kind = MoveKind::Board;
    out.nonPromote.from = fromSq;
    out.nonPromote.to = toSq;
    out.nonPromote.piece = pieceType;
    out.nonPromote.promote = false;

    // 成りバリアント
    if (isPromotable(pieceType)) {
        int fromRank = squareRank(fromSq);
        int toRank = squareRank(toSq);

        bool inPromotionZone = false;
        if (expectedColor == Color::Black) {
            inPromotionZone = (fromRank <= 2 || toRank <= 2);
        } else {
            inPromotionZone = (fromRank >= 6 || toRank >= 6);
        }

        if (inPromotionZone) {
            out.promote = out.nonPromote;
            out.promote.promote = true;
            // piece は元の駒種のまま（成りフラグで区別）
            out.hasPromoteVariant = true;
        }
    }

    return true;
}

Color Converter::toColor(const EngineMoveValidator::Turn& t) noexcept
{
    return (t == EngineMoveValidator::BLACK) ? Color::Black : Color::White;
}

} // namespace fmv
