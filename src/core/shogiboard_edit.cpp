/// @file shogiboard_edit.cpp
/// @brief 盤面編集の実装

#include "shogiboard.h"
#include "boardconstants.h"

// ============================================================
// 盤面更新
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    if (fileTo < BoardConstants::kBlackStandFile) {
        // 指したマスに相手の駒があった場合、自分の駒台に加える
        addPieceToStand(dest);
    } else {
        // 駒台に駒を移した場合
        incrementPieceOnStand(dest);
    }

    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        decrementPieceOnStand(source);
    }

    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

void ShogiBoard::setInitialPieceStandValues()
{
    static const QList<QPair<Piece, int>> initialValues = {
        {Piece::BlackPawn, 9}, {Piece::BlackLance, 2}, {Piece::BlackKnight, 2},
        {Piece::BlackSilver, 2}, {Piece::BlackGold, 2}, {Piece::BlackBishop, 1},
        {Piece::BlackRook, 1}, {Piece::BlackKing, 0},
        {Piece::WhiteKing, 0}, {Piece::WhiteRook, 1}, {Piece::WhiteBishop, 1},
        {Piece::WhiteGold, 2}, {Piece::WhiteSilver, 2}, {Piece::WhiteKnight, 2},
        {Piece::WhiteLance, 2}, {Piece::WhitePawn, 9},
    };

    for (const auto& pair : initialValues) {
        m_pieceStand[pair.first] = pair.second;
    }
}

// 王・玉は現在位置に残し、それ以外の駒を先後同数ずつ駒台に載せる。
void ShogiBoard::resetGameBoard()
{
    for (Piece& piece : m_boardData) {
        if (piece != Piece::BlackKing && piece != Piece::WhiteKing) piece = Piece::None;
    }
    setInitialPieceStandValues();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    QList<Piece> originalBoardData = m_boardData;
    QList<Piece> newBoardData;

    for (int i = 0; i < 81; i++) {
        Piece piece = originalBoardData.at(80 - i);
        // 先後反転
        if (isBlackPiece(piece)) {
            piece = toWhite(piece);
        } else if (isWhitePiece(piece)) {
            piece = toBlack(piece);
        }
        newBoardData.append(piece);
    }

    m_boardData = newBoardData;

    const QMap<Piece, int> originalPieceStand = m_pieceStand;

    for (auto it = originalPieceStand.cbegin(); it != originalPieceStand.cend(); ++it) {
        Piece flipped = isBlackPiece(it.key()) ? toWhite(it.key()) : toBlack(it.key());
        m_pieceStand[flipped] = it.value();
    }
}

// ============================================================
// 局面編集: 成駒/不成駒/先後の巡回変換
// ============================================================

// 局面編集中に右クリックで成駒/不成駒/先後を巡回変換する（禁置き段＋二歩をスキップ）。
void ShogiBoard::promoteOrDemotePiece(const int fileFrom, const int rankFrom)
{
    // 処理フロー:
    // 1. 駒種ごとの巡回リストを特定
    // 2. 禁置き段・二歩の候補をフィルタ
    // 3. フィルタ済みリストで次の候補に切り替え

    auto nextInCycle = [](const QList<Piece>& cyc, Piece cur)->Piece {
        qsizetype idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QList<Piece>{Piece::BlackLance, Piece::BlackPromotedLance, Piece::WhiteLance, Piece::WhitePromotedLance};
    const auto knightCycle = QList<Piece>{Piece::BlackKnight, Piece::BlackPromotedKnight, Piece::WhiteKnight, Piece::WhitePromotedKnight};
    const auto silverCycle = QList<Piece>{Piece::BlackSilver, Piece::BlackPromotedSilver, Piece::WhiteSilver, Piece::WhitePromotedSilver};
    const auto bishopCycle = QList<Piece>{Piece::BlackBishop, Piece::BlackHorse, Piece::WhiteBishop, Piece::WhiteHorse};
    const auto rookCycle   = QList<Piece>{Piece::BlackRook, Piece::BlackDragon, Piece::WhiteRook, Piece::WhiteDragon};
    const auto pawnCycle   = QList<Piece>{Piece::BlackPawn, Piece::BlackPromotedPawn, Piece::WhitePawn, Piece::WhitePromotedPawn};

    const Piece cur = pieceCharacter(fileFrom, rankFrom);
    QList<Piece> base;
    switch (static_cast<char>(cur)) {
    case 'L': case 'M': case 'l': case 'm': base = lanceCycle;  break;
    case 'N': case 'O': case 'n': case 'o': base = knightCycle; break;
    case 'S': case 'T': case 's': case 't': base = silverCycle; break;
    case 'B': case 'C': case 'b': case 'c': base = bishopCycle; break;
    case 'R': case 'U': case 'r': case 'u': base = rookCycle;   break;
    case 'P': case 'Q': case 'p': case 'q': base = pawnCycle;   break;
    default:
        return; // 金・玉などは変換対象外
    }

    const bool onBoard = (fileFrom >= 1 && fileFrom <= 9);

    auto isRankDisallowed = [&](Piece piece)->bool {
        if (!onBoard) return false;
        if (piece == Piece::BlackLance && rankFrom == 1) return true;
        if (piece == Piece::BlackKnight && (rankFrom == 1 || rankFrom == 2)) return true;
        if (piece == Piece::BlackPawn && rankFrom == 1) return true;
        if (piece == Piece::WhiteLance && rankFrom == 9) return true;
        if (piece == Piece::WhiteKnight && (rankFrom == 8 || rankFrom == 9)) return true;
        if (piece == Piece::WhitePawn && rankFrom == 9) return true;
        return false;
    };

    auto isNiFuDisallowed = [&](Piece candidate)->bool {
        if (!onBoard) return false;
        if (candidate != Piece::BlackPawn && candidate != Piece::WhitePawn) return false;
        for (int r = 1; r <= 9; ++r) {
            if (r == rankFrom) continue;
            const Piece pc = pieceCharacter(fileFrom, r);
            if (candidate == Piece::BlackPawn && pc == Piece::BlackPawn) return true;
            if (candidate == Piece::WhitePawn && pc == Piece::WhitePawn) return true;
        }
        return false;
    };

    auto isDisallowed = [&](Piece piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    QList<Piece> filtered;
    filtered.reserve(base.size());
    for (Piece p : std::as_const(base)) {
        if (!isDisallowed(p)) filtered.push_back(p);
    }
    if (filtered.isEmpty()) return;

    // 現在形がfiltered外（既に禁止形）なら、baseを回して最初の許可形へジャンプ
    Piece next = cur;
    if (filtered.indexOf(cur) < 0) {
        Piece probe = cur;
        for (qsizetype i = 0; i < base.size(); ++i) {
            probe = nextInCycle(base, probe);
            if (filtered.indexOf(probe) >= 0) { next = probe; break; }
        }
    } else {
        qsizetype idx = filtered.indexOf(cur);
        next = filtered[(idx + 1) % filtered.size()];
    }

    setData(fileFrom, rankFrom, next);
}
