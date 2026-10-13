/// @file shogigamecontroller.cpp
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラの実装

#include "shogigamecontroller.h"
#include "boardconstants.h"

#include <QPoint>
#include <QDebug>
#include <utility>

#include "logcategories.h"
#include "matchcoordinator.h"
#include "piecemoverules.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "playmode.h"
#include "kifmovetext.h"
#include "legalmovestatus.h"
#include "boardcoordinates.h"

// ============================================================
// 初期化
// ============================================================

ShogiGameController::ShogiGameController(QObject* parent)
    : QObject(parent)
{
}

ShogiGameController::~ShogiGameController() = default;

ShogiBoard* ShogiGameController::board() const
{
    return m_board.get();
}

void ShogiGameController::newGame(QString& initialSfenString)
{
    qCDebug(lcGame) << "ShogiGameController::newGame called";
    qCDebug(lcGame) << "  Initial SFEN:" << initialSfenString;
    qCDebug(lcGame) << "  Current board ptr:" << m_board.get();

    setupBoard();

    qCDebug(lcGame) << "  New board ptr after setupBoard:" << m_board.get();

    board()->setSfen(initialSfenString);
    setResult(NoResult);
    setForcedPromotion(false);

    // SFEN の手番フィールドから GC の手番を同期
    const Turn boardTurn = board()->currentPlayer();
    setCurrentPlayer(boardTurn == Turn::White ? Turn::White : Turn::Black);

    // 前の対局の最終手の移動先が残ると「同」表記が誤表示されるためリセット
    previousFileTo = 0;
    previousRankTo = 0;
    m_lastMove = UsiMove{};
}

void ShogiGameController::setupBoard()
{
    m_board = std::make_unique<ShogiBoard>(9, 9);
}

// ============================================================
// 対局結果
// ============================================================

void ShogiGameController::setResult(Result value)
{
    if (result() == value) return;

    m_result = value;
}

void ShogiGameController::resetResult()
{
    m_result = NoResult;
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
    qCDebug(lcGame) << "setForcedPromotion: force=" << force << "promote=" << promote;
}

// ============================================================
// 手番管理
// ============================================================

void ShogiGameController::setCurrentPlayer(Turn player)
{
    // 最初の設定は、既定の先手と同じでも知らせる（手番の表示を初めて合わせるため）
    if (m_sideToMoveSet && currentPlayer() == player) return;

    m_sideToMoveSet = true;
    m_currentPlayer = player;

    // 盤面モデルの手番は setSfen() でしか更新されないため、GC の手番に追従させる。
    // 対局中は盤面を SFEN から再設定しないので、これが無いと盤面から生成する SFEN
    // （読み筋表示の基準局面・局面コピー・表示一致性検証）の手番が古いままになる。
    if (board() != nullptr && (player == Turn::Black || player == Turn::White)) {
        board()->setCurrentPlayer(player == Turn::Black ? Turn::Black : Turn::White);
    }

    emit currentPlayerChanged(m_currentPlayer);
}

void ShogiGameController::changeCurrentPlayer()
{
    setCurrentPlayer(currentPlayer() == Turn::Black ? Turn::White : Turn::Black);
}

// ============================================================
// 棋譜文字列変換
// ============================================================

QString ShogiGameController::convertMoveToKanjiStr(Piece moving, const UsiMove& move)
{
    if (currentPlayer() != Turn::Black && currentPlayer() != Turn::White) {
        qCWarning(lcGame) << "convertMoveToKanjiStr: current player is invalid.";
        return QString();
    }
    // 前手と同じマスへの移動は「同」表記にする
    const Turn mover = (currentPlayer() == Turn::Black) ? Turn::Black : Turn::White;
    const QString text = KifMoveText::format(mover, moving, move, BoardSquare{previousFileTo, previousRankTo});
    previousFileTo = move.to.file;
    previousRankTo = move.to.rank;
    return text;
}

// ============================================================
// 指し手検証・盤面更新
// ============================================================

LegalMoveStatus ShogiGameController::legalStatus(const UsiMove& move, Turn mover, Piece moving, Piece captured) const
{
    EngineMoveValidator validator;
    const auto turn = (mover == Turn::Black) ? Turn::Black : Turn::White;
    ShogiMove probe = ShogiMove::fromUsiMove(move, mover, moving, captured);
    probe.isPromotion = false;
    return validator.isLegalMove(turn, board()->boardData(), board()->pieceStand(), probe);
}

bool ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, int moveNumber,
                                          QStringList* sfenHistory, QList<ShogiMove>& gameMoves)
{
    // 成れない手や不合法手でも、この着手の成り指定を後続の手に持ち越さない。
    const bool forcedPromotionMode = std::exchange(m_forcedPromotionMode, false);
    const bool forcedPromotionValue = std::exchange(m_forcedPromotionValue, false);

    if (!board() || (currentPlayer() != Turn::Black && currentPlayer() != Turn::White)) {
        qCWarning(lcGame) << "validateAndMove: no board or no side to move.";
        emit endDragSignal();
        return false;
    }

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
        return false;
    }

    const Turn mover = (currentPlayer() == Turn::Black) ? Turn::Black : Turn::White;
    const Piece moving = candidate->isDrop() ? candidate->dropPieceFor(mover)
                                             : board()->pieceCharacter(fileFrom, rankFrom);
    if (moving == Piece::None) {
        qCWarning(lcGame) << "validateAndMove: the source square is empty.";
        emit endDragSignal();
        return false;
    }
    const Piece captured = board()->pieceCharacter(candidate->to.file, candidate->to.rank);

    // 成り・不成のどちらが合法かで決める。両方とも合法なら対局者に選ばせる
    // （定跡の手は成り・不成が決まっているので選ばせない）。
    const LegalMoveStatus status = legalStatus(*candidate, mover, moving, captured);
    UsiMove move = *candidate;
    if (status.promotingMoveExists && status.nonPromotingMoveExists) {
        move.promote = forcedPromotionMode ? forcedPromotionValue
                                           : (m_promotionChooser ? m_promotionChooser() : false);
    } else if (status.promotingMoveExists) {
        move.promote = true;
    } else if (!status.nonPromotingMoveExists) {
        emit endDragSignal();
        return false;
    }
    emit endDragSignal();

    return commitMove(move, mover, moving, captured, record, moveNumber, sfenHistory, gameMoves);
}

bool ShogiGameController::applyMove(const UsiMove& move, QString& record, int moveNumber,
                                    QStringList* sfenHistory, QList<ShogiMove>& gameMoves)
{
    if (!board() || !move.isValid() || (currentPlayer() != Turn::Black && currentPlayer() != Turn::White)) {
        qCWarning(lcGame).noquote() << "applyMove: rejected" << move.toString();
        return false;
    }

    const Turn mover = (currentPlayer() == Turn::Black) ? Turn::Black : Turn::White;
    const Piece moving = move.isDrop() ? move.dropPieceFor(mover)
                                       : board()->pieceCharacter(move.from.file, move.from.rank);
    if (moving == Piece::None) {
        qCWarning(lcGame).noquote() << "applyMove: the source square is empty:" << move.toString();
        return false;
    }
    const Piece captured = board()->pieceCharacter(move.to.file, move.to.rank);

    // エンジン・通信対局の手も、人間の手と同じく合法手か確かめる（成り・不成も指し手どおりに判定する）
    const LegalMoveStatus status = legalStatus(move, mover, moving, captured);
    if (move.promote ? !status.promotingMoveExists : !status.nonPromotingMoveExists) {
        qCWarning(lcGame).noquote() << "applyMove: illegal move" << move.toString();
        return false;
    }

    return commitMove(move, mover, moving, captured, record, moveNumber, sfenHistory, gameMoves);
}

bool ShogiGameController::commitMove(const UsiMove& move, Turn mover, Piece moving, Piece captured, QString& record,
                                     int moveNumber, QStringList* sfenHistory, QList<ShogiMove>& gameMoves)
{
    const Turn nextPlayerTurn = (mover == Turn::Black) ? Turn::White : Turn::Black;

    if (!board()->applyUsiMove(move, mover)) {
        qCWarning(lcGame).noquote() << "commitMove: the board rejected" << move.toString();
        return false;
    }
    gameMoves.append(ShogiMove::fromUsiMove(move, mover, moving, captured));
    board()->addSfenRecord(nextPlayerTurn, moveNumber, sfenHistory);
    if (!sfenHistory) {
        qCWarning(lcGame) << "commitMove: sfen history is null";
    }

    record = convertMoveToKanjiStr(moving, move);
    m_lastMove = move;

    // 着手確定シグナル: 手番切替の「前」に出す
    const Turn moverBefore = currentPlayer();
    const int confirmedPly = static_cast<int>(gameMoves.size());
    qCDebug(lcGame) << "emit moveCommitted mover=" << moverBefore << "ply=" << confirmedPly;
    emit moveCommitted(moverBefore, confirmedPly);

    setCurrentPlayer(nextPlayerTurn == Turn::Black ? Turn::Black : Turn::White);
    return true;
}

// ============================================================
// 局面編集
// ============================================================

bool ShogiGameController::editPosition(const QPoint& outFrom, const QPoint& outTo, Turn pieceBoxSide)
{
    if (!board()) {
        qCWarning(lcGame) << "editPosition: board() is null.";
        return false;
    }

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


// ============================================================
// 対局結果
// ============================================================

void ShogiGameController::gameResult()
{
    if (currentPlayer() == Turn::White) {
        setResult(Player1Wins);
    }
    else if (currentPlayer() == Turn::Black) {
        setResult(Player2Wins);
    }
}

void ShogiGameController::switchPiecePromotionStatusOnRightClick(const int fileFrom, const int rankFrom) const
{
    if (!board()) return;

    // 二歩や段の必成などの禁止形スキップは board 側で一括処理する
    board()->promoteOrDemotePiece(fileFrom, rankFrom);
}



// ============================================================
// SFEN更新
// ============================================================

// ============================================================
// 対局結果制御（時間切れ・投了・最終決定）
// ============================================================

void ShogiGameController::applyTimeoutLossFor(int clockPlayer)
{
    if (m_result != NoResult) return;

    if (clockPlayer == 1) {
        setResult(Player2Wins);
    } else if (clockPlayer == 2) {
        setResult(Player1Wins);
    } else {
        qCWarning(lcGame).noquote() << "applyTimeoutLossFor: invalid clockPlayer =" << clockPlayer;
    }
}

void ShogiGameController::finalizeGameResult()
{
    if (m_result != NoResult) return;

    // 未確定の場合は currentPlayer に基づく既存ロジックで勝敗をセット
    gameResult();
}
