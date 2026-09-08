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
#include "shogiclock.h"
#include <limits>

namespace {

/// QList<Piece> → QList<QChar> 変換（ShogiBoard::boardData() → 内部クローン用）
QList<QChar> pieceVectorToCharVector(const QList<Piece>& pieces)
{
    QList<QChar> chars;
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

        Piece movingPiece = board->pieceCharacter(*fileFrom, *rankFrom);
        Piece capturedPiece = board->pieceCharacter(*fileTo, *rankTo);

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

void UsiMatchHandler::setClonedBoardData(const QList<QChar>& boardData)
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
                                                       const UsiTimingParams& timing,
                                                       QStringList& positionStrList)
{
    // 人間の指し手をUSI形式に変換
    QString bestMove = convertHumanMoveToUsiFormat(outFrom, outTo, m_gameController->promote());

    ensureMovesKeyword(positionStr);
    positionStr += " " + bestMove;
    positionStrList.append(positionStr);

    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo, timing);
}

void UsiMatchHandler::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                                    QString& positionPonderStr,
                                                                    QPoint& outFrom, QPoint& outTo,
                                                                    const UsiTimingParams& timing)
{
    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo, timing);
}

void UsiMatchHandler::executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                                 QPoint& outFrom, QPoint& outTo,
                                                 const UsiTimingParams& timing)
{
    outFrom = outTo = QPoint(-1, -1);
    m_acceptBestMove = false;
    if (m_clock) m_clock->updateClock();
    if (m_clock && m_clock->isGameOver()) return;
    if (!processEngineResponse(positionStr, positionPonderStr, timing)) return;
    if (m_protocolHandler->specialMove() != SpecialMove::None) return;

    int fileFrom, rankFrom, fileTo, rankTo;
    m_protocolHandler->parseMoveCoordinates(fileFrom, rankFrom, fileTo, rankTo);
    outFrom = QPoint(fileFrom, rankFrom);
    outTo = QPoint(fileTo, rankTo);
}

void UsiMatchHandler::onBestMoveReceived()
{
    if (m_acceptBestMove && m_clock) m_clock->finishTurn();
    m_acceptBestMove = false;
}

bool UsiMatchHandler::processEngineResponse(QString& positionStr, QString& positionPonderStr,
                                            const UsiTimingParams& timing)
{
    if (m_protocolHandler->currentPhase() != UsiProtocolHandler::SearchPhase::Ponder) {
        return sendCommandsAndProcess(positionStr, positionPonderStr, timing);
    }

    // 直近の実着手を含む局面と、先読み開始時の局面を比較する。
    // 前回のエンジン着手(bestMove)との比較ではヒットを判定できない。
    const bool hit = m_protocolHandler->isPonderEnabled()
        && !m_protocolHandler->predictedMove().isEmpty()
        && positionStr.simplified() == positionPonderStr.simplified();
    if (!hit) {
        m_protocolHandler->sendStop();
        // 旧探索の応答を必ず回収してから新しい探索を開始する。
        // 回収したresign/winも予測局面の結果なので採用しない。
        if (!m_protocolHandler->waitForBestMove(2000)) {
            if (m_hooks.onBestmoveTimeout) m_hooks.onBestmoveTimeout();
            return false;
        }
        return sendCommandsAndProcess(positionStr, positionPonderStr, timing);
    }

    cloneCurrentBoardData();
    const QString baseSfen = computeBaseSfenFromBoard();
    if (!baseSfen.isEmpty()) m_presenter->setBaseSfen(baseSfen);
    m_lastUsiMove = positionStr.section(QLatin1Char(' '), -1);
    m_acceptBestMove = true;
    m_protocolHandler->sendPonderHit();
    if (!waitAndCheckForBestMoveRemainingTime(timing)) return false;
    if (m_protocolHandler->specialMove() == SpecialMove::None) {
        appendBestMoveAndStartPondering(positionStr, positionPonderStr, timing);
    }
    return true;
}

UsiTimingParams UsiMatchHandler::timingForSearch(const UsiTimingParams& timing, bool pondering) const
{
    UsiTimingParams result = timing;
    const int side = m_gameController->currentPlayer() == ShogiGameController::Player1 ? 1 : 2;
    qint64 main = side == 1 ? timing.btime.toLongLong() : timing.wtime.toLongLong();
    qint64 byo = timing.useByoyomi ? qMax(0, timing.byoyomiMilliSec) : 0;
    if (m_clock) {
        result.btime = QString::number(m_clock->remainingMainTimeMs(1));
        result.wtime = QString::number(m_clock->remainingMainTimeMs(2));
        main = m_clock->remainingMainTimeMs(side);
        if (pondering) {
            // 現在の着手確定による加算は、Strategyで適用される前。
            if (!timing.useByoyomi) {
                main += side == 1 ? timing.addEachMoveMilliSec1 : timing.addEachMoveMilliSec2;
            }
        } else {
            // stop待ち、局面準備、最後のtick以降も含めた残予算。
            byo = qMax<qint64>(0, m_clock->remainingTurnTimeMs(side) - main);
        }
    }
    // GUIの期限より先に返せるよう、通信・探索停止の余裕を確保する。
    // 短い設定でも最低1msの思考予算を残す。
    const qint64 total = qMax<qint64>(0, main) + byo;
    const qint64 reserve = qMin<qint64>(250, total / 2);
    const qint64 byoReserve = qMin(byo, reserve);
    byo -= byoReserve;
    main = qMax<qint64>(0, main - (reserve - byoReserve));
    if (side == 1) result.btime = QString::number(main);
    else result.wtime = QString::number(main);
    result.byoyomiMilliSec = static_cast<int>(qMin<qint64>(byo, std::numeric_limits<int>::max()));
    return result;
}

bool UsiMatchHandler::sendCommandsAndProcess(QString& positionStr, QString& positionPonderStr,
                                             const UsiTimingParams& timing)
{
    if (m_clock && m_clock->isGameOver()) return false;
    const QString baseSfen = computeBaseSfenFromBoard();
    if (!baseSfen.isEmpty()) m_presenter->setBaseSfen(baseSfen);
    if (positionStr.contains(QStringLiteral(" moves "))) {
        m_lastUsiMove = positionStr.section(QLatin1Char(' '), -1);
    }
    m_protocolHandler->sendPosition(positionStr);
    cloneCurrentBoardData();
    const UsiTimingParams adjusted = timingForSearch(timing, false);
    m_acceptBestMove = true;
    m_protocolHandler->sendGo(adjusted.byoyomiMilliSec, adjusted.btime, adjusted.wtime,
                              adjusted.addEachMoveMilliSec1, adjusted.addEachMoveMilliSec2,
                              adjusted.useByoyomi);
    if (!waitAndCheckForBestMoveRemainingTime(timing)) return false;
    if (m_protocolHandler->specialMove() == SpecialMove::None) {
        appendBestMoveAndStartPondering(positionStr, positionPonderStr, timing);
    }
    return true;
}

bool UsiMatchHandler::waitAndCheckForBestMoveRemainingTime(const UsiTimingParams& timing)
{
    const int side = m_gameController->currentPlayer() == ShogiGameController::Player1 ? 1 : 2;
    qint64 budget = side == 1 ? timing.btime.toLongLong() : timing.wtime.toLongLong();
    if (timing.useByoyomi) budget += timing.byoyomiMilliSec;
    if (m_clock) {
        budget = m_clock->enforcesTimeout() ? m_clock->remainingTurnTimeMs(side)
                                           : UsiProtocolHandler::kKeepWaitingHardTimeoutMs;
    }
    const int cap = static_cast<int>(qBound(qint64(1), budget, qint64(std::numeric_limits<int>::max())));
    // GUI時計と同じ残予算。独立した+150msの猶予は設けない。
    const bool received = m_protocolHandler->waitForBestMove(cap);
    if (received) onBestMoveReceived(); // ファサードなしの利用でも精算する
    m_acceptBestMove = false;
    if (m_clock && !received) m_clock->updateClock();
    if (m_clock && m_clock->isGameOver()) return false;
    if (!received) {
        if (!m_protocolHandler->isTimeoutDeclared() && m_hooks.onBestmoveTimeout) {
            m_hooks.onBestmoveTimeout();
        }
        return false;
    }
    return true;
}

void UsiMatchHandler::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr,
                                                const UsiTimingParams& timing)
{
    const QString predictedMove = m_protocolHandler->predictedMove();
    if (!predictedMove.isEmpty() && m_protocolHandler->isPonderEnabled()) {
        applyMovesToBoardFromBestMoveAndPonder();
        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + predictedMove;
        updateBaseSfenForPonder();
        m_lastUsiMove = predictedMove;
        const UsiTimingParams nextTiming = timingForSearch(timing, true);
        m_protocolHandler->sendPosition(positionPonderStr);
        m_protocolHandler->sendGoPonder(nextTiming);
    }
}

void UsiMatchHandler::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr,
                                                     const UsiTimingParams& timing)
{
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_protocolHandler->bestMove();
    startPonderingAfterBestMove(positionStr, positionPonderStr, timing);
}
