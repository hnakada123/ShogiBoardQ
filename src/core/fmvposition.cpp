/// @file fmvposition.cpp
/// @brief エンジン内部局面のdo/undo実装

#include "fmvposition.h"

namespace fmv {

void EnginePosition::clear() noexcept
{
    board.fill(Piece::None);
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
    for (auto& colorPieces : pieceOcc) {
        for (auto& bb : colorPieces) {
            bb = {};
        }
    }
    kingSq[0] = kInvalidSquare;
    kingSq[1] = kInvalidSquare;

    for (int sq = 0; sq < kSquareNb; ++sq) {
        const Piece piece = board[static_cast<std::size_t>(sq)];
        if (piece == Piece::None) {
            continue;
        }

        auto s = static_cast<Square>(sq);
        int ci = static_cast<int>(colorOf(piece));
        const PieceType type = typeOf(piece);

        occupied.set(s);
        colorOcc[ci].set(s);
        pieceOcc[ci][static_cast<int>(type)].set(s);

        if (type == PieceType::King) {
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
        undo.movedPieceBefore = Piece::None;
        undo.capturedPiece = Piece::None;

        // 持ち駒を減らす
        HandType ht = handTypeOf(move.piece);
        auto hti = static_cast<std::size_t>(ht);
        if (ht == HandType::HandTypeNb || hand[ci][hti] <= 0) {
            return false;
        }
        --hand[ci][hti];

        // 盤面に配置
        board[to] = pieceOf(side, move.piece);
        occupied.set(to);
        colorOcc[ci].set(to);
        pieceOcc[ci][static_cast<int>(move.piece)].set(to);

        return true;
    }

    // 盤上移動
    Square from = move.from;
    Square to = move.to;

    const Piece moving = board[from];
    undo.movedPieceBefore = moving;
    const Piece captured = board[to];
    undo.capturedPiece = captured;

    if (moving == Piece::None) {
        return false;
    }
    const PieceType movingType = typeOf(moving);

    // 移動元をクリア
    board[from] = Piece::None;
    occupied.clear(from);
    colorOcc[ci].clear(from);
    pieceOcc[ci][static_cast<int>(movingType)].clear(from);

    // 取る駒の処理
    if (captured != Piece::None) {
        int oi = static_cast<int>(opposite(side));
        occupied.clear(to);
        colorOcc[oi].clear(to);
        pieceOcc[oi][static_cast<int>(typeOf(captured))].clear(to);

        // 持ち駒に追加（成り解除して）
        HandType ht = handTypeOf(typeOf(captured));
        if (ht != HandType::HandTypeNb) {
            ++hand[ci][static_cast<std::size_t>(ht)];
        }
    }

    // 成り処理
    PieceType placedType = move.promote ? promotedType(movingType) : movingType;

    // 移動先に配置
    board[to] = pieceOf(side, placedType);
    occupied.set(to);
    colorOcc[ci].set(to);
    pieceOcc[ci][static_cast<int>(placedType)].set(to);

    // 玉の位置更新
    if (movingType == PieceType::King) {
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
        board[to] = Piece::None;
        occupied.clear(to);
        colorOcc[ci].clear(to);
        pieceOcc[ci][static_cast<int>(move.piece)].clear(to);

        // 持ち駒を戻す
        HandType ht = handTypeOf(move.piece);
        if (ht != HandType::HandTypeNb) {
            ++hand[ci][static_cast<std::size_t>(ht)];
        }

        return;
    }

    // 盤上移動の取り消し
    Square from = move.from;
    Square to = move.to;

    // 現在toにある駒の情報
    const Piece current = board[to];

    // toから除去
    if (current != Piece::None) {
        board[to] = Piece::None;
        occupied.clear(to);
        colorOcc[ci].clear(to);
        pieceOcc[ci][static_cast<int>(typeOf(current))].clear(to);
    }

    // fromに元の駒を戻す
    board[from] = undo.movedPieceBefore;
    if (undo.movedPieceBefore != Piece::None) {
        occupied.set(from);
        colorOcc[ci].set(from);
        pieceOcc[ci][static_cast<int>(typeOf(undo.movedPieceBefore))].set(from);
    }

    // 取った駒を復元
    if (undo.capturedPiece != Piece::None) {
        int oi = static_cast<int>(opposite(side));
        board[to] = undo.capturedPiece;
        occupied.set(to);
        colorOcc[oi].set(to);
        pieceOcc[oi][static_cast<int>(typeOf(undo.capturedPiece))].set(to);

        // 持ち駒から除去
        HandType ht = handTypeOf(typeOf(undo.capturedPiece));
        if (ht != HandType::HandTypeNb) {
            --hand[ci][static_cast<std::size_t>(ht)];
        }
    }

    // 玉位置復元
    kingSq[0] = undo.kingSquareBefore[0];
    kingSq[1] = undo.kingSquareBefore[1];
    zobristKey = undo.zobristBefore;
}

} // namespace fmv
