/// @file usimatchhandler.cpp
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの実装

#include "usimatchhandler.h"
#include "logcategories.h"
#include "parsecommon.h"
#include "shogiboard.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "thinkinginfopresenter.h"
#include "usiprotocolhandler.h"

namespace {

/// QVector<Piece> → QVector<QChar> 変換（ShogiBoard::boardData() → 内部クローン用）
QVector<QChar> pieceVectorToCharVector(const QVector<Piece>& pieces)
{
    QVector<QChar> chars;
    chars.reserve(pieces.size());
    for (const Piece p : pieces) {
        chars.append(pieceToChar(p));
    }
    return chars;
}

void ensureMovesKeyword(QString& s)
{
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}

/// USI形式の指し手をShogiBoard上に適用するローカルヘルパ
void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isSenteMove)
{
    if (usiMove.length() < 4) return;

    bool promote = (usiMove.length() >= 5 && usiMove.at(4) == QLatin1Char('+'));

    if (usiMove.at(1) == QLatin1Char('*')) {
        // 駒打ち: "P*5e"
        auto fileTo = KifuParseCommon::parseFileChar(usiMove.at(2));
        auto rankTo = KifuParseCommon::parseRankChar(usiMove.at(3));
        if (!fileTo || !rankTo) return;

        QChar pieceChar = isSenteMove ? usiMove.at(0).toUpper() : usiMove.at(0).toLower();
        Piece piece = charToPiece(pieceChar);

        board->decrementPieceOnStand(piece);
        board->movePieceToSquare(piece, 10, 0, *fileTo, *rankTo, false);
    } else {
        // 盤上の移動: "8c8d" or "8c8d+"
        auto fileFrom = KifuParseCommon::parseFileChar(usiMove.at(0));
        auto rankFrom = KifuParseCommon::parseRankChar(usiMove.at(1));
        auto fileTo = KifuParseCommon::parseFileChar(usiMove.at(2));
        auto rankTo = KifuParseCommon::parseRankChar(usiMove.at(3));

        if (!fileFrom || !rankFrom || !fileTo || !rankTo) return;

        Piece movingPiece = board->getPieceCharacter(*fileFrom, *rankFrom);
        Piece capturedPiece = board->getPieceCharacter(*fileTo, *rankTo);

        if (capturedPiece != Piece::None) {
            board->addPieceToStand(capturedPiece);
        }

        board->movePieceToSquare(movingPiece, *fileFrom, *rankFrom, *fileTo, *rankTo, promote);
    }
}

} // anonymous namespace

// ============================================================
// 構築
// ============================================================

UsiMatchHandler::UsiMatchHandler(UsiProtocolHandler* protocolHandler,
                                 ThinkingInfoPresenter* presenter,
                                 ShogiGameController* gameController)
    : m_protocolHandler(protocolHandler)
    , m_presenter(presenter)
    , m_gameController(gameController)
{
}

void UsiMatchHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 盤面データ管理
// ============================================================

void UsiMatchHandler::cloneCurrentBoardData()
{
    qCDebug(lcEngine) << "cloneCurrentBoardData: gameController=" << m_gameController;
    if (!m_gameController) {
        qCWarning(lcEngine) << "cloneCurrentBoardData: gameControllerがnull";
        return;
    }
    qCDebug(lcEngine) << "cloneCurrentBoardData: board=" << m_gameController->board();
    if (!m_gameController->board()) {
        qCWarning(lcEngine) << "cloneCurrentBoardData: boardがnull";
        return;
    }
    m_clonedBoardData = pieceVectorToCharVector(m_gameController->board()->boardData());
    qCDebug(lcEngine) << "cloneCurrentBoardData: size=" << m_clonedBoardData.size();
    m_presenter->setClonedBoardData(m_clonedBoardData);
}

void UsiMatchHandler::prepareBoardDataForAnalysis()
{
    qCDebug(lcEngine) << "prepareBoardDataForAnalysis";
    if (m_gameController && m_gameController->board()) {
        m_clonedBoardData = pieceVectorToCharVector(m_gameController->board()->boardData());
        qCDebug(lcEngine) << "盤面クローン完了: size=" << m_clonedBoardData.size();
        m_presenter->setClonedBoardData(m_clonedBoardData);
    } else {
        qCWarning(lcEngine) << "prepareBoardDataForAnalysis: gameControllerまたはboardがnull";
    }
}

void UsiMatchHandler::setClonedBoardData(const QVector<QChar>& boardData)
{
    m_clonedBoardData = boardData;
    if (m_presenter) {
        m_presenter->setClonedBoardData(m_clonedBoardData);
    }
}

QString UsiMatchHandler::computeBaseSfenFromBoard() const
{
    if (!m_gameController || !m_gameController->board()) return QString();

    ShogiBoard* board = m_gameController->board();
    // board->currentPlayer() は setSfen() 時に SFEN の手番フィールドから設定されるため、
    // 棋譜ナビゲーション後も正しい手番を返す。
    // m_gameController->currentPlayer() は対局中の手番管理用であり、
    // ナビゲーション時には更新されないため使用しない。
    const Turn boardTurn = board->currentPlayer();
    const QString turn = turnToSfen(boardTurn);
    return board->convertBoardToSfen() + QStringLiteral(" ") + turn +
           QStringLiteral(" ") + board->convertStandToSfen() + QStringLiteral(" 1");
}

// ============================================================
// 最終指し手管理
// ============================================================

QString UsiMatchHandler::lastUsiMove() const
{
    return m_lastUsiMove;
}

void UsiMatchHandler::setLastUsiMove(const QString& move)
{
    qCDebug(lcEngine) << "setLastUsiMove:" << move;
    m_lastUsiMove = move;
}

// ============================================================
// 盤面処理（内部）
// ============================================================

void UsiMatchHandler::applyMovesToBoardFromBestMoveAndPonder()
{
    ShogiEngineInfoParser info;
    info.parseAndApplyMoveToClonedBoard(m_protocolHandler->bestMove(), m_clonedBoardData);
    info.parseAndApplyMoveToClonedBoard(m_protocolHandler->predictedMove(), m_clonedBoardData);
    m_presenter->setClonedBoardData(m_clonedBoardData);
}

void UsiMatchHandler::updateBaseSfenForPonder()
{
    QString currentSfen = m_presenter->baseSfen();
    if (currentSfen.isEmpty()) return;

    // 現在のbaseSfenから手番を取得
    QStringList sfenParts = currentSfen.split(QLatin1Char(' '));
    if (sfenParts.size() < 2) return;
    bool isSente = (sfenParts.at(1) == QStringLiteral("b"));

    // 一時的なShogiBoardを作成し、現在のbaseSfenを設定
    ShogiBoard tempBoard;
    tempBoard.setSfen(currentSfen);

    // bestmove（現在の手番のプレイヤーの指し手）を適用
    applyUsiMoveToBoard(&tempBoard, m_protocolHandler->bestMove(), isSente);

    // predictedMove（相手の予測手）を適用
    applyUsiMoveToBoard(&tempBoard, m_protocolHandler->predictedMove(), !isSente);

    // ポンダー局面のSFENを生成（2手適用後なので手番は元と同じ）
    QString ponderTurn = isSente ? QStringLiteral("b") : QStringLiteral("w");
    QString ponderBaseSfen = tempBoard.convertBoardToSfen() + QStringLiteral(" ") + ponderTurn +
                             QStringLiteral(" ") + tempBoard.convertStandToSfen() + QStringLiteral(" 1");

    qCDebug(lcEngine) << "updateBaseSfenForPonder:" << ponderBaseSfen.left(80);
    m_presenter->setBaseSfen(ponderBaseSfen);
}

QString UsiMatchHandler::convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote)
{
    return m_protocolHandler->convertHumanMoveToUsi(outFrom, outTo, promote);
}

// ============================================================
// 対局通信処理
// ============================================================

void UsiMatchHandler::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                                       QPoint& outFrom, QPoint& outTo,
                                                       int byoyomiMilliSec, const QString& btime,
                                                       const QString& wtime, QStringList& positionStrList,
                                                       int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                                       bool useByoyomi)
{
    // 人間の指し手をUSI形式に変換
    QString bestMove = convertHumanMoveToUsiFormat(outFrom, outTo, m_gameController->promote());

    ensureMovesKeyword(positionStr);
    positionStr += " " + bestMove;
    positionStrList.append(positionStr);

    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                               byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

void UsiMatchHandler::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                                    QString& positionPonderStr,
                                                                    QPoint& outFrom, QPoint& outTo,
                                                                    int byoyomiMilliSec, const QString& btime,
                                                                    const QString& wtime,
                                                                    int addEachMoveMilliSec1,
                                                                    int addEachMoveMilliSec2, bool useByoyomi)
{
    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                               byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

void UsiMatchHandler::executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                                 QPoint& outFrom, QPoint& outTo, int byoyomiMilliSec,
                                                 const QString& btime, const QString& wtime,
                                                 int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                                 bool useByoyomi)
{
    processEngineResponse(positionStr, positionPonderStr, byoyomiMilliSec, btime, wtime,
                          addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    if (m_protocolHandler->isResignMove()) return;

    int fileFrom, rankFrom, fileTo, rankTo;
    m_protocolHandler->parseMoveCoordinates(fileFrom, rankFrom, fileTo, rankTo);

    outFrom = QPoint(fileFrom, rankFrom);
    outTo = QPoint(fileTo, rankTo);
}

void UsiMatchHandler::processEngineResponse(QString& positionStr, QString& positionPonderStr,
                                            int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                            int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // 処理フロー:
    // 1. ポンダー予測手がない場合 → 通常のコマンド送信
    // 2. ポンダーヒット（bestmove == predictedMove）→ ponderhit送信して応答待ち
    // 3. ポンダーミス → stop送信後に通常のコマンド送信

    const QString& predictedMove = m_protocolHandler->predictedMove();

    if (predictedMove.isEmpty() || !m_protocolHandler->isPonderEnabled()) {
        sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime,
                               positionPonderStr, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
        return;
    }

    const QString& bestMove = m_protocolHandler->bestMove();

    if (bestMove == predictedMove) {
        // ポンダーヒット
        cloneCurrentBoardData();

        // ポンダーヒット時のbaseSfenと最終指し手を更新（現在の盤面から）
        QString baseSfen = computeBaseSfenFromBoard();
        if (!baseSfen.isEmpty()) {
            m_presenter->setBaseSfen(baseSfen);
        }
        // positionStrの最後のトークンがヒットした指し手
        if (positionStr.contains(QStringLiteral(" moves "))) {
            const QStringList tokens = positionStr.split(QLatin1Char(' '));
            if (!tokens.isEmpty()) {
                m_lastUsiMove = tokens.last();
            }
        }

        m_protocolHandler->sendPonderHit();

        if (byoyomiMilliSec == 0) {
            m_protocolHandler->keepWaitingForBestMove();
        } else {
            waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);
        }

        if (m_protocolHandler->isResignMove()) return;

        appendBestMoveAndStartPondering(positionStr, positionPonderStr);
    } else {
        // ポンダーミス
        m_protocolHandler->sendStop();

        if (byoyomiMilliSec == 0) {
            m_protocolHandler->keepWaitingForBestMove();
        } else {
            waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);
        }

        if (m_protocolHandler->isResignMove()) return;

        sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime,
                               positionPonderStr, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
    }
}

void UsiMatchHandler::sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr,
                                             const QString& btime, const QString& wtime,
                                             QString& positionPonderStr, int addEachMoveMilliSec1,
                                             int addEachMoveMilliSec2, bool useByoyomi)
{
    // 処理フロー:
    // 1. 局面SFENを保存（読み筋表示用）
    // 2. position + goコマンド送信
    // 3. bestmove応答待ち
    // 4. bestmoveを反映してポンダー開始

    // 思考開始時の局面SFENを保存（読み筋表示用）
    QString baseSfen = computeBaseSfenFromBoard();
    if (!baseSfen.isEmpty()) {
        m_presenter->setBaseSfen(baseSfen);
    }

    // 思考開始局面に至った最後の指し手を更新（読み筋表示ウィンドウのハイライト用）
    // positionStrの最後のトークンが最終指し手（"position startpos moves 7g7f 8c8d" → "8c8d"）
    if (positionStr.contains(QStringLiteral(" moves "))) {
        const QStringList tokens = positionStr.split(QLatin1Char(' '));
        if (!tokens.isEmpty()) {
            m_lastUsiMove = tokens.last();
        }
    }

    m_protocolHandler->sendPosition(positionStr);
    cloneCurrentBoardData();
    m_protocolHandler->sendGo(byoyomiMilliSec, btime, wtime,
                              addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);

    if (m_protocolHandler->isResignMove()) return;

    appendBestMoveAndStartPondering(positionStr, positionPonderStr);
}

void UsiMatchHandler::waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
                                                           const QString& wtime, bool useByoyomi)
{
    const bool p1turn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    const int mainMs = p1turn ? btime.toInt() : wtime.toInt();
    int capMs = useByoyomi ? (mainMs + byoyomiMilliSec) : mainMs;
    if (capMs >= 200) capMs -= 100;

    static constexpr int kBestmoveGraceMs = 250;

    if (!m_protocolHandler->waitForBestMoveWithGrace(capMs, kBestmoveGraceMs)) {
        if (m_protocolHandler->isTimeoutDeclared()) {
            return;
        }
        if (m_hooks.onBestmoveTimeout) {
            m_hooks.onBestmoveTimeout();
        }
    }
}

void UsiMatchHandler::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr)
{
    const QString& predictedMove = m_protocolHandler->predictedMove();

    if (!predictedMove.isEmpty() && m_protocolHandler->isPonderEnabled()) {
        applyMovesToBoardFromBestMoveAndPonder();

        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + predictedMove;

        // ポンダー先読み開始時の局面SFENと最終指し手を更新（読み筋表示用）
        updateBaseSfenForPonder();
        m_lastUsiMove = predictedMove;

        m_protocolHandler->sendPosition(positionPonderStr);
        m_protocolHandler->sendGoPonder();
    }
}

void UsiMatchHandler::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr)
{
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_protocolHandler->bestMove();
    startPonderingAfterBestMove(positionStr, positionPonderStr);
}
