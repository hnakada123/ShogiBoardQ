/// @file gameboard.cpp
/// @brief 対局の盤（指し手を確かめて指し、棋譜の1行にする）の実装

#include "gameboard.h"

#include "enginemovevalidator.h"
#include "legalmovestatus.h"
#include "logcategories.h"
#include "moverecord.h"
#include "shogiboard.h"

// ============================================================
// 初期化
// ============================================================

GameBoard::GameBoard(QObject* parent)
    : QObject(parent)
    , m_board(std::make_unique<ShogiBoard>())
{
}

GameBoard::~GameBoard() = default;

ShogiBoard* GameBoard::board() const
{
    return m_board.get();
}

void GameBoard::newGame(const QString& startSfen)
{
    qCDebug(lcGame) << "GameBoard::newGame:" << startSfen;
    m_board->setSfen(startSfen);
}

// ============================================================
// 指し手検証・盤面更新
// ============================================================

LegalMoveStatus GameBoard::legalStatus(const UsiMove& move, Turn mover) const
{
    return EngineMoveValidator::legalStatus(board()->position(), mover, move);
}

bool GameBoard::isLegal(const UsiMove& move) const
{
    if (!move.isValid()) return false;
    const LegalMoveStatus status = legalStatus(move, m_board->currentPlayer());
    return move.promote ? status.promotingMoveExists : status.nonPromotingMoveExists;
}

std::optional<RecordEntry> GameBoard::applyMove(const UsiMove& move, BoardSquare previousTo)
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

std::optional<RecordEntry> GameBoard::applyRecordedMove(const UsiMove& move, BoardSquare previousTo)
{
    if (!move.isValid()) return std::nullopt;
    return commitMove(move, previousTo);
}

std::optional<RecordEntry> GameBoard::commitMove(const UsiMove& move, BoardSquare previousTo)
{
    // 棋譜の読み込み（KifLineBuilder）と同じ組み立てで指した手の行を作り、変わった所だけを盤に知らせる
    Position next = m_board->position();
    QString error;
    std::optional<RecordEntry> entry =
        MoveRecord::play(next, move, previousTo, {}, {}, Position::DropRule::RequireHand, &error);
    if (!entry) {
        qCWarning(lcGame).noquote() << "commitMove:" << error;
        return std::nullopt;
    }
    m_board->replacePosition(next);
    return entry;
}

void GameBoard::notifyMoveCommitted(Turn mover, int ply)
{
    emit moveCommitted(mover, ply);
}

