/// @file shogiboard.cpp
/// @brief 将棋盤の初期化・盤面アクセス・駒台操作の実装
///
/// SFEN変換は shogiboard_sfen.cpp、盤面編集は shogiboard_edit.cpp に分離。

#include "shogiboard.h"
#include "boardcoordinates.h"
#include "boardconstants.h"
#include "logcategories.h"

#include <algorithm>

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

namespace {

/// 16種の駒を0枚で持つ駒台の表。盤を作るたびに16個を挿入しないよう、すべての盤で共有する
/// （QMap は暗黙の共有なので、枚数を書き換えた盤だけが自分の表を持つ）
const QMap<Piece, int>& emptyStand()
{
    static const QMap<Piece, int> stand = [] {
        QMap<Piece, int> pieces;
        for (const Piece piece : {
                 Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight, Piece::WhiteSilver,
                 Piece::WhiteGold, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteKing,
                 Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
                 Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook, Piece::BlackKing}) {
            pieces.insert(piece, 0);
        }
        return pieces;
    }();
    return stand;
}

} // namespace

void ShogiBoard::initStand()
{
    m_pieceStand = emptyStand();
}

// ============================================================
// 盤面アクセス
// ============================================================

Piece ShogiBoard::pieceAt(BoardSquare square) const
{
    if (square.file < 1 || square.file > files() || square.rank < 1 || square.rank > ranks()) return Piece::None;
    return m_boardData.at((square.rank - 1) * files() + (square.file - 1));
}

void ShogiBoard::setPieceAt(BoardSquare square, Piece piece)
{
    if (square.file < 1 || square.file > files() || square.rank < 1 || square.rank > ranks()) return;
    setData(square.file, square.rank, piece);
}

const QList<Piece>& ShogiBoard::boardData() const
{
    return m_boardData;
}

const QMap<Piece, int>& ShogiBoard::pieceStand() const
{
    return m_pieceStand;
}

int ShogiBoard::pieceStandCount(Piece piece) const
{
    return m_pieceStand.value(piece, 0);
}

void ShogiBoard::addStandPiece(Piece piece, int delta)
{
    if (delta <= 0 || !m_pieceStand.contains(piece)) {
        return;
    }
    m_pieceStand[piece] += delta;
    emit standChanged(piece);
}

bool ShogiBoard::consumeStandPiece(Piece piece)
{
    if (!m_pieceStand.contains(piece) || m_pieceStand[piece] <= 0) {
        return false;
    }
    m_pieceStand[piece]--;
    emit standChanged(piece);
    return true;
}

Turn ShogiBoard::currentPlayer() const
{
    return m_currentPlayer;
}

void ShogiBoard::setCurrentPlayer(Turn turn)
{
    m_currentPlayer = turn;
}

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
Piece ShogiBoard::pieceCharacter(const int file, const int rank)
{
    if (file >= 1 && file <= 9) {
        if (rank < 1 || rank > ranks()) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_boardData.at((rank - 1) * files() + (file - 1));
    }
    const Piece standPiece = BoardCoordinates::standPieceAt(file, rank);
    if (standPiece == Piece::None) {
        qCWarning(lcCore, "Invalid stand coordinates: file=%d rank=%d", file, rank);
    }
    return standPiece;
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
        Piece placed = pieceCharacter(fileTo, rankTo);

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

bool ShogiBoard::applyUsiMove(const UsiMove& move, Turn mover)
{
    if (!move.isValid()) return false;
    if (move.isDrop()) {
        const Piece piece = move.dropPieceFor(mover);
        if (pieceCharacter(move.to.file, move.to.rank) != Piece::None) return false;
        if (!consumeStandPiece(piece)) return false;
        movePieceToSquare(piece, 0, 0, move.to.file, move.to.rank, false);
        return true;
    }
    const Piece moving = pieceCharacter(move.from.file, move.from.rank);
    if (moving == Piece::None) return false;
    const Piece captured = pieceCharacter(move.to.file, move.to.rank);
    if (captured != Piece::None) addPieceToStand(captured);
    movePieceToSquare(moving, move.from.file, move.from.rank, move.to.file, move.to.rank, move.promote);
    return true;
}

void ShogiBoard::copyPositionFrom(const ShogiBoard& other)
{
    if (this == &other) return;
    m_ranks = other.m_ranks;
    m_files = other.m_files;
    if (m_boardData.size() == other.m_boardData.size()) {
        std::copy(other.m_boardData.cbegin(), other.m_boardData.cend(), m_boardData.begin());
    } else {
        m_boardData = other.m_boardData;
    }
    // 駒台は、駒の種類（キー）が同じなら枚数だけを上書きする。違えば丸ごと写す
    bool sameKinds = m_pieceStand.size() == other.m_pieceStand.size();
    auto dst = m_pieceStand.begin();
    for (auto src = other.m_pieceStand.cbegin(); sameKinds && src != other.m_pieceStand.cend(); ++src, ++dst) {
        sameKinds = dst.key() == src.key();
        if (sameKinds) dst.value() = src.value();
    }
    if (!sameKinds) m_pieceStand = other.m_pieceStand;
    m_currentPlayer = other.m_currentPlayer;

    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
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
    addStandPiece(pieceChar);
}

bool ShogiBoard::decrementPieceOnStand(Piece source)
{
    if (!consumeStandPiece(source)) {
        qCWarning(lcCore, "decrementPieceOnStand: no piece on stand (%c)",
                  static_cast<char>(source));
        return false;
    }
    return true;
}

// 駒台から指そうとした場合、駒台の駒数が0以下なら指せない。
bool ShogiBoard::isPieceAvailableOnStand(const Piece source, const int fileFrom) const
{
    if (fileFrom == BoardConstants::kPieceBoxFile) return pieceBoxCount(source) > 0;
    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        return pieceStandCount(source) > 0;
    }
    return true;
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
    addStandPiece(originalPiece);
}
