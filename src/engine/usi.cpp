/**
 * @file usi.cpp
 * @brief USIプロトコル通信ファサードクラスの実装
 *
 * 単一責任原則（SRP）に基づき、以下の3つのクラスに処理を委譲する:
 * - EngineProcessManager: QProcessの管理
 * - UsiProtocolHandler: USIプロトコルの送受信
 * - ThinkingInfoPresenter: GUIへの表示更新
 */

#include "usi.h"
#include "shogiboard.h"
#include "shogiengineinfoparser.h"
#include "shogiinforecord.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QTimer>

namespace {
void ensureMovesKeyword(QString& s)
{
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}
} // anonymous namespace

// === コンストラクタ・デストラクタ ===

Usi::Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
         ShogiGameController* gameController, PlayMode& playMode, QObject* parent)
    : QObject(parent)
    , m_processManager(std::make_unique<EngineProcessManager>(this))
    , m_protocolHandler(std::make_unique<UsiProtocolHandler>(this))
    , m_presenter(std::make_unique<ThinkingInfoPresenter>(this))
    , m_commLogModel(model)
    , m_thinkingModel(modelThinking)
    , m_gameController(gameController)
    , m_playMode(playMode)
{
    setupConnections();
    
    // Presenterにゲームコントローラのみを設定（モデルへの直接依存を排除）
    m_presenter->setGameController(gameController);
    
    m_protocolHandler->setProcessManager(m_processManager.get());
    m_protocolHandler->setPresenter(m_presenter.get());
    m_protocolHandler->setGameController(gameController);
}

Usi::~Usi()
{
    // デストラクタ時はモデルクリアをスキップ（モデルが既に破棄されている可能性があるため）
    m_processManager->stopProcess();
    // m_presenter->requestClearThinkingInfo() は呼ばない
}

void Usi::setupConnections()
{
    // ProcessManagerからのシグナル
    connect(m_processManager.get(), &EngineProcessManager::processError,
            this, &Usi::onProcessError);
    connect(m_processManager.get(), &EngineProcessManager::commandSent,
            this, &Usi::onCommandSent);
    connect(m_processManager.get(), &EngineProcessManager::dataReceived,
            this, &Usi::onDataReceived);
    connect(m_processManager.get(), &EngineProcessManager::stderrReceived,
            this, &Usi::onStderrReceived);

    // ProtocolHandlerからのシグナル
    connect(m_protocolHandler.get(), &UsiProtocolHandler::usiOkReceived,
            this, &Usi::usiOkReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::readyOkReceived,
            this, &Usi::readyOkReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::bestMoveReceived,
            this, &Usi::bestMoveReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::bestMoveResignReceived,
            this, &Usi::bestMoveResignReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::bestMoveWinReceived,
            this, &Usi::bestMoveWinReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::stopOrPonderhitSent,
            this, &Usi::stopOrPonderhitCommandSent);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::errorOccurred,
            this, &Usi::errorOccurred);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::infoLineReceived,
            this, &Usi::infoLineReceived);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::checkmateSolved,
            this, &Usi::checkmateSolved);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::checkmateNoMate,
            this, &Usi::checkmateNoMate);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::checkmateNotImplemented,
            this, &Usi::checkmateNotImplemented);
    connect(m_protocolHandler.get(), &UsiProtocolHandler::checkmateUnknown,
            this, &Usi::checkmateUnknown);

    // Presenterからのシグナル（View層への間接的な更新）
    connect(m_presenter.get(), &ThinkingInfoPresenter::thinkingInfoUpdated,
            this, &Usi::onThinkingInfoUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::searchedMoveUpdated,
            this, &Usi::onSearchedMoveUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::searchDepthUpdated,
            this, &Usi::onSearchDepthUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::nodeCountUpdated,
            this, &Usi::onNodeCountUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::npsUpdated,
            this, &Usi::onNpsUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::hashUsageUpdated,
            this, &Usi::onHashUsageUpdated);
    connect(m_presenter.get(), &ThinkingInfoPresenter::commLogAppended,
            this, &Usi::onCommLogAppended);
    connect(m_presenter.get(), &ThinkingInfoPresenter::clearThinkingInfoRequested,
            this, &Usi::onClearThinkingInfoRequested);
}

// === スロット実装 ===

void Usi::onProcessError(QProcess::ProcessError error, const QString& message)
{
    Q_UNUSED(error)
    cleanupEngineProcessAndThread();
    emit errorOccurred(message);
    cancelCurrentOperation();
}

void Usi::onCommandSent(const QString& command)
{
    m_presenter->logSentCommand(m_processManager->logPrefix(), command);
}

void Usi::onDataReceived(const QString& line)
{
    m_presenter->logReceivedData(m_processManager->logPrefix(), line);
}

void Usi::onStderrReceived(const QString& line)
{
    m_presenter->logStderrData(m_processManager->logPrefix(), line);
}

// === モデル更新スロット実装（シグナル経由で更新）===

void Usi::onSearchedMoveUpdated(const QString& move)
{
    if (m_commLogModel) {
        m_commLogModel->setSearchedMove(move);
    }
}

void Usi::onSearchDepthUpdated(const QString& depth)
{
    if (m_commLogModel) {
        m_commLogModel->setSearchDepth(depth);
    }
}

void Usi::onNodeCountUpdated(const QString& nodes)
{
    if (m_commLogModel) {
        m_commLogModel->setNodeCount(nodes);
    }
}

void Usi::onNpsUpdated(const QString& nps)
{
    if (m_commLogModel) {
        m_commLogModel->setNodesPerSecond(nps);
    }
}

void Usi::onHashUsageUpdated(const QString& hashUsage)
{
    if (m_commLogModel) {
        m_commLogModel->setHashUsage(hashUsage);
    }
}

void Usi::onCommLogAppended(const QString& log)
{
    if (m_commLogModel) {
        m_commLogModel->appendUsiCommLog(log);
    }
}

void Usi::onClearThinkingInfoRequested()
{
    if (m_thinkingModel) {
        m_thinkingModel->clearAllItems();
    }
}

void Usi::onThinkingInfoUpdated(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen)
{
    qDebug().noquote() << "[Usi::onThinkingInfoUpdated] m_lastUsiMove=" << m_lastUsiMove
                       << " baseSfen=" << baseSfen.left(50);
    
    // 思考タブへ追記（USI PVとbaseSfenも保存）
    if (m_thinkingModel) {
        ShogiInfoRecord* record = new ShogiInfoRecord(time, depth, nodes, score, pvKanjiStr);
        record->setUsiPv(usiPv);
        record->setBaseSfen(baseSfen);
        record->setLastUsiMove(m_lastUsiMove);
        qDebug().noquote() << "[Usi::onThinkingInfoUpdated] record->lastUsiMove() after set=" << record->lastUsiMove();
        m_thinkingModel->prependItem(record);
    }
    
    // 外部への通知
    emit thinkingInfoUpdated(time, depth, nodes, score, pvKanjiStr, usiPv, baseSfen);
}

// === 公開インターフェース実装 ===

QString Usi::scoreStr() const
{
    return m_presenter->scoreStr();
}

bool Usi::isResignMove() const
{
    return m_protocolHandler->isResignMove();
}

bool Usi::isWinMove() const
{
    return m_protocolHandler->isWinMove();
}

int Usi::lastScoreCp() const
{
    return m_presenter->lastScoreCp();
}

QString Usi::pvKanjiStr() const
{
    return m_pvKanjiStr;
}

void Usi::setPvKanjiStr(const QString& newPvKanjiStr)
{
    m_pvKanjiStr = newPvKanjiStr;
}

QString Usi::convertFirstPlayerPieceNumberToSymbol(int rankFrom) const
{
    return m_protocolHandler->convertFirstPlayerPieceSymbol(rankFrom);
}

QString Usi::convertSecondPlayerPieceNumberToSymbol(int rankFrom) const
{
    return m_protocolHandler->convertSecondPlayerPieceSymbol(rankFrom);
}

void Usi::parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo)
{
    m_protocolHandler->parseMoveCoordinates(fileFrom, rankFrom, fileTo, rankTo);
}

QChar Usi::rankToAlphabet(int rank) const
{
    return UsiProtocolHandler::rankToAlphabet(rank);
}

qint64 Usi::lastBestmoveElapsedMs() const
{
    return m_protocolHandler->lastBestmoveElapsedMs();
}

void Usi::setPreviousFileTo(int newPreviousFileTo)
{
    m_previousFileTo = newPreviousFileTo;
    m_presenter->setPreviousMove(m_previousFileTo, m_previousRankTo);
}

void Usi::setPreviousRankTo(int newPreviousRankTo)
{
    m_previousRankTo = newPreviousRankTo;
    m_presenter->setPreviousMove(m_previousFileTo, m_previousRankTo);
}

void Usi::setLastUsiMove(const QString& move)
{
    qDebug().noquote() << "[Usi::setLastUsiMove] move=" << move;
    m_lastUsiMove = move;
}

void Usi::setLogIdentity(const QString& engineTag, const QString& sideTag,
                         const QString& engineName)
{
    m_processManager->setLogIdentity(engineTag, sideTag, engineName);
}

void Usi::setSquelchResignLogging(bool on)
{
    Q_UNUSED(on)
    // 必要に応じて実装
}

void Usi::resetResignNotified()
{
    m_protocolHandler->resetResignNotified();
}

void Usi::resetWinNotified()
{
    m_protocolHandler->resetWinNotified();
}

void Usi::markHardTimeout()
{
    m_protocolHandler->markHardTimeout();
}

void Usi::clearHardTimeout()
{
    m_protocolHandler->clearHardTimeout();
}

bool Usi::isIgnoring() const
{
    return m_processManager->shutdownState() !=
           EngineProcessManager::ShutdownState::Running;
}

QString Usi::currentEnginePath() const
{
    return m_processManager->currentEnginePath();
}

void Usi::setThinkingModel(ShogiEngineThinkingModel* m)
{
    m_thinkingModel = m;
    // Presenterはシグナル経由で更新するため、モデル参照は不要
}

void Usi::setLogModel(UsiCommLogModel* m)
{
    m_commLogModel = m;
    // Presenterはシグナル経由で更新するため、モデル参照は不要
}

#ifdef QT_DEBUG
ShogiEngineThinkingModel* Usi::debugThinkingModel() const
{
    return m_thinkingModel;
}

UsiCommLogModel* Usi::debugLogModel() const
{
    return m_commLogModel;
}
#endif

void Usi::cancelCurrentOperation()
{
    m_protocolHandler->cancelCurrentOperation();
}
// usi.cpp Part 2 - エンジン起動・コマンド送信

// === エンジン起動・初期化 ===

void Usi::initializeAndStartEngineCommunication(QString& engineFile, QString& enginename)
{
    if (engineFile.isEmpty()) {
        cleanupEngineProcessAndThread();
        emit errorOccurred(tr("Engine file path is empty."));
        cancelCurrentOperation();
        return;
    }

    changeDirectoryToEnginePath(engineFile);
    startAndInitializeEngine(engineFile, enginename);
}

void Usi::changeDirectoryToEnginePath(const QString& engineFile)
{
    const QFileInfo fileInfo(engineFile);

    if (!QDir::setCurrent(fileInfo.path())) {
        cleanupEngineProcessAndThread();
        emit errorOccurred(tr("Failed to change directory to %1").arg(fileInfo.path()));
        cancelCurrentOperation();
    }
}

void Usi::startAndInitializeEngine(const QString& engineFile, const QString& enginename)
{
    // プロセス起動
    if (!m_processManager->startProcess(engineFile)) {
        cleanupEngineProcessAndThread();
        return;
    }

    // オプション読み込み
    m_protocolHandler->loadEngineOptions(enginename);

    // 初期化シーケンス実行
    if (!m_protocolHandler->initializeEngine(enginename)) {
        cleanupEngineProcessAndThread();
        return;
    }
}

void Usi::cleanupEngineProcessAndThread()
{
    // エンジンプロセスが実行中の場合は quit コマンドを送信してから停止
    if (m_processManager->isRunning()) {
        m_protocolHandler->sendQuit();
    }
    m_processManager->stopProcess();
    m_presenter->requestClearThinkingInfo();
}

// === コマンド送信 ===

void Usi::sendGameOverCommand(const QString& result)
{
    m_protocolHandler->sendGameOver(result);
}

void Usi::sendQuitCommand()
{
    m_protocolHandler->sendQuit();
}

void Usi::sendStopCommand()
{
    m_protocolHandler->sendStop();
}

void Usi::sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                        int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGo(byoyomiMilliSec, btime, wtime,
                              addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

void Usi::sendGoDepthCommand(int depth)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoDepth(depth);
}

void Usi::sendGoNodesCommand(qint64 nodes)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoNodes(nodes);
}

void Usi::sendGoMovetimeCommand(int timeMs)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoMovetime(timeMs);
}

void Usi::sendGoSearchmovesCommand(const QStringList& moves, bool infinite)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoSearchmoves(moves, infinite);
}

void Usi::sendGoSearchmovesDepthCommand(const QStringList& moves, int depth)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoSearchmovesDepth(moves, depth);
}

void Usi::sendGoSearchmovesMovetimeCommand(const QStringList& moves, int timeMs)
{
    cloneCurrentBoardData();
    m_protocolHandler->sendGoSearchmovesMovetime(moves, timeMs);
}

void Usi::sendRaw(const QString& command) const
{
    m_protocolHandler->sendRaw(command);
}

bool Usi::isEngineRunning() const
{
    return m_processManager && m_processManager->isRunning();
}

void Usi::prepareBoardDataForAnalysis()
{
    qDebug().noquote() << "[Usi::prepareBoardDataForAnalysis] called";
    if (m_gameController && m_gameController->board()) {
        m_clonedBoardData = m_gameController->board()->boardData();
        qDebug().noquote() << "[Usi::prepareBoardDataForAnalysis] m_clonedBoardData.size()=" << m_clonedBoardData.size();
        m_presenter->setClonedBoardData(m_clonedBoardData);
    } else {
        qWarning().noquote() << "[Usi::prepareBoardDataForAnalysis] ERROR: gameController or board is null!";
    }
}

void Usi::setClonedBoardData(const QVector<QChar>& boardData)
{
    m_clonedBoardData = boardData;
    if (m_presenter) {
        m_presenter->setClonedBoardData(m_clonedBoardData);
    }
}

void Usi::setBaseSfen(const QString& sfen)
{
    qDebug().noquote() << "[Usi::setBaseSfen] sfen=" << sfen.left(50);
    if (m_presenter) {
        m_presenter->setBaseSfen(sfen);
    }
}

void Usi::flushThinkingInfoBuffer()
{
    qDebug().noquote() << "[Usi::flushThinkingInfoBuffer] called";
    if (m_presenter) {
        m_presenter->flushInfoBuffer();
    }
}

void Usi::requestClearThinkingInfo()
{
    qDebug().noquote() << "[Usi::requestClearThinkingInfo] called";
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }
}

void Usi::sendGameOverLoseAndQuitCommands()
{
    if (!m_processManager->isRunning()) return;

    if (!m_gameoverSent) {
        m_protocolHandler->sendGameOver("lose");
        m_gameoverSent = true;
    }

    m_protocolHandler->sendQuit();
}

void Usi::sendGameOverWinAndQuitCommands()
{
    if (!m_processManager->isRunning()) return;

    if (!m_gameoverSent) {
        m_protocolHandler->sendGameOver("win");
        m_gameoverSent = true;
    }

    m_protocolHandler->sendQuit();
}

void Usi::waitForStopOrPonderhitCommand()
{
    m_protocolHandler->waitForStopOrPonderhit();
}

// === 詰将棋関連 ===

void Usi::sendPositionAndGoMate(const QString& sfen, int timeMs, bool infinite)
{
    if (!m_processManager->isRunning()) {
        emit errorOccurred(tr("USI engine is not running."));
        cancelCurrentOperation();
        return;
    }

    m_protocolHandler->sendPosition(QStringLiteral("position sfen %1").arg(sfen.trimmed()));
    m_protocolHandler->sendGoMate(timeMs, infinite);
}

void Usi::sendStopForMate()
{
    if (!m_processManager->isRunning()) return;
    m_protocolHandler->sendStop();
}

void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    cloneCurrentBoardData();
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoMate(mateLimitMilliSec);
}

// === 盤面処理 ===

void Usi::cloneCurrentBoardData()
{
    qDebug().noquote() << "[Usi::cloneCurrentBoardData] m_gameController=" << m_gameController;
    if (!m_gameController) {
        qWarning().noquote() << "[Usi::cloneCurrentBoardData] ERROR: m_gameController is nullptr!";
        return;
    }
    qDebug().noquote() << "[Usi::cloneCurrentBoardData] m_gameController->board()=" << m_gameController->board();
    if (!m_gameController->board()) {
        qWarning().noquote() << "[Usi::cloneCurrentBoardData] ERROR: m_gameController->board() is nullptr!";
        return;
    }
    m_clonedBoardData = m_gameController->board()->boardData();
    qDebug().noquote() << "[Usi::cloneCurrentBoardData] m_clonedBoardData.size()=" << m_clonedBoardData.size();
    m_presenter->setClonedBoardData(m_clonedBoardData);
}

void Usi::applyMovesToBoardFromBestMoveAndPonder()
{
    ShogiEngineInfoParser info;
    info.parseAndApplyMoveToClonedBoard(m_protocolHandler->bestMove(), m_clonedBoardData);
    info.parseAndApplyMoveToClonedBoard(m_protocolHandler->predictedMove(), m_clonedBoardData);
    m_presenter->setClonedBoardData(m_clonedBoardData);
}

QString Usi::convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote)
{
    return m_protocolHandler->convertHumanMoveToUsi(outFrom, outTo, promote);
}
// usi.cpp Part 3 - 対局通信処理

// === 対局通信処理 ===

void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
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

void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
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

void Usi::executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
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

void Usi::processEngineResponse(QString& positionStr, QString& positionPonderStr,
                                int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
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

void Usi::sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr,
                                 const QString& btime, const QString& wtime,
                                 QString& positionPonderStr, int addEachMoveMilliSec1,
                                 int addEachMoveMilliSec2, bool useByoyomi)
{
    // 思考開始時の局面SFENを保存（読み筋表示用）
    if (m_gameController && m_gameController->board()) {
        ShogiBoard* board = m_gameController->board();
        QString turn = (m_gameController->currentPlayer() == ShogiGameController::Player1) 
                       ? QStringLiteral("b") : QStringLiteral("w");
        QString baseSfen = board->convertBoardToSfen() + QStringLiteral(" ") + turn +
                          QStringLiteral(" ") + board->convertStandToSfen() + QStringLiteral(" 1");
        m_presenter->setBaseSfen(baseSfen);
    }
    
    m_protocolHandler->sendPosition(positionStr);
    cloneCurrentBoardData();
    m_protocolHandler->sendGo(byoyomiMilliSec, btime, wtime,
                              addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);

    if (m_protocolHandler->isResignMove()) return;

    appendBestMoveAndStartPondering(positionStr, positionPonderStr);
}

void Usi::waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
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
        emit errorOccurred(tr("Timeout waiting for bestmove."));
        cancelCurrentOperation();
    }
}

void Usi::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr)
{
    const QString& predictedMove = m_protocolHandler->predictedMove();
    
    if (!predictedMove.isEmpty() && m_protocolHandler->isPonderEnabled()) {
        applyMovesToBoardFromBestMoveAndPonder();

        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + predictedMove;

        m_protocolHandler->sendPosition(positionPonderStr);
        m_protocolHandler->sendGoPonder();
    }
}

void Usi::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr)
{
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_protocolHandler->bestMove();
    startPonderingAfterBestMove(positionStr, positionPonderStr);
}

// === 棋譜解析 ===

void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec)
{
    // 思考開始時の局面SFENを保存（読み筋表示用）
    if (m_gameController && m_gameController->board()) {
        ShogiBoard* board = m_gameController->board();
        QString turn = (m_gameController->currentPlayer() == ShogiGameController::Player1) 
                       ? QStringLiteral("b") : QStringLiteral("w");
        QString baseSfen = board->convertBoardToSfen() + QStringLiteral(" ") + turn +
                          QStringLiteral(" ") + board->convertStandToSfen() + QStringLiteral(" 1");
        m_presenter->setBaseSfen(baseSfen);
    }
    
    cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);
    
    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");

    if (byoyomiMilliSec <= 0) {
        m_protocolHandler->keepWaitingForBestMove();
    } else {
        // タイムアウト後にstop送信
        QTimer::singleShot(byoyomiMilliSec, this, [this]() {
            m_protocolHandler->sendStop();
        });
        
        static constexpr int kPostStopGraceMs = 4000;
        const int waitBudget = qMax(byoyomiMilliSec + kPostStopGraceMs, 2500);
        m_protocolHandler->waitForBestMove(waitBudget);
    }
}
