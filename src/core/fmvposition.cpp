/// @file fmvposition.cpp
/// @brief エンジン内部局面のdo/undo実装

#include "fmvposition.h"

#include <cctype>

namespace fmv {

namespace {

constexpr char kEmpty = ' ';

struct PieceInfo {
    Color color = Color::Black;
    PieceType type = PieceType::PieceTypeNb;
    bool valid = false;
};

PieceInfo charToPieceInfo(char c) noexcept
{
    PieceInfo info;
    if (c >= 'A' && c <= 'Z') {
        info.color = Color::Black;
    } else if (c >= 'a' && c <= 'z') {
        info.color = Color::White;
    } else {
        return info; // invalid
    }

    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (upper) {
    case 'P': info.type = PieceType::Pawn;      break;
    case 'L': info.type = PieceType::Lance;     break;
    case 'N': info.type = PieceType::Knight;    break;
    case 'S': info.type = PieceType::Silver;    break;
    case 'G': info.type = PieceType::Gold;      break;
    case 'B': info.type = PieceType::Bishop;    break;
    case 'R': info.type = PieceType::Rook;      break;
    case 'K': info.type = PieceType::King;      break;
    case 'Q': info.type = PieceType::ProPawn;   break;
    case 'M': info.type = PieceType::ProLance;  break;
    case 'O': info.type = PieceType::ProKnight; break;
    case 'T': info.type = PieceType::ProSilver; break;
    case 'C': info.type = PieceType::Horse;     break;
    case 'U': info.type = PieceType::Dragon;    break;
    default: return info; // invalid
    }

    info.valid = true;
    return info;
}

char pieceInfoToChar(Color color, PieceType type) noexcept
{
    char c = kEmpty;
    switch (type) {
    case PieceType::Pawn:      c = 'P'; break;
    case PieceType::Lance:     c = 'L'; break;
    case PieceType::Knight:    c = 'N'; break;
    case PieceType::Silver:    c = 'S'; break;
    case PieceType::Gold:      c = 'G'; break;
    case PieceType::Bishop:    c = 'B'; break;
    case PieceType::Rook:      c = 'R'; break;
    case PieceType::King:      c = 'K'; break;
    case PieceType::ProPawn:   c = 'Q'; break;
    case PieceType::ProLance:  c = 'M'; break;
    case PieceType::ProKnight: c = 'O'; break;
    case PieceType::ProSilver: c = 'T'; break;
    case PieceType::Horse:     c = 'C'; break;
    case PieceType::Dragon:    c = 'U'; break;
    default: return kEmpty;
    }
    if (color == Color::White) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return c;
}

PieceType promotePieceType(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::Pawn:   return PieceType::ProPawn;
    case PieceType::Lance:  return PieceType::ProLance;
    case PieceType::Knight: return PieceType::ProKnight;
    case PieceType::Silver: return PieceType::ProSilver;
    case PieceType::Bishop: return PieceType::Horse;
    case PieceType::Rook:   return PieceType::Dragon;
    default: return pt;
    }
}

PieceType demotePieceType(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::ProPawn:   return PieceType::Pawn;
    case PieceType::ProLance:  return PieceType::Lance;
    case PieceType::ProKnight: return PieceType::Knight;
    case PieceType::ProSilver: return PieceType::Silver;
    case PieceType::Horse:     return PieceType::Bishop;
    case PieceType::Dragon:    return PieceType::Rook;
    default: return pt;
    }
}

HandType pieceTypeToHandType(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::Pawn:      case PieceType::ProPawn:   return HandType::Pawn;
    case PieceType::Lance:     case PieceType::ProLance:  return HandType::Lance;
    case PieceType::Knight:    case PieceType::ProKnight: return HandType::Knight;
    case PieceType::Silver:    case PieceType::ProSilver: return HandType::Silver;
    case PieceType::Gold:      return HandType::Gold;
    case PieceType::Bishop:    case PieceType::Horse:     return HandType::Bishop;
    case PieceType::Rook:      case PieceType::Dragon:    return HandType::Rook;
    default: return HandType::HandTypeNb;
    }
}

} // namespace

void EnginePosition::clear() noexcept
{
    board.fill(kEmpty);
    occupied = {};
    colorOcc[0] = {};
    colorOcc[1] = {};
    for (int c = 0; c < 2; ++c) {
        for (int pt = 0; pt < static_cast<int>(PieceType::PieceTypeNb); ++pt) {
            pieceOcc[c][pt] = {};
        }
        hand[c].fill(0);
    }
    kingSq[0] = kInvalidSquare;
    kingSq[1] = kInvalidSquare;
    zobristKey = 0ULL;
}

void EnginePosition::rebuildBitboards() noexcept
{
    occupied = {};
    colorOcc[0] = {};
    colorOcc[1] = {};
    for (int c = 0; c < 2; ++c) {
        for (int pt = 0; pt < static_cast<int>(PieceType::PieceTypeNb); ++pt) {
            pieceOcc[c][pt] = {};
        }
    }
    kingSq[0] = kInvalidSquare;
    kingSq[1] = kInvalidSquare;

    for (int sq = 0; sq < kSquareNb; ++sq) {
        char c = board[static_cast<std::size_t>(sq)];
        if (c == kEmpty) {
            continue;
        }

        PieceInfo info = charToPieceInfo(c);
        if (!info.valid) {
            continue;
        }

        auto s = static_cast<Square>(sq);
        int ci = static_cast<int>(info.color);

        occupied.set(s);
        colorOcc[ci].set(s);
        pieceOcc[ci][static_cast<int>(info.type)].set(s);

        if (info.type == PieceType::King) {
            kingSq[ci] = s;
        }
    }
}

bool EnginePosition::doMove(const Move& move, Color side, UndoState& undo) noexcept
{
    int ci = static_cast<int>(side);

    // UndoState保存
    undo.move = move;
    undo.kingSquareBefore[0] = kingSq[0];
    undo.kingSquareBefore[1] = kingSq[1];
    undo.zobristBefore = zobristKey;

    if (move.kind == MoveKind::Drop) {
        // 打ち
        Square to = move.to;
        undo.movedPieceBefore = kEmpty;
        undo.capturedPiece = kEmpty;

        // 持ち駒を減らす
        HandType ht = pieceTypeToHandType(move.piece);
        auto hti = static_cast<std::size_t>(ht);
        if (ht == HandType::HandTypeNb || hand[ci][hti] <= 0) {
            return false;
        }
        --hand[ci][hti];

        // 盤面に配置
        char pieceChar = pieceInfoToChar(side, move.piece);
        board[to] = pieceChar;
        occupied.set(to);
        colorOcc[ci].set(to);
        pieceOcc[ci][static_cast<int>(move.piece)].set(to);

        return true;
    }

    // 盤上移動
    Square from = move.from;
    Square to = move.to;

    char movingChar = board[from];
    undo.movedPieceBefore = movingChar;
    char capturedChar = board[to];
    undo.capturedPiece = capturedChar;

    PieceInfo movingInfo = charToPieceInfo(movingChar);
    if (!movingInfo.valid) {
        return false;
    }

    // 移動元をクリア
    board[from] = kEmpty;
    occupied.clear(from);
    colorOcc[ci].clear(from);
    pieceOcc[ci][static_cast<int>(movingInfo.type)].clear(from);

    // 取る駒の処理
    if (capturedChar != kEmpty) {
        PieceInfo capInfo = charToPieceInfo(capturedChar);
        if (capInfo.valid) {
            int oi = static_cast<int>(opposite(side));
            occupied.clear(to);
            colorOcc[oi].clear(to);
            pieceOcc[oi][static_cast<int>(capInfo.type)].clear(to);

            // 持ち駒に追加（成り解除して）
            PieceType basePt = demotePieceType(capInfo.type);
            HandType ht = pieceTypeToHandType(basePt);
            if (ht != HandType::HandTypeNb) {
                ++hand[ci][static_cast<std::size_t>(ht)];
            }
        }
    }

    // 成り処理
    PieceType placedType = move.promote ? promotePieceType(movingInfo.type) : movingInfo.type;
    char placedChar = pieceInfoToChar(side, placedType);

    // 移動先に配置
    board[to] = placedChar;
    occupied.set(to);
    colorOcc[ci].set(to);
    pieceOcc[ci][static_cast<int>(placedType)].set(to);

    // 玉の位置更新
    if (movingInfo.type == PieceType::King) {
        kingSq[ci] = to;
    }

    return true;
}

void EnginePosition::undoMove(const UndoState& undo, Color side) noexcept
{
    int ci = static_cast<int>(side);
    const Move& move = undo.move;

    if (move.kind == MoveKind::Drop) {
        // 打ちの取り消し
        Square to = move.to;

        // 盤面から除去
        board[to] = kEmpty;
        occupied.clear(to);
        colorOcc[ci].clear(to);
        pieceOcc[ci][static_cast<int>(move.piece)].clear(to);

        // 持ち駒を戻す
        HandType ht = pieceTypeToHandType(move.piece);
        if (ht != HandType::HandTypeNb) {
            ++hand[ci][static_cast<std::size_t>(ht)];
        }

        return;
    }

    // 盤上移動の取り消し
    Square from = move.from;
    Square to = move.to;

    // 現在toにある駒の情報
    char currentPieceChar = board[to];
    PieceInfo currentInfo = charToPieceInfo(currentPieceChar);

    // toから除去
    if (currentInfo.valid) {
        board[to] = kEmpty;
        occupied.clear(to);
        colorOcc[ci].clear(to);
        pieceOcc[ci][static_cast<int>(currentInfo.type)].clear(to);
    }

    // fromに元の駒を戻す
    board[from] = undo.movedPieceBefore;
    PieceInfo origInfo = charToPieceInfo(undo.movedPieceBefore);
    if (origInfo.valid) {
        occupied.set(from);
        colorOcc[ci].set(from);
        pieceOcc[ci][static_cast<int>(origInfo.type)].set(from);
    }

    // 取った駒を復元
    if (undo.capturedPiece != kEmpty) {
        PieceInfo capInfo = charToPieceInfo(undo.capturedPiece);
        if (capInfo.valid) {
            int oi = static_cast<int>(opposite(side));
            board[to] = undo.capturedPiece;
            occupied.set(to);
            colorOcc[oi].set(to);
            pieceOcc[oi][static_cast<int>(capInfo.type)].set(to);

            // 持ち駒から除去
            PieceType basePt = demotePieceType(capInfo.type);
            HandType ht = pieceTypeToHandType(basePt);
            if (ht != HandType::HandTypeNb) {
                --hand[ci][static_cast<std::size_t>(ht)];
            }
        }
    }

    // 玉位置復元
    kingSq[0] = undo.kingSquareBefore[0];
    kingSq[1] = undo.kingSquareBefore[1];
    zobristKey = undo.zobristBefore;
}

} // namespace fmv
