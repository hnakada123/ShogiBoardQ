/// @file shogigamecontroller.cpp
/// @brief 対局の盤で指し手を確かめて指すコントローラの実装

#include "shogigamecontroller.h"

#include <QPoint>
#include <QDebug>
#include <utility>

#include "boardconstants.h"
#include "boardcoordinates.h"
#include "enginemovevalidator.h"
#include "kifmovetext.h"
#include "legalmovestatus.h"
#include "logcategories.h"
#include "shogiboard.h"

// ============================================================
// 初期化
// ============================================================

ShogiGameController::ShogiGameController(QObject* parent)
    : QObject(parent)
    , m_board(std::make_unique<ShogiBoard>())
{
}

ShogiGameController::~ShogiGameController() = default;

ShogiBoard* ShogiGameController::board() const
{
    return m_board.get();
}

void ShogiGameController::newGame(const QString& startSfen)
{
    qCDebug(lcGame) << "ShogiGameController::newGame:" << startSfen;

    board()->setSfen(startSfen);
}

// ============================================================
// 成り制御
// ============================================================

void ShogiGameController::setPromotionChooser(PromotionChooser chooser)
{
    m_promotionChooser = std::move(chooser);
}


// ============================================================
// 手番管理
// ============================================================

Turn ShogiGameController::currentPlayer() const
{
    return m_board->currentPlayer();
}

void ShogiGameController::setCurrentPlayer(Turn player)
{
    m_board->setCurrentPlayer(player);
}

// ============================================================
// 指し手検証・盤面更新
// ============================================================

LegalMoveStatus ShogiGameController::legalStatus(const UsiMove& move, Turn mover) const
{
    return EngineMoveValidator::legalStatus(board()->position(), mover, move);
}

std::optional<UsiMove> ShogiGameController::prepareMove(const QPoint& outFrom, const QPoint& outTo)
{
    const int fileFrom = outFrom.x();
    const int rankFrom = outFrom.y();
    const bool boardFrom = fileFrom >= 1 && fileFrom <= BoardConstants::kBoardSize
        && rankFrom >= 1 && rankFrom <= BoardConstants::kBoardSize;
    // 駒台から打てるのは手番の側の駒台の駒（玉は打てない）
    const bool handFrom = (currentPlayer() == Turn::Black
        && fileFrom == BoardConstants::kBlackStandFile && rankFrom >= 1 && rankFrom <= 7)
        || (currentPlayer() == Turn::White
        && fileFrom == BoardConstants::kWhiteStandFile && rankFrom >= 3 && rankFrom <= 9);
    const std::optional<UsiMove> candidate =
        (boardFrom || handFrom) ? BoardCoordinates::usiMoveFromSquares(outFrom, outTo, false) : std::nullopt;
    if (!candidate) {
        qCWarning(lcGame) << "prepareMove: invalid coordinates" << outFrom << outTo;
        emit endDragSignal();
        return std::nullopt;
    }

    const Turn mover = currentPlayer();
    const Piece moving = candidate->isDrop() ? candidate->dropPieceFor(mover)
                                             : board()->pieceCharacter(fileFrom, rankFrom);
    if (moving == Piece::None) {
        qCWarning(lcGame) << "prepareMove: the source square is empty.";
        emit endDragSignal();
        return std::nullopt;
    }

    // 成り・不成のどちらが合法かで決める。両方とも合法なら対局者に選ばせる
    const LegalMoveStatus status = legalStatus(*candidate, mover);
    UsiMove move = *candidate;
    if (status.promotingMoveExists && status.nonPromotingMoveExists) {
        move.promote = m_promotionChooser ? m_promotionChooser() : false;
    } else if (status.promotingMoveExists) {
        move.promote = true;
    } else if (!status.nonPromotingMoveExists) {
        emit endDragSignal();
        return std::nullopt;
    }
    emit endDragSignal();

    return move;
}

bool ShogiGameController::isLegal(const UsiMove& move) const
{
    if (!move.isValid()) return false;
    const LegalMoveStatus status = legalStatus(move, currentPlayer());
    return move.promote ? status.promotingMoveExists : status.nonPromotingMoveExists;
}

std::optional<RecordEntry> ShogiGameController::applyMove(const UsiMove& move, BoardSquare previousTo)
{
    if (!move.isValid()) {
        qCWarning(lcGame).noquote() << "applyMove: rejected" << move.toString();
        return std::nullopt;
    }
    // エンジンの手も、人間の手と同じく合法手か確かめる
    if (!isLegal(move)) {
        qCWarning(lcGame).noquote() << "applyMove: illegal move" << move.toString();
        return std::nullopt;
    }
    return commitMove(move, previousTo);
}

std::optional<RecordEntry> ShogiGameController::applyRecordedMove(const UsiMove& move, BoardSquare previousTo)
{
    if (!move.isValid()) return std::nullopt;
    return commitMove(move, previousTo);
}

std::optional<RecordEntry> ShogiGameController::commitMove(const UsiMove& move, BoardSquare previousTo)
{
    const Turn mover = currentPlayer();
    const Piece moving = move.isDrop() ? move.dropPieceFor(mover) : board()->pieceAt(move.from);
    if (moving == Piece::None) {
        qCWarning(lcGame).noquote() << "commitMove: the source square is empty:" << move.toString();
        return std::nullopt;
    }
    const Piece captured = board()->pieceAt(move.to);
    if (!board()->applyUsiMove(move, mover)) {
        qCWarning(lcGame).noquote() << "commitMove: the board rejected" << move.toString();
        return std::nullopt;
    }
    RecordEntry entry = RecordEntry::played(ShogiMove(move, moving, captured),
                                            KifMoveText::format(mover, moving, move, previousTo), board()->toSfen());
    return entry;
}

void ShogiGameController::notifyMoveCommitted(Turn mover, int ply)
{
    emit moveCommitted(mover, ply);
}

