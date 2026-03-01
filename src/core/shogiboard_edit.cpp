/// @file shogiboard_edit.cpp
/// @brief 盤面編集・デバッグ出力の実装

#include "shogiboard.h"
#include "logcategories.h"

// ============================================================
// 盤面更新
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    if (fileTo < 10) {
        // 指したマスに相手の駒があった場合、自分の駒台に加える
        addPieceToStand(dest);
    } else {
        // 駒台に駒を移した場合
        incrementPieceOnStand(dest);
    }

    if (fileFrom == 10 || fileFrom == 11) {
        decrementPieceOnStand(source);
    }

    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

void ShogiBoard::setInitialPieceStandValues()
{
    static const QList<QPair<Piece, int>> initialValues = {
        {Piece::BlackPawn, 9}, {Piece::BlackLance, 2}, {Piece::BlackKnight, 2},
        {Piece::BlackSilver, 2}, {Piece::BlackGold, 2}, {Piece::BlackBishop, 1},
        {Piece::BlackRook, 1}, {Piece::BlackKing, 1},
        {Piece::WhiteKing, 1}, {Piece::WhiteRook, 1}, {Piece::WhiteBishop, 1},
        {Piece::WhiteGold, 2}, {Piece::WhiteSilver, 2}, {Piece::WhiteKnight, 2},
        {Piece::WhiteLance, 2}, {Piece::WhitePawn, 9},
    };

    for (const auto& pair : initialValues) {
        m_pieceStand[pair.first] = pair.second;
    }
}

// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void ShogiBoard::resetGameBoard()
{
    m_boardData.fill(Piece::None, static_cast<qsizetype>(ranks()) * files());
    setInitialPieceStandValues();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    QVector<Piece> originalBoardData = m_boardData;
    QVector<Piece> newBoardData;

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

    auto nextInCycle = [](const QVector<Piece>& cyc, Piece cur)->Piece {
        qsizetype idx = cyc.indexOf(cur);
        if (idx < 0) return cur;
        return cyc[(idx + 1) % cyc.size()];
    };

    const auto lanceCycle  = QVector<Piece>{Piece::BlackLance, Piece::BlackPromotedLance, Piece::WhiteLance, Piece::WhitePromotedLance};
    const auto knightCycle = QVector<Piece>{Piece::BlackKnight, Piece::BlackPromotedKnight, Piece::WhiteKnight, Piece::WhitePromotedKnight};
    const auto silverCycle = QVector<Piece>{Piece::BlackSilver, Piece::BlackPromotedSilver, Piece::WhiteSilver, Piece::WhitePromotedSilver};
    const auto bishopCycle = QVector<Piece>{Piece::BlackBishop, Piece::BlackHorse, Piece::WhiteBishop, Piece::WhiteHorse};
    const auto rookCycle   = QVector<Piece>{Piece::BlackRook, Piece::BlackDragon, Piece::WhiteRook, Piece::WhiteDragon};
    const auto pawnCycle   = QVector<Piece>{Piece::BlackPawn, Piece::BlackPromotedPawn, Piece::WhitePawn, Piece::WhitePromotedPawn};

    const Piece cur = getPieceCharacter(fileFrom, rankFrom);
    QVector<Piece> base;
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
            const Piece pc = getPieceCharacter(fileFrom, r);
            if (candidate == Piece::BlackPawn && pc == Piece::BlackPawn) return true;
            if (candidate == Piece::WhitePawn && pc == Piece::WhitePawn) return true;
        }
        return false;
    };

    auto isDisallowed = [&](Piece piece)->bool {
        return isRankDisallowed(piece) || isNiFuDisallowed(piece);
    };

    QVector<Piece> filtered;
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

// ============================================================
// デバッグ出力
// ============================================================

void ShogiBoard::printPlayerPieces(const QString& player, const QString& pieceSet) const
{
    qCDebug(lcCore, "%s の持ち駒", qUtf8Printable(player));

    for (const QChar& ch : pieceSet) {
        Piece piece = charToPiece(ch);
        qCDebug(lcCore, "%s  %d", qUtf8Printable(QString(ch)), m_pieceStand.value(piece, 0));
    }
}

void ShogiBoard::printPieceStand()
{
    printPlayerPieces("先手", "KRGBSNLP");
    printPlayerPieces("後手", "krgbsnlp");
}

void ShogiBoard::printPieceCount() const
{
    qCDebug(lcCore, "先手の持ち駒:");
    qCDebug(lcCore, "歩:  %d", m_pieceStand.value(Piece::BlackPawn, 0));
    qCDebug(lcCore, "香車:  %d", m_pieceStand.value(Piece::BlackLance, 0));
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand.value(Piece::BlackKnight, 0));
    qCDebug(lcCore, "銀:  %d", m_pieceStand.value(Piece::BlackSilver, 0));
    qCDebug(lcCore, "金:  %d", m_pieceStand.value(Piece::BlackGold, 0));
    qCDebug(lcCore, "角:  %d", m_pieceStand.value(Piece::BlackBishop, 0));
    qCDebug(lcCore, "飛車:  %d", m_pieceStand.value(Piece::BlackRook, 0));

    qCDebug(lcCore, "後手の持ち駒:");
    qCDebug(lcCore, "歩:  %d", m_pieceStand.value(Piece::WhitePawn, 0));
    qCDebug(lcCore, "香車:  %d", m_pieceStand.value(Piece::WhiteLance, 0));
    qCDebug(lcCore, "桂馬:  %d", m_pieceStand.value(Piece::WhiteKnight, 0));
    qCDebug(lcCore, "銀:  %d", m_pieceStand.value(Piece::WhiteSilver, 0));
    qCDebug(lcCore, "金:  %d", m_pieceStand.value(Piece::WhiteGold, 0));
    qCDebug(lcCore, "角:  %d", m_pieceStand.value(Piece::WhiteBishop, 0));
    qCDebug(lcCore, "飛車:  %d", m_pieceStand.value(Piece::WhiteRook, 0));
}
