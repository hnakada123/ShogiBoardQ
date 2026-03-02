/// @file shogiboard.cpp
/// @brief 将棋盤の初期化・盤面アクセス・駒台操作の実装
///
/// SFEN変換は shogiboard_sfen.cpp、盤面編集は shogiboard_edit.cpp に分離。

#include "shogiboard.h"
#include "boardconstants.h"
#include "logcategories.h"

// ============================================================
// 初期化
// ============================================================

ShogiBoard::ShogiBoard(int ranks, int files, QObject *parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    initBoard();
    initStand();
}

void ShogiBoard::initBoard()
{
    m_boardData.fill(Piece::None, static_cast<qsizetype>(ranks()) * files());
}

void ShogiBoard::initStand()
{
    m_pieceStand.clear();

    static const QList<Piece> pieces = {
        Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight, Piece::WhiteSilver,
        Piece::WhiteGold, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteKing,
        Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
        Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook, Piece::BlackKing
    };

    for (const Piece& piece : pieces) {
        m_pieceStand.insert(piece, 0);
    }
}

// ============================================================
// 盤面アクセス
// ============================================================

const QList<Piece>& ShogiBoard::boardData() const
{
    return m_boardData;
}

const QMap<Piece, int>& ShogiBoard::getPieceStand() const
{
    return m_pieceStand;
}

Turn ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
Piece ShogiBoard::getPieceCharacter(const int file, const int rank)
{
    static const QMap<int, Piece> pieceMapBlack = {
        {1, Piece::BlackPawn}, {2, Piece::BlackLance}, {3, Piece::BlackKnight},
        {4, Piece::BlackSilver}, {5, Piece::BlackGold}, {6, Piece::BlackBishop},
        {7, Piece::BlackRook}, {8, Piece::BlackKing}
    };
    static const QMap<int, Piece> pieceMapWhite = {
        {2, Piece::WhiteKing}, {3, Piece::WhiteRook}, {4, Piece::WhiteBishop},
        {5, Piece::WhiteGold}, {6, Piece::WhiteSilver}, {7, Piece::WhiteKnight},
        {8, Piece::WhiteLance}, {9, Piece::WhitePawn}
    };

    if (file >= 1 && file <= 9) {
        if (rank < 1 || rank > ranks()) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_boardData.at((rank - 1) * files() + (file - 1));
    } else if (file == BoardConstants::kBlackStandFile) {
        const auto it = pieceMapBlack.find(rank);
        if (it != pieceMapBlack.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for black stand: %d", rank);
        return Piece::None;
    } else if (file == BoardConstants::kWhiteStandFile) {
        const auto it = pieceMapWhite.find(rank);
        if (it != pieceMapWhite.end()) return it.value();

        qCWarning(lcCore, "Invalid rank for white stand: %d", rank);
        return Piece::None;
    } else {
        qCWarning(lcCore, "Invalid file value: %d", file);
        return Piece::None;
    }
}

bool ShogiBoard::setDataInternal(const int file, const int rank, const Piece piece)
{
    int index = (rank - 1) * files() + (file - 1);

    // 変更がなければ再描画不要
    if (m_boardData.at(index) == piece) return false;

    m_boardData[index] = piece;
    return true;
}

// ============================================================
// 盤面操作
// ============================================================

void ShogiBoard::setData(const int file, const int rank, const Piece piece)
{
    if (setDataInternal(file, rank, piece)) {
        // ShogiView::setBoardのconnectで再描画がトリガされる
        emit dataChanged(file, rank);
    }
}

// 編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
void ShogiBoard::movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                                   const int fileTo, const int rankTo, const bool promote)
{
    // 1) 「成る」指定が来ていれば、先に成駒へ
    if (promote) {
        selectedPiece = ::promote(selectedPiece);
    }

    // 2) 元位置を空白に
    if ((fileFrom >= 1) && (fileFrom <= 9)) {
        setData(fileFrom, rankFrom, Piece::None);
    }

    // 3) まず素の駒を置く（盤上のみ）
    if ((fileTo >= 1) && (fileTo <= 9)) {
        setData(fileTo, rankTo, selectedPiece);

        // 4) 置いた結果に対して「必成」補正（歩/桂/香）
        Piece placed = getPieceCharacter(fileTo, rankTo);

        // 先手の必成
        if (placed == Piece::BlackPawn && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedPawn);
        } else if (placed == Piece::BlackLance && rankTo == 1) {
            setData(fileTo, rankTo, Piece::BlackPromotedLance);
        } else if (placed == Piece::BlackKnight && (rankTo == 1 || rankTo == 2)) {
            setData(fileTo, rankTo, Piece::BlackPromotedKnight);
        }
        // 後手の必成
        else if (placed == Piece::WhitePawn && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedPawn);
        } else if (placed == Piece::WhiteLance && rankTo == 9) {
            setData(fileTo, rankTo, Piece::WhitePromotedLance);
        } else if (placed == Piece::WhiteKnight && (rankTo == 8 || rankTo == 9)) {
            setData(fileTo, rankTo, Piece::WhitePromotedKnight);
        }
    }
}

// ============================================================
// 駒台操作
// ============================================================

// 成駒は相手の生駒に変換し、それ以外は先後を反転する。
Piece ShogiBoard::convertPieceChar(const Piece c) const
{
    // 成駒→相手の生駒
    if (isPromoted(c)) {
        Piece base = demote(c);
        return isBlackPiece(base) ? toWhite(base) : toBlack(base);
    }
    // 非成駒→先後反転
    return isBlackPiece(c) ? toWhite(c) : toBlack(c);
}

// 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(Piece dest)
{
    Piece pieceChar = convertPieceChar(dest);

    if (m_pieceStand.contains(pieceChar)) {
            m_pieceStand[pieceChar]++;
    }
}

void ShogiBoard::decrementPieceOnStand(Piece source)
{
    if (m_pieceStand.contains(source)) {
        m_pieceStand[source]--;
    }
}

// 駒台から指そうとした場合、駒台の駒数が0以下なら指せない。
bool ShogiBoard::isPieceAvailableOnStand(const Piece source, const int fileFrom) const
{
    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        if (m_pieceStand.contains(source) && m_pieceStand[source] > 0) {
            return true;
        }
        else {
            return false;
        }
    }
    // 盤内から指そうとした場合、チェック範囲外のため常にtrue
    else {
        return true;
    }
}

Piece ShogiBoard::convertPromotedPieceToOriginal(const Piece dest) const
{
    if (isPromoted(dest)) {
        return demote(dest);
    }
    return dest;
}

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const Piece dest)
{
    Piece originalPiece = convertPromotedPieceToOriginal(dest);

    if (m_pieceStand.contains(originalPiece)) {
        m_pieceStand[originalPiece]++;
    }
}
