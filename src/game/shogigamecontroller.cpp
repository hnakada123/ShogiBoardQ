/// @file shogigamecontroller.cpp
/// @brief 将棋の対局進行・盤面更新・合法手検証を管理するコントローラの実装

#include "shogigamecontroller.h"

#include <QPoint>
#include <QDebug>

#include "matchcoordinator.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "movevalidator.h"
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

ShogiBoard* ShogiGameController::board() const
{
    return m_board;
}

void ShogiGameController::newGame(QString& initialSfenString)
{
    qCDebug(lcGame) << "ShogiGameController::newGame called";
    qCDebug(lcGame) << "  Initial SFEN:" << initialSfenString;
    qCDebug(lcGame) << "  Current board ptr:" << m_board;

    setupBoard();

    qCDebug(lcGame) << "  New board ptr after setupBoard:" << m_board;

    board()->setSfen(initialSfenString);
    setResult(NoResult);

    // SFEN の手番フィールドから GC の手番を同期
    const QString boardTurn = board()->currentPlayer();
    setCurrentPlayer((boardTurn == QLatin1String("w")) ? Player2 : Player1);

    // 前の対局の最終手の移動先が残ると「同」表記が誤表示されるためリセット
    previousFileTo = 0;
    previousRankTo = 0;
}

void ShogiGameController::setupBoard()
{
    setBoard(new ShogiBoard(9, 9, this));
}

void ShogiGameController::setBoard(ShogiBoard* board)
{
    if (!board) {
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::setBoard: null board was passed.");
        emit errorOccurred(errorMessage);
        return;
    }

    if (board == m_board) return;

    if (m_board) delete m_board;

    m_board = board;
    emit boardChanged(m_board);
}

// ============================================================
// 対局結果
// ============================================================

void ShogiGameController::setResult(Result value)
{
    if (result() == value) return;

    // 初回設定時のみ gameOver シグナルを発行する
    if (result() == NoResult) {
        m_result = value;
        emit gameOver(m_result);
    } else {
        m_result = value;
    }
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

void ShogiGameController::clearForcedPromotion()
{
    m_forcedPromotionMode = false;
    m_forcedPromotionValue = false;
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

QString ShogiGameController::getNextPlayerSfen()
{
    QString nextPlayerColorSfen;

    if (currentPlayer() == Player1) {
        nextPlayerColorSfen = QStringLiteral("w");
    } else if (currentPlayer() == Player2) {
        nextPlayerColorSfen = QStringLiteral("b");
    } else {
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::getNextPlayerSfen: Invalid player state.");
        qCDebug(lcGame) << "currentPlayer() =" << currentPlayer();
        emit errorOccurred(errorMessage);
        return QString();
    }

    return nextPlayerColorSfen;
}

MoveValidator::Turn ShogiGameController::getCurrentTurnForValidator(MoveValidator& validator)
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
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::convertMoveToKanjiStr: current player is invalid.");
        emit errorOccurred(errorMessage);
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

QString ShogiGameController::getPieceKanji(const QChar& piece)
{
    static const QMap<QChar, QString> pieceKanjiNames = {
        {' ', "　"},
        {'P', "歩"}, {'L', "香"}, {'N', "桂"}, {'S', "銀"}, {'G', "金"}, {'B', "角"}, {'R', "飛"}, {'K', "玉"},
        {'Q', "と"}, {'M', "杏"}, {'O', "圭"}, {'T', "全"}, {'C', "馬"}, {'U', "龍"},
        {'p', "歩"}, {'l', "香"}, {'n', "桂"}, {'s', "銀"}, {'g', "金"}, {'b', "角"}, {'r', "飛"}, {'k', "玉"},
        {'q', "と"}, {'m', "杏"}, {'o', "圭"}, {'t', "全"}, {'c', "馬"}, {'u', "龍"}
    };

    auto it = pieceKanjiNames.find(piece);

    if (it != pieceKanjiNames.end()) {
        return it.value();
    }
    else {
        const QString errorMessage = tr("An error occurred in ShogiGameController::getPieceKanji: The piece %1 is not found.").arg(piece);
        emit errorOccurred(errorMessage);
    }

    return QString();
}

// ============================================================
// 成り判定
// ============================================================

bool ShogiGameController::decidePromotion(PlayMode& playMode, MoveValidator& validator, const MoveValidator::Turn& turnMove,
                                     int& fileFrom, int& rankFrom, int& fileTo, int& rankTo, QChar& piece,  ShogiMove& currentMove)
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

void ShogiGameController::decidePromotionByDialog(QChar& piece, int& rankFrom, int& rankTo)
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

bool ShogiGameController::isPromotablePiece(QChar& piece)
{
    static const QString promotablePieces = "PLNSBRplnsbr";
    return promotablePieces.contains(piece);
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
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::validateAndMove: board() is null.");
        emit errorOccurred(errorMessage);
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

    MoveValidator validator;
    MoveValidator::Turn turn = getCurrentTurnForValidator(validator);

    QPoint fromPoint(fileFrom - 1, rankFrom - 1);
    QPoint toPoint(fileTo - 1, rankTo - 1);

    QChar movingPiece   = board()->getPieceCharacter(fileFrom, rankFrom);
    QChar capturedPiece = board()->getPieceCharacter(fileTo,   rankTo);

    // 盤上移動のとき、移動元が空白なら不正
    if ((fileFrom >= 1 && fileFrom <= 9) && movingPiece == QLatin1Char(' ')) {
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::validateAndMove: the source square is empty.");
        emit errorOccurred(errorMessage);
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
    QString nextPlayerColorSfen = getNextPlayerSfen();
    if (nextPlayerColorSfen.isEmpty()) {
        return false;
    }

    gameMoves.append(currentMove);

    board()->updateBoardAndPieceStand(movingPiece, capturedPiece, fileFrom, rankFrom, fileTo, rankTo, m_promote);


    qCDebug(lcGame).noquote() << "pre-add: nextTurn=" << nextPlayerColorSfen
                       << " moveIndex=" << moveNumber
                       << " rec*=" << static_cast<const void*>(m_sfenHistory)
                       << " size(before)=" << (m_sfenHistory ? m_sfenHistory->size() : -1);

    board()->addSfenRecord(nextPlayerColorSfen, moveNumber, m_sfenHistory);

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
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::editPosition: board() is null.");
        emit errorOccurred(errorMessage);
        return false;
    }

    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    QChar source = board()->getPieceCharacter(fileFrom, rankFrom);
    QChar dest = board()->getPieceCharacter(fileTo, rankTo);

    // 移動元の駒の大文字/小文字から手番を推定
    if (source.isUpper()) {
        setCurrentPlayer(Player1);
    } else {
        setCurrentPlayer(Player2);
    }

    if (!checkMovePiece(source, dest, fileFrom, fileTo)) return false;

    setMandatoryPromotionFlag(fileTo, rankTo, source);

    board()->updateBoardAndPieceStand(source, dest, fileFrom, rankFrom, fileTo, rankTo, m_promote);

    return true;
}

// ============================================================
// 禁じ手チェック
// ============================================================

bool ShogiGameController::checkMovePiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    if (!checkTwoPawn(source, fileFrom, fileTo)) return false;
    if (!checkWhetherAllyPiece(source, dest, fileFrom, fileTo)) return false;
    if (!checkNumberStandPiece(source, fileFrom)) return false;
    if (!checkFromPieceStandToPieceStand(source, dest, fileFrom, fileTo)) return false;
    if (!checkGetKingOpponentPiece(source, dest)) return false;

    return true;
}

bool ShogiGameController::checkNumberStandPiece(const QChar source, const int fileFrom) const
{
    return board()->isPieceAvailableOnStand(source, fileFrom);
}

bool ShogiGameController::checkWhetherAllyPiece(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    if (fileTo < 10) {
        // 同じ大文字/小文字同士の場合は味方の駒
        if (source.isUpper() && dest.isUpper()) {
            if ((fileFrom < 10) && (fileTo > 9)) {
                return true;
            } else {
                return false;
            }
        }
        if (source.isLower() && dest.isLower()) {
            if ((fileFrom < 10) && (fileTo > 9)) {
                return true;
            } else {
                return false;
            }
        }
    }

    return true;
}

bool ShogiGameController::checkTwoPawn(const QChar source, const int fileFrom, const int fileTo) const
{
    // 同じ筋内の移動は二歩にならない
    if (fileFrom == fileTo) return true;

    if (source == 'P') {
        if (fileTo < 10) {
            for (int rank = 1; rank <= 9; rank++) {
                if (board()->getPieceCharacter(fileTo, rank) == 'P') return false;
            }
        }
    }

    if (source == 'p') {
        if (fileTo < 10) {
            for (int rank = 1; rank <= 9; rank++) {
                if (board()->getPieceCharacter(fileTo, rank) == 'p') return false;
            }
        }
    }

    return true;
}

void ShogiGameController::setMandatoryPromotionFlag(const int fileTo, const int rankTo, const QChar source)
{
    m_promote = false;

    // 駒台への移動は成りなし
    if (fileTo >= 10) {
        return;
    }

    // 先手: 歩・香は1段目、桂は1-2段目で必ず成る
    if ((source == 'P' && rankTo == 1) ||
        (source == 'L' && rankTo == 1) ||
        (source == 'N' && rankTo <= 2)) {
        m_promote = true;
        return;
    }

    // 後手: 歩・香は9段目、桂は8-9段目で必ず成る
    if ((source == 'p' && rankTo == 9) ||
        (source == 'l' && rankTo == 9) ||
        (source == 'n' && rankTo >= 8)) {
        m_promote = true;
        return;
    }
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


bool ShogiGameController::checkFromPieceStandToPieceStand(const QChar source, const QChar dest, const int fileFrom, const int fileTo) const
{
    // 先手駒台→後手駒台: 同種の駒のみ移動可能
    if ((fileFrom == 10) && (fileTo == 11)) {
        QChar c = dest.toUpper();
        if (source == c) {
            return true;
        } else {
            return false;
        }
    }
    // 後手駒台→先手駒台: 同種の駒のみ移動可能
    if ((fileFrom == 11) && (fileTo == 10)) {
        QChar c = dest.toLower();
        if (source == c) {
            return true;
        } else {
            return false;
        }
    }

    return true;
}

bool ShogiGameController::checkGetKingOpponentPiece(const QChar source, const QChar dest) const
{
    // 後手の駒で先手玉は取れない
    if ((dest == 'K') && source.isLower()) return false;

    // 先手の駒で後手玉は取れない
    if ((dest == 'k') && source.isUpper()) return false;

    return true;
}

// ============================================================
// SFEN更新
// ============================================================

void ShogiGameController::updateSfenRecordAfterEdit(QStringList* m_sfenHistory)
{
    if (!board()) {
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::updateSfenRecordAfterEdit: board() is null.");
        emit errorOccurred(errorMessage);
        return;
    }
    if (!m_sfenHistory) {
        const QString errorMessage =
            tr("An error occurred in ShogiGameController::updateSfenRecordAfterEdit: record list is null.");
        emit errorOccurred(errorMessage);
        return;
    }

    // moveIndex=-1 を渡すと addSfenRecord 側の (+2) で " 1 " になる仕様
    const int moveIndex = -1;

    // 編集メニューの「手番変更」に追従
    const QString nextTurn = (currentPlayer() == ShogiGameController::Player1)
                                 ? QStringLiteral("b") : QStringLiteral("w");

    board()->addSfenRecord(nextTurn, moveIndex, m_sfenHistory);
}

QPoint ShogiGameController::lastMoveTo() const
{
    return QPoint(previousFileTo, previousRankTo);
}

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

void ShogiGameController::applyResignationOfCurrentSide()
{
    if (m_result != NoResult) return;

    if (m_currentPlayer == Player1) {
        setResult(Player2Wins);
    } else if (m_currentPlayer == Player2) {
        setResult(Player1Wins);
    } else {
        qCWarning(lcGame).noquote() << "applyResignationOfCurrentSide: currentPlayer is NoPlayer, defaulting to Draw";
        setResult(Draw);
    }
}

void ShogiGameController::finalizeGameResult()
{
    if (m_result != NoResult) return;

    // 未確定の場合は currentPlayer に基づく既存ロジックで勝敗をセット
    gameResult();
}
