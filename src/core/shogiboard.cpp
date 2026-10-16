/// @file shogiboard.cpp
/// @brief 表示に使う盤（局面の値の包みと、局面編集の操作）の実装

#include "shogiboard.h"
#include "boardcoordinates.h"
#include "boardconstants.h"
#include "logcategories.h"
#include "piecerules.h"

#include "errorbus.h"

#include <utility>

// ============================================================
// 初期化・局面
// ============================================================

ShogiBoard::ShogiBoard(QObject* parent)
    : QObject(parent)
{
}

void ShogiBoard::setPosition(const Position& position)
{
    m_position = position;
    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit boardReset();
}

void ShogiBoard::setSfen(const QString& sfenStr)
{
    QString error;
    // 欄の数が違う文字列（SFEN でないもの）は知らせない。4つの欄の中身（盤・持ち駒）が読めないときだけ知らせる
    if (!trySetSfen(sfenStr, &error) && !error.isEmpty() && sfenStr.count(QLatin1Char(' ')) == 3) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, error);
    }
}

bool ShogiBoard::trySetSfen(const QString& sfenStr, QString* error)
{
    const std::optional<Position> position = Position::fromSfen(sfenStr, error);
    if (!position) {
        qCWarning(lcCore, "Invalid SFEN: %s", qUtf8Printable(sfenStr.left(200)));
        return false;
    }
    setPosition(*position);
    return true;
}

// ============================================================
// 盤面アクセス
// ============================================================

void ShogiBoard::setPieceAt(BoardSquare square, Piece piece)
{
    if (!square.isValid()) return;
    setData(square.file, square.rank, piece);
}

void ShogiBoard::addStandPiece(Piece piece, int delta)
{
    if (delta <= 0 || piece == Piece::None) return;
    m_position.addToHand(piece, delta);
    emit standChanged(demote(piece));
}

bool ShogiBoard::consumeStandPiece(Piece piece)
{
    if (!m_position.takeFromHand(piece)) return false;
    emit standChanged(demote(piece));
    return true;
}

void ShogiBoard::setCurrentPlayer(Turn turn)
{
    if (m_position.sideToMove() == turn) return;
    m_position.setSideToMove(turn);
    emit turnChanged(turn);
}

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台）
// rank: 段（先手は1〜7「歩、香車、桂馬、銀、金、角、飛車」、後手は3〜9「飛車、角、金、銀、桂馬、香車、歩」を使用）
Piece ShogiBoard::pieceCharacter(const int file, const int rank) const
{
    if (file >= 1 && file <= Position::kFiles) {
        if (rank < 1 || rank > Position::kRanks) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_position.pieceAt(file, rank);
    }
    const Piece standPiece = BoardCoordinates::standPieceAt(file, rank);
    if (standPiece == Piece::None) {
        qCWarning(lcCore, "Invalid stand coordinates: file=%d rank=%d", file, rank);
    }
    return standPiece;
}

void ShogiBoard::setData(const int file, const int rank, const Piece piece)
{
    const BoardSquare square{file, rank};
    if (m_position.pieceAt(square) == piece) return;
    m_position.setPieceAt(square, piece);
    // ShogiView::setBoardのconnectで再描画がトリガされる
    emit dataChanged(file, rank);
}

// ============================================================
// 盤面操作
// ============================================================

// 編集局面でも使用されるため、歩/桂/香の禁置き段では自動で成駒に置き換える（必成）。
void ShogiBoard::movePieceToSquare(Piece selectedPiece, const int fileFrom, const int rankFrom,
                                   const int fileTo, const int rankTo, const bool promote)
{
    if (promote) selectedPiece = ::promote(selectedPiece);
    if (fileFrom >= 1 && fileFrom <= Position::kFiles) setData(fileFrom, rankFrom, Piece::None);
    if (fileTo >= 1 && fileTo <= Position::kFiles) {
        if (PieceRules::isDeadSquare(selectedPiece, rankTo)) selectedPiece = ::promote(selectedPiece);
        setData(fileTo, rankTo, selectedPiece);
    }
}

bool ShogiBoard::applyUsiMove(const UsiMove& move, Turn mover)
{
    Position next = m_position;
    next.setSideToMove(mover);
    if (move.isDrop() && next.pieceAt(move.to) != Piece::None) return false;
    if (!next.apply(move)) return false;

    // 変わったマスと駒台だけを知らせる
    const Position before = std::exchange(m_position, next);
    for (int rank = 1; rank <= Position::kRanks; ++rank) {
        for (int file = 1; file <= Position::kFiles; ++file) {
            if (before.pieceAt(file, rank) != m_position.pieceAt(file, rank)) emit dataChanged(file, rank);
        }
    }
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const Piece piece = makePiece(side, static_cast<PieceKind>(kind));
            if (before.handCount(piece) != m_position.handCount(piece)) emit standChanged(piece);
        }
    }
    emit turnChanged(m_position.sideToMove());
    return true;
}

// ============================================================
// 駒台操作
// ============================================================

// 指したマスに相手の駒があった場合、自分の駒台の枚数に1加える。
void ShogiBoard::addPieceToStand(Piece dest)
{
    addStandPiece(capturedToHand(dest));
}

bool ShogiBoard::decrementPieceOnStand(Piece source)
{
    if (!consumeStandPiece(source)) {
        qCWarning(lcCore) << "decrementPieceOnStand: no piece on stand" << source;
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

// 自分の駒台に駒を置いた場合、自分の駒台の枚数に1加える。
void ShogiBoard::incrementPieceOnStand(const Piece dest)
{
    addStandPiece(demote(dest));
}

// ============================================================
// 局面編集
// ============================================================

// 局面編集中に使用される。将棋盤と駒台の駒を更新する。
void ShogiBoard::updateBoardAndPieceStand(const Piece source, const Piece dest, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo, const bool promote)
{
    if (fileTo < BoardConstants::kBlackStandFile) {
        // 指したマスに相手の駒があった場合、自分の駒台に加える
        if (dest != Piece::None) addPieceToStand(dest);
    } else if (fileTo != BoardConstants::kPieceBoxFile) {
        // 駒台に駒を移した場合
        incrementPieceOnStand(dest);
    }

    if (fileFrom == BoardConstants::kBlackStandFile || fileFrom == BoardConstants::kWhiteStandFile) {
        decrementPieceOnStand(source);
    }

    movePieceToSquare(source, fileFrom, rankFrom, fileTo, rankTo, promote);
}

QMap<Piece, int> ShogiBoard::pieceBox() const
{
    return m_position.pieceBox();
}

int ShogiBoard::pieceBoxCount(Piece piece) const
{
    return pieceBox().value(toBlack(demote(piece)), 0);
}

void ShogiBoard::resetGameBoard()
{
    Position empty;
    empty.setSideToMove(m_position.sideToMove());
    empty.setMoveNumber(m_position.moveNumber());
    setPosition(empty);
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiBoard::flipSides()
{
    Position flipped;
    flipped.setSideToMove(m_position.sideToMove());
    flipped.setMoveNumber(m_position.moveNumber());
    for (int rank = 1; rank <= Position::kRanks; ++rank) {
        for (int file = 1; file <= Position::kFiles; ++file) {
            flipped.setPieceAt({10 - file, 10 - rank}, flipSide(m_position.pieceAt(file, rank)));
        }
    }
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (int kind = 0; kind < kHandKindCount; ++kind) {
            const Piece piece = makePiece(side, static_cast<PieceKind>(kind));
            flipped.setHandCount(flipSide(piece), m_position.handCount(piece));
        }
    }
    m_position = flipped;
}

// 局面編集中に右クリックで成駒/不成駒/先後を巡回変換する（禁置き段＋二歩をスキップ）。
void ShogiBoard::promoteOrDemotePiece(const int fileFrom, const int rankFrom)
{
    if (fileFrom < 1 || fileFrom > Position::kFiles || rankFrom < 1 || rankFrom > Position::kRanks) return;
    const Piece current = m_position.pieceAt(fileFrom, rankFrom);
    if (current == Piece::None) return;

    // 巡回の順: 先手の生駒 → 先手の成駒 → 後手の生駒 → 後手の成駒（成れない駒は先手 → 後手）
    const Piece base = toBlack(demote(current));
    QList<Piece> cycle{base};
    if (isPromotable(base)) cycle << promote(base);
    cycle << toWhite(base);
    if (isPromotable(base)) cycle << toWhite(promote(base));

    // 行き所のない段と二歩になる形は飛ばす
    const auto allowed = [&](Piece candidate) {
        if (PieceRules::isDeadSquare(candidate, rankFrom)) return false;
        if (pieceKind(candidate) != PieceKind::Pawn) return true;
        for (int rank = 1; rank <= Position::kRanks; ++rank) {
            if (rank != rankFrom && m_position.pieceAt(fileFrom, rank) == candidate) return false;
        }
        return true;
    };

    const qsizetype index = cycle.indexOf(current);
    for (qsizetype step = 1; step <= cycle.size(); ++step) {
        const Piece next = cycle.at((index + step) % cycle.size());
        if (allowed(next)) {
            setData(fileFrom, rankFrom, next);
            return;
        }
    }
}
