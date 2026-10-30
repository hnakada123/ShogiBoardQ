/// @file shogiboard.cpp
/// @brief 表示に使う盤（局面の値の包みと、変更の知らせ）の実装

#include "shogiboard.h"
#include "boardcoordinates.h"
#include "logcategories.h"

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

void ShogiBoard::replacePosition(const Position& next)
{
    // 変わったマスと駒台と手番だけを知らせる
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
    if (before.sideToMove() != m_position.sideToMove()) emit turnChanged(m_position.sideToMove());
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

void ShogiBoard::setCurrentPlayer(Turn turn)
{
    if (m_position.sideToMove() == turn) return;
    m_position.setSideToMove(turn);
    emit turnChanged(turn);
}

// ============================================================
// 盤面アクセス
// ============================================================

// 指定した位置の駒を返す。
// file: 筋（1〜9は盤上、10と11は先手と後手の駒台、12は駒箱）
// rank: 駒台・駒箱の段の並びは BoardCoordinates::standPieceAt
Piece ShogiBoard::pieceCharacter(const int file, const int rank) const
{
    if (file >= 1 && file <= Position::kFiles) {
        if (rank < 1 || rank > Position::kRanks) {
            qCWarning(lcCore, "Invalid rank for board access: file=%d rank=%d", file, rank);
            return Piece::None;
        }
        return m_position.pieceAt(file, rank);
    }
    const Piece standPiece = BoardCoordinates::pieceAt(m_position, file, rank);
    if (standPiece == Piece::None) {
        qCWarning(lcCore, "Invalid stand coordinates: file=%d rank=%d", file, rank);
    }
    return standPiece;
}

// 駒台・駒箱から動かそうとした場合、その駒の枚数が0なら動かせない。
bool ShogiBoard::isPieceAvailableOnStand(const Piece source, const int fileFrom) const
{
    return BoardCoordinates::isAvailable(m_position, source, fileFrom);
}

QMap<Piece, int> ShogiBoard::pieceBox() const
{
    return m_position.pieceBox();
}

int ShogiBoard::pieceBoxCount(Piece piece) const
{
    return pieceBox().value(toBlack(demote(piece)), 0);
}
