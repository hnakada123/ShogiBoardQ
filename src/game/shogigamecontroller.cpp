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
#include "piecemoverules.h"
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
    setForcedPromotion(false);

    m_lastMove = UsiMove{};
}

// ============================================================
// 成り制御
// ============================================================

void ShogiGameController::setPromotionChooser(PromotionChooser chooser)
{
    m_promotionChooser = std::move(chooser);
}

void ShogiGameController::setForcedPromotion(bool force, bool promote)
{
    m_forcedPromotionMode = force;
    m_forcedPromotionValue = promote;
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

std::optional<RecordEntry> ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo, BoardSquare previousTo)
{
    const auto move = prepareMove(outFrom, outTo);
    return move ? applyRecordedMove(*move, previousTo) : std::nullopt;
}

std::optional<UsiMove> ShogiGameController::prepareMove(const QPoint& outFrom, const QPoint& outTo)
{
    // 成れない手や不合法手でも、この着手の成り指定を後続の手に持ち越さない。
    const bool forcedPromotionMode = std::exchange(m_forcedPromotionMode, false);
    const bool forcedPromotionValue = std::exchange(m_forcedPromotionValue, false);

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
        qCWarning(lcGame) << "validateAndMove: invalid coordinates" << outFrom << outTo;
        emit endDragSignal();
        return std::nullopt;
    }

    const Turn mover = currentPlayer();
    const Piece moving = candidate->isDrop() ? candidate->dropPieceFor(mover)
                                             : board()->pieceCharacter(fileFrom, rankFrom);
    if (moving == Piece::None) {
        qCWarning(lcGame) << "validateAndMove: the source square is empty.";
        emit endDragSignal();
        return std::nullopt;
    }

    // 成り・不成のどちらが合法かで決める。両方とも合法なら対局者に選ばせる
    // （定跡の手は成り・不成が決まっているので選ばせない）。
    const LegalMoveStatus status = legalStatus(*candidate, mover);
    UsiMove move = *candidate;
    if (status.promotingMoveExists && status.nonPromotingMoveExists) {
        move.promote = forcedPromotionMode ? forcedPromotionValue
                                           : (m_promotionChooser ? m_promotionChooser() : false);
    } else if (status.promotingMoveExists) {
        move.promote = true;
    } else if (!status.nonPromotingMoveExists) {
        emit endDragSignal();
        return std::nullopt;
    }
    emit endDragSignal();

    return move;
}

std::optional<RecordEntry> ShogiGameController::applyMove(const UsiMove& move, BoardSquare previousTo)
{
    if (!move.isValid()) {
        qCWarning(lcGame).noquote() << "applyMove: rejected" << move.toString();
        return std::nullopt;
    }
    // エンジンの手も、人間の手と同じく合法手か確かめる（成り・不成も指し手どおりに判定する）
    const LegalMoveStatus status = legalStatus(move, currentPlayer());
    if (move.promote ? !status.promotingMoveExists : !status.nonPromotingMoveExists) {
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
    m_lastMove = move;
    return entry;
}

void ShogiGameController::notifyMoveCommitted(Turn mover, int ply)
{
    emit moveCommitted(mover, ply);
}

// ============================================================
// 局面編集
// ============================================================

bool ShogiGameController::editPosition(const QPoint& outFrom, const QPoint& outTo, Turn pieceBoxSide)
{

    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    const auto validSquare = [](const QPoint& square) {
        return (square.x() >= 1 && square.x() <= 9 && square.y() >= 1 && square.y() <= 9)
            || (square.x() == BoardConstants::kBlackStandFile && square.y() >= 1 && square.y() <= 8)
            || (square.x() == BoardConstants::kWhiteStandFile && square.y() >= 2 && square.y() <= 9)
            || (square.x() == BoardConstants::kPieceBoxFile && square.y() >= 1 && square.y() <= 8);
    };
    if (!validSquare(outFrom) || !validSquare(outTo) || outFrom == outTo) return false;

    Piece source = board()->pieceCharacter(fileFrom, rankFrom);
    Piece dest = board()->pieceCharacter(fileTo, rankTo);
    // 駒箱は先後共通の在庫。盤上に置く側を、二歩・成りなどの判定前に確定する。
    if (fileFrom == BoardConstants::kPieceBoxFile && pieceBoxSide == Turn::White)
        source = toWhite(source);
    if (source == Piece::None) return false;
    if (fileTo >= BoardConstants::kBlackStandFile && toBlack(demote(source)) != toBlack(dest)) return false;

    // 編集中の駒配置は、ユーザーが選んだ手番を変更しない。
    if (!PieceMoveRules::checkMovePiece(board(), source, dest, fileFrom, fileTo)) return false;

    const bool promote = PieceMoveRules::shouldAutoPromote(fileTo, rankTo, source);

    board()->updateBoardAndPieceStand(source, dest, fileFrom, rankFrom, fileTo, rankTo, promote);

    return true;
}

void ShogiGameController::switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const
{
    // 二歩や段の必成などの禁止形スキップは board 側で一括処理する

    board()->promoteOrDemotePiece(fileFrom, rankFrom);
}

