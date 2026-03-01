/// @file shogigamecontroller.cpp
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラの実装

#include "shogigamecontroller.h"

#include <QPoint>
#include <QDebug>

#include "logcategories.h"
#include "matchcoordinator.h"
#include "piecemoverules.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "playmode.h"
#include "shogiutils.h"
#include "legalmovestatus.h"

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

    // SFEN の手番フィールドから GC の手番を同期
    const Turn boardTurn = board()->currentPlayer();
    setCurrentPlayer(boardTurn == Turn::White ? Player2 : Player1);

    // 前の対局の最終手の移動先が残ると「同」表記が誤表示されるためリセット
    previousFileTo = 0;
    previousRankTo = 0;
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

bool ShogiGameController::promote() const
{
    return m_promote;
}

void ShogiGameController::setPromote(bool newPromote)
{
    m_promote = newPromote;
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

void ShogiGameController::setCurrentPlayer(const Player player)
{
    if (currentPlayer() == player) return;

    m_currentPlayer = player;
    emit currentPlayerChanged(m_currentPlayer);
}

void ShogiGameController::changeCurrentPlayer()
{
    setCurrentPlayer(currentPlayer() == Player1 ? Player2 : Player1);
}

Turn ShogiGameController::getNextPlayerSfen()
{
    if (currentPlayer() == Player1) {
        return Turn::White;
    } else if (currentPlayer() == Player2) {
        return Turn::Black;
    } else {
        qCWarning(lcGame) << "getNextPlayerSfen: Invalid player state. currentPlayer() =" << currentPlayer();
        return Turn::Black;
    }
}

EngineMoveValidator::Turn ShogiGameController::getCurrentTurnForValidator(EngineMoveValidator& validator)
{
    if (currentPlayer() == Player1) {
        return validator.BLACK;
    }
    else {
        return validator.WHITE;
    }
}

// ============================================================
// 棋譜文字列変換
// ============================================================

QString ShogiGameController::convertMoveToKanjiStr(const QString piece, const int fileFrom, const int rankFrom, const int fileTo, const int rankTo)
{
    if (currentPlayer() != Player1 && currentPlayer() != Player2) {
        qCWarning(lcGame) << "convertMoveToKanjiStr: current player is invalid.";
        return QString();
    }

    // 「全、圭、杏」はそれぞれ「成銀、成桂、成香」とする
    QString pieceKanji;
    if (piece == "全") {
        pieceKanji = "成銀";
    } else if (piece == "圭") {
        pieceKanji = "成桂";
    } else if (piece == "杏") {
        pieceKanji = "成香";
    } else {
        pieceKanji = piece;
    }

    QString moveStr;

    if (currentPlayer() == Player1) {
        moveStr = "▲";
    } else { // Player2
        moveStr = "△";
    }

    // 前手と同じマスへの移動は「同」表記にする
    if ((fileTo == previousFileTo) && (rankTo == previousRankTo)) {
        moveStr += "同　" + pieceKanji;
    } else {
        moveStr += ShogiUtils::transFileTo(fileTo) + ShogiUtils::transRankTo(rankTo) + pieceKanji;
    }

    if (m_promote) moveStr += "成";

    // 駒台から打つ場合（10=先手駒台、11=後手駒台）
    if ((fileFrom >= 10) && (fileFrom <= 11)) {
        moveStr += "打";
    } else {
        moveStr += "(" + QString::number(fileFrom) + QString::number(rankFrom) + ")";
    }

    previousFileTo = fileTo;
    previousRankTo = rankTo;

    return moveStr;
}

QString ShogiGameController::getPieceKanji(const Piece piece)
{
    static const QMap<QChar, QString> pieceKanjiNames = {
        {' ', "　"},
        {'P', "歩"}, {'L', "香"}, {'N', "桂"}, {'S', "銀"}, {'G', "金"}, {'B', "角"}, {'R', "飛"}, {'K', "玉"},
        {'Q', "と"}, {'M', "杏"}, {'O', "圭"}, {'T', "全"}, {'C', "馬"}, {'U', "龍"},
        {'p', "歩"}, {'l', "香"}, {'n', "桂"}, {'s', "銀"}, {'g', "金"}, {'b', "角"}, {'r', "飛"}, {'k', "玉"},
        {'q', "と"}, {'m', "杏"}, {'o', "圭"}, {'t', "全"}, {'c', "馬"}, {'u', "龍"}
    };

    const QChar pieceChar = pieceToChar(piece);
    auto it = pieceKanjiNames.find(pieceChar);

    if (it != pieceKanjiNames.end()) {
        return it.value();
    }
    else {
        qCWarning(lcGame) << "getPieceKanji: The piece" << pieceChar << "is not found.";
    }

    return QString();
}

// ============================================================
// 成り判定
// ============================================================

bool ShogiGameController::decidePromotion(PlayMode& playMode, EngineMoveValidator& validator,
                                          const EngineMoveValidator::Turn& turnMove,
                                          int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, Piece piece, ShogiMove& currentMove)
{
    // 駒台には指せない
    if (fileTo >= 10) return false;

    if (isCurrentPlayerHumanControlled(playMode)) {
        currentMove.isPromotion = false;

        LegalMoveStatus legalMoveStatus = validator.isLegalMove(turnMove, board()->boardData(), board()->getPieceStand(), currentMove);

        bool canMoveWithoutPromotion = legalMoveStatus.nonPromotingMoveExists;
        bool canMoveWithPromotion = legalMoveStatus.promotingMoveExists;

        if (canMoveWithPromotion) {
            if (canMoveWithoutPromotion) {
                // 盤上の指し手の場合、対局者に成り/不成を選択させる
                if (fileFrom <= 9) {
                    // 強制成りモードの場合はダイアログをスキップ
                    if (m_forcedPromotionMode) {
                        qCDebug(lcGame) << "decidePromotion: using forced promotion value=" << m_forcedPromotionValue;
                        m_promote = m_forcedPromotionValue;
                        currentMove.isPromotion = m_forcedPromotionValue;
                        // 強制モードは1回使用したらクリア
                        m_forcedPromotionMode = false;
                        m_forcedPromotionValue = false;
                        return true;
                    }

                    decidePromotionByDialog(piece, rankFrom, rankTo);
                    currentMove.isPromotion = m_promote;
                    return true;
                }
            }
            // 必ず成る必要がある場合
            else {
                currentMove.isPromotion = true;
            }
        } else {
            if (canMoveWithoutPromotion) {
                currentMove.isPromotion = false;
            } else {
                // 成りも不成も不合法
                return false;
            }
        }
    }

    m_promote = currentMove.isPromotion;
    return true;
}

bool ShogiGameController::isCurrentPlayerHumanControlled(PlayMode& playMode)
{
    auto player = currentPlayer();

    return (playMode == PlayMode::HumanVsHuman)
           || (player == Player1 && playMode == PlayMode::EvenHumanVsEngine)
           || (player == Player2 && playMode == PlayMode::EvenEngineVsHuman)
           || (player == Player1 && playMode == PlayMode::HandicapHumanVsEngine)
           || (player == Player2 && playMode == PlayMode::HandicapEngineVsHuman)
           || (playMode == PlayMode::CsaNetworkMode);
}

void ShogiGameController::decidePromotionByDialog(Piece piece, int& rankFrom, int& rankTo)
{
    auto player = currentPlayer();

    if (isPromotablePiece(piece)) {
        if (player == Player1) {
            // 先手: 移動元が4段目以降、または移動先が3段目以内なら選択させる
            if (rankFrom < 4 || (rankFrom >= 4 && rankTo <= 3)) {
                emit showPromotionDialog();
            }
        }
        else if (player == Player2) {
            // 後手: 移動元が6段目以前、または移動先が7段目以降なら選択させる
            if (rankFrom > 6 || (rankFrom <= 6 && rankTo >= 7)) {
                emit showPromotionDialog();
            }
        }
    }
}

bool ShogiGameController::isPromotablePiece(Piece piece)
{
    switch (piece) {
    case Piece::BlackPawn:   case Piece::BlackLance:  case Piece::BlackKnight:
    case Piece::BlackSilver: case Piece::BlackBishop: case Piece::BlackRook:
    case Piece::WhitePawn:   case Piece::WhiteLance:  case Piece::WhiteKnight:
    case Piece::WhiteSilver: case Piece::WhiteBishop: case Piece::WhiteRook:
        return true;
    default:
        return false;
    }
}

// ============================================================
// 指し手検証・盤面更新
// ============================================================

bool ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, PlayMode& playMode, int& moveNumber,
                                          QStringList* m_sfenHistory, QVector<ShogiMove>& gameMoves)
{
    // 処理フロー:
    // 1. 入力ガード（盤面・移動元の検証）
    // 2. 成り/不成の判定
    // 3. 盤面更新・SFEN記録
    // 4. 棋譜文字列生成
    // 5. 着手確定シグナル発行・手番切替

    qCDebug(lcGame).noquote() << "validateAndMove enter argMove=" << moveNumber
                      << " recPtr=" << static_cast<const void*>(m_sfenHistory)
                      << " recSize(before)=" << (m_sfenHistory ? m_sfenHistory->size() : -1);

    if (!board()) {
        qCWarning(lcGame) << "validateAndMove: board() is null.";
        emit endDragSignal();
        return false;
    }

    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo   = outTo.x();
    int rankTo   = outTo.y();

    qCDebug(lcGame) << "in ShogiGameController::validateAndMove";
    qCDebug(lcGame) << "playMode = " << static_cast<int>(playMode);
    qCDebug(lcGame) << "promote = " << m_promote;
    qCDebug(lcGame) << "fileFrom = " << fileFrom;
    qCDebug(lcGame) << "rankFrom = " << rankFrom;
    qCDebug(lcGame) << "fileTo = " << fileTo;
    qCDebug(lcGame) << "rankTo = " << rankTo;
    qCDebug(lcGame) << "currentPlayer() = " << currentPlayer();

    EngineMoveValidator validator;
    EngineMoveValidator::Turn turn = getCurrentTurnForValidator(validator);

    QPoint fromPoint(fileFrom - 1, rankFrom - 1);
    QPoint toPoint(fileTo - 1, rankTo - 1);

    Piece movingPiece   = board()->getPieceCharacter(fileFrom, rankFrom);
    Piece capturedPiece = board()->getPieceCharacter(fileTo,   rankTo);

    // 盤上移動のとき、移動元が空白なら不正
    if ((fileFrom >= 1 && fileFrom <= 9) && movingPiece == Piece::None) {
        qCWarning(lcGame) << "validateAndMove: the source square is empty.";
        emit endDragSignal();
        return false;
    }

    ShogiMove currentMove(fromPoint, toPoint, movingPiece, capturedPiece, m_promote);

    if (!decidePromotion(playMode, validator, turn, fileFrom, rankFrom, fileTo, rankTo, movingPiece, currentMove)) {
        emit endDragSignal();
        return false;
    } else {
        emit endDragSignal();
    }

    // 手番SFENの取得を先に行い、異常時は盤面更新前に打ち切る
    const Turn nextPlayerTurn = getNextPlayerSfen();

    gameMoves.append(currentMove);

    board()->updateBoardAndPieceStand(movingPiece, capturedPiece, fileFrom, rankFrom, fileTo, rankTo, m_promote);


    qCDebug(lcGame).noquote() << "pre-add: nextTurn=" << turnToSfen(nextPlayerTurn)
                       << " moveIndex=" << moveNumber
                       << " rec*=" << static_cast<const void*>(m_sfenHistory)
                       << " size(before)=" << (m_sfenHistory ? m_sfenHistory->size() : -1);

    board()->addSfenRecord(nextPlayerTurn, moveNumber, m_sfenHistory);

    qCDebug(lcGame).noquote() << "post-add: size(after)=" << (m_sfenHistory ? m_sfenHistory->size() : -1)
                       << " head=" << (m_sfenHistory && !m_sfenHistory->isEmpty() ? m_sfenHistory->first() : QString())
                       << " tail=" << (m_sfenHistory && !m_sfenHistory->isEmpty() ? m_sfenHistory->last()  : QString());

    if (m_sfenHistory) {
        const qsizetype n = m_sfenHistory->size();
        const QString last = (n > 0) ? m_sfenHistory->at(n - 1) : QString();
        const QString preview = (last.size() > 200) ? last.left(200) + " ..." : last;
        qCDebug(lcGame) << "validateAndMove: sfenRecord size =" << n
                        << " moveNumber =" << moveNumber;
        qCDebug(lcGame).noquote() << "last sfen = " << preview;
        if (last.startsWith(QLatin1String("position "))) {
            qCWarning(lcGame) << "*** NON-SFEN stored into sfenRecord! (bug)";
        }
    } else {
        qCWarning(lcGame) << "validateAndMove: sfen history is null";
    }

    QString kanjiPiece = getPieceKanji(movingPiece);

    record = convertMoveToKanjiStr(kanjiPiece, fileFrom, rankFrom, fileTo, rankTo);
    if (record.isEmpty()) {
        return false;
    }

    // 着手確定シグナル: 手番切替の「前」に出す
    const Player moverBefore   = currentPlayer();
    const int confirmedPly     = static_cast<int>(gameMoves.size());
    qCDebug(lcGame) << "emit moveCommitted mover=" << moverBefore << "ply=" << confirmedPly;
    emit moveCommitted(moverBefore, confirmedPly);

    setCurrentPlayer(currentPlayer() == Player1 ? Player2 : Player1);

    if (m_sfenHistory && !m_sfenHistory->isEmpty()) {
        const QString tail = m_sfenHistory->last();
        qCDebug(lcGame).noquote() << "validateAndMove exit argMove=" << moveNumber
                          << " recSize(after)=" << m_sfenHistory->size()
                          << " tail='" << tail << "'";
    } else {
        qCDebug(lcGame).noquote() << "validateAndMove exit argMove=" << moveNumber
                          << " recSize(after)=" << (m_sfenHistory ? m_sfenHistory->size() : -1)
                          << " tail=<empty>";
    }

    return true;
}

// ============================================================
// 局面編集
// ============================================================

bool ShogiGameController::editPosition(const QPoint& outFrom, const QPoint& outTo)
{
    if (!board()) {
        qCWarning(lcGame) << "editPosition: board() is null.";
        return false;
    }

    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    Piece source = board()->getPieceCharacter(fileFrom, rankFrom);
    Piece dest = board()->getPieceCharacter(fileTo, rankTo);

    // 移動元の駒の先手/後手から手番を推定
    if (isBlackPiece(source)) {
        setCurrentPlayer(Player1);
    } else {
        setCurrentPlayer(Player2);
    }

    if (!PieceMoveRules::checkMovePiece(board(), source, dest, fileFrom, fileTo)) return false;

    m_promote = PieceMoveRules::shouldAutoPromote(fileTo, rankTo, source);

    board()->updateBoardAndPieceStand(source, dest, fileFrom, rankFrom, fileTo, rankTo, m_promote);

    return true;
}


// ============================================================
// 対局結果
// ============================================================

void ShogiGameController::gameResult()
{
    if (currentPlayer() == Player2) {
        setResult(Player1Wins);
    }
    else if (currentPlayer() == Player1) {
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
