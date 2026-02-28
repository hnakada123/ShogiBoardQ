/// @file usi.cpp
/// @brief USIプロトコル通信ファサードクラスの実装

#include "usi.h"
#include "usimatchhandler.h"
#include "shogiinforecord.h"

#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QTimer>

namespace {
constexpr int kMaxThinkingRows = 500;

[[nodiscard]] bool isSameThinkingPayload(const ShogiInfoRecord* record,
                                         const QString& time,
                                         const QString& depth,
                                         const QString& nodes,
                                         const QString& score,
                                         const QString& pvKanjiStr,
                                         const QString& usiPv,
                                         const QString& baseSfen,
                                         const QString& lastUsiMove,
                                         int multipv,
                                         int scoreCp)
{
    if (!record) {
        return false;
    }

    // 頻繁に変化するフィールドを先に比較して早期リターン
    return record->scoreCp() == scoreCp
        && record->multipv() == multipv
        && record->depth() == depth
        && record->pv() == pvKanjiStr
        && record->time() == time
        && record->nodes() == nodes
        && record->score() == score
        && record->usiPv() == usiPv
        && record->baseSfen() == baseSfen
        && record->lastUsiMove() == lastUsiMove;
}
} // anonymous namespace

// ============================================================
// 構築・破棄
// ============================================================

Usi::Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
         ShogiGameController* gameController, PlayMode& playMode, QObject* parent)
    : QObject(parent)
    , m_processManager(std::make_unique<EngineProcessManager>())
    , m_protocolHandler(std::make_unique<UsiProtocolHandler>())
    , m_presenter(std::make_unique<ThinkingInfoPresenter>())
    , m_matchHandler(std::make_unique<UsiMatchHandler>(m_protocolHandler.get(),
                                                       m_presenter.get(),
                                                       gameController))
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

    // UsiMatchHandlerのフック設定
    m_matchHandler->setHooks({
        /*.onBestmoveTimeout =*/ [this]() {
            emit errorOccurred(tr("Timeout waiting for bestmove."));
            cancelCurrentOperation();
        }
    });
}

Usi::~Usi()
{
    // 停止タイマーをキャンセル
    if (m_analysisStopTimer) {
        m_analysisStopTimer->stop();
        // m_analysisStopTimer は this を parent に持つため、Qt親子モデルにより自動解放される
    }
    // デストラクタ時はモデルクリアをスキップ（モデルが既に破棄されている可能性があるため）
    m_processManager->stopProcess();
    // m_presenter->requestClearThinkingInfo() は呼ばない
}

void Usi::setupConnections()
{
    // 処理フロー:
    // 1. ProcessManager → Usi（プロセスイベント転送）
    // 2. ProtocolHandler → Usi（USIプロトコルイベント転送）
    // 3. Presenter → Usi（GUI表示更新の中継）

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

// ============================================================
// スロット実装
// ============================================================

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

// ============================================================
// モデル更新スロット実装（Presenter → CommLogModel/ThinkingModel転送）
// ============================================================

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

void Usi::onAnalysisStopTimeout()
{
    qCDebug(lcEngine) << "解析停止タイマー発火";
    m_protocolHandler->sendStop();
    m_analysisStopTimer = nullptr;
}

void Usi::onConsiderationStopTimeout()
{
    if (m_processManager && m_processManager->isRunning()) {
        qCDebug(lcEngine) << "検討停止タイマー発火";
        m_protocolHandler->sendStop();
    }
    m_analysisStopTimer = nullptr;
}

void Usi::onClearThinkingInfoRequested()
{
    qCDebug(lcEngine) << "思考情報クリア要求";
    if (m_thinkingModel) {
        qCDebug(lcEngine) << "思考モデルクリア実行";
        m_thinkingModel->clearAllItems();
    }
}

void Usi::onThinkingInfoUpdated(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen, int multipv, int scoreCp)
{
    // 処理フロー:
    // 1. ShogiInfoRecordを生成して思考タブへ追記（先頭に追加）
    // 2. 検討タブへ追記（MultiPVモードで行を更新/挿入）
    // 3. 外部へシグナルで通知
    const QString lastUsiMove = m_matchHandler->lastUsiMove();
    qCDebug(lcEngine) << "思考情報更新: m_lastUsiMove=" << lastUsiMove
                      << "baseSfen=" << baseSfen.left(50)
                      << "multipv=" << multipv << "scoreCp=" << scoreCp;

    // 思考タブへ追記（通常モード: 先頭に追加）
    // 読み筋（PV）が空の行は表示しない（詰み探索の中間結果など）
    if (m_thinkingModel && !pvKanjiStr.isEmpty()) {
        const ShogiInfoRecord* topRecord =
            (m_thinkingModel->rowCount() > 0) ? m_thinkingModel->recordAt(0) : nullptr;
        if (!isSameThinkingPayload(topRecord, time, depth, nodes, score, pvKanjiStr, usiPv,
                                   baseSfen, lastUsiMove, multipv, scoreCp)) {
            ShogiInfoRecord* record = new ShogiInfoRecord(time, depth, nodes, score, pvKanjiStr);
            record->setUsiPv(usiPv);
            record->setBaseSfen(baseSfen);
            record->setLastUsiMove(lastUsiMove);
            record->setMultipv(multipv);
            record->setScoreCp(scoreCp);
            qCDebug(lcEngine) << "record->lastUsiMove()=" << record->lastUsiMove();
            m_thinkingModel->prependItem(record);
            m_thinkingModel->trimToMaxRows(kMaxThinkingRows);
        }
    }

    // 検討タブへ追記（MultiPVモード: multipv値に基づいて行を更新/挿入）
    if (m_considerationModel) {
        const int existingRow = m_considerationModel->findRowByMultipv(multipv);
        const ShogiInfoRecord* existingMultipvRecord = (existingRow >= 0)
            ? m_considerationModel->recordAt(existingRow) : nullptr;

        if (!isSameThinkingPayload(existingMultipvRecord, time, depth, nodes, score, pvKanjiStr, usiPv,
                                   baseSfen, lastUsiMove, multipv, scoreCp)) {
            ShogiInfoRecord* record = new ShogiInfoRecord(time, depth, nodes, score, pvKanjiStr);
            record->setUsiPv(usiPv);
            record->setBaseSfen(baseSfen);
            record->setLastUsiMove(lastUsiMove);
            record->setMultipv(multipv);
            record->setScoreCp(scoreCp);
            m_considerationModel->updateByMultipv(record, m_considerationMaxMultiPV);
        }
    }

    // 外部への通知
    emit thinkingInfoUpdated(time, depth, nodes, score, pvKanjiStr, usiPv, baseSfen, multipv, scoreCp);
}

// ============================================================
// 公開インターフェース実装
// ============================================================

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
    qCDebug(lcEngine) << "setPreviousFileTo:" << newPreviousFileTo
                      << "m_previousRankTo=" << m_previousRankTo;
    m_previousFileTo = newPreviousFileTo;
    m_presenter->setPreviousMove(m_previousFileTo, m_previousRankTo);
}

void Usi::setPreviousRankTo(int newPreviousRankTo)
{
    qCDebug(lcEngine) << "setPreviousRankTo: m_previousFileTo=" << m_previousFileTo
                      << "newPreviousRankTo=" << newPreviousRankTo;
    m_previousRankTo = newPreviousRankTo;
    m_presenter->setPreviousMove(m_previousFileTo, m_previousRankTo);
}

void Usi::setLastUsiMove(const QString& move)
{
    m_matchHandler->setLastUsiMove(move);
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

void Usi::cancelCurrentOperation()
{
    m_protocolHandler->cancelCurrentOperation();
}

// ============================================================
// エンジン起動・初期化
// ============================================================

void Usi::initializeAndStartEngineCommunication(QString& engineFile, QString& enginename)
{
    if (engineFile.isEmpty()) {
        cleanupEngineProcessAndThread();
        emit errorOccurred(tr("Engine file path is empty."));
        cancelCurrentOperation();
        return;
    }

    if (!changeDirectoryToEnginePath(engineFile)) {
        return;
    }
    startAndInitializeEngine(engineFile, enginename);
}

bool Usi::changeDirectoryToEnginePath(const QString& engineFile)
{
    const QFileInfo fileInfo(engineFile);

    if (!QDir::setCurrent(fileInfo.path())) {
        cleanupEngineProcessAndThread();
        emit errorOccurred(tr("Failed to change directory to %1").arg(fileInfo.path()));
        cancelCurrentOperation();
        return false;
    }

    return true;
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

void Usi::cleanupEngineProcessAndThread(bool clearThinking)
{
    // エンジンプロセスが実行中の場合は quit コマンドを送信してから停止
    if (m_processManager->isRunning()) {
        m_protocolHandler->sendQuit();
    }
    m_processManager->stopProcess();
    if (clearThinking) {
        m_presenter->requestClearThinkingInfo();
    }
}

// ============================================================
// コマンド送信
// ============================================================

void Usi::sendGameOverCommand(GameOverResult result)
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
    // 検討モデルはクリアしない（再開時に必要なため）
    // モデルのクリアはエンジン破棄時に自動的に行われる
}

void Usi::setConsiderationModel(ShogiEngineThinkingModel* model, int maxMultiPV)
{
    m_considerationModel = model;
    m_considerationMaxMultiPV = qBound(1, maxMultiPV, 10);
    qCDebug(lcEngine) << "setConsiderationModel: model=" << model << "maxMultiPV=" << m_considerationMaxMultiPV;

    // モデルをクリア
    if (m_considerationModel) {
        m_considerationModel->clearAllItems();
    }
}

void Usi::updateConsiderationMultiPV(int multiPV)
{
    const int newMultiPV = qBound(1, multiPV, 10);
    qCDebug(lcEngine) << "updateConsiderationMultiPV: old=" << m_considerationMaxMultiPV
                      << "new=" << newMultiPV;

    // 変更がない場合は何もしない
    if (m_considerationMaxMultiPV == newMultiPV) {
        return;
    }

    m_considerationMaxMultiPV = newMultiPV;

    // エンジンにMultiPV設定を送信
    if (m_protocolHandler) {
        m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(newMultiPV));
    }

    // モデルをクリアして新しいMultiPV設定で再表示
    if (m_considerationModel) {
        m_considerationModel->clearAllItems();
    }
}

void Usi::sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                        int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendGo(byoyomiMilliSec, btime, wtime,
                              addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
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
    m_matchHandler->prepareBoardDataForAnalysis();
}

void Usi::setClonedBoardData(const QVector<QChar>& boardData)
{
    m_matchHandler->setClonedBoardData(boardData);
}

void Usi::setBaseSfen(const QString& sfen)
{
    qCDebug(lcEngine) << "setBaseSfen:" << sfen.left(50);
    if (m_presenter) {
        m_presenter->setBaseSfen(sfen);
    }
}

void Usi::flushThinkingInfoBuffer()
{
    qCDebug(lcEngine) << "flushThinkingInfoBuffer";
    if (m_presenter) {
        m_presenter->flushInfoBuffer();
    }
}

void Usi::requestClearThinkingInfo()
{
    qCDebug(lcEngine) << "requestClearThinkingInfo";
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }
}

void Usi::sendGameOverLoseAndQuitCommands()
{
    if (!m_processManager->isRunning()) return;

    if (!m_gameoverSent) {
        m_protocolHandler->sendGameOver(GameOverResult::Lose);
        m_gameoverSent = true;
    }

    m_protocolHandler->sendQuit();
}

void Usi::sendGameOverWinAndQuitCommands()
{
    if (!m_processManager->isRunning()) return;

    if (!m_gameoverSent) {
        m_protocolHandler->sendGameOver(GameOverResult::Win);
        m_gameoverSent = true;
    }

    m_protocolHandler->sendQuit();
}

// ============================================================
// 詰将棋関連
// ============================================================

void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    m_matchHandler->cloneCurrentBoardData();
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoMate(mateLimitMilliSec);
}

// ============================================================
// 対局通信処理（UsiMatchHandlerへ委譲）
// ============================================================

void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                           QPoint& outFrom, QPoint& outTo,
                                           int byoyomiMilliSec, const QString& btime,
                                           const QString& wtime, QStringList& positionStrList,
                                           int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                           bool useByoyomi)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleHumanVsEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                                                     byoyomiMilliSec, btime, wtime, positionStrList,
                                                     addEachMoveMilliSec1, addEachMoveMilliSec2,
                                                     useByoyomi);
}

void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                        QString& positionPonderStr,
                                                        QPoint& outFrom, QPoint& outTo,
                                                        int byoyomiMilliSec, const QString& btime,
                                                        const QString& wtime,
                                                        int addEachMoveMilliSec1,
                                                        int addEachMoveMilliSec2, bool useByoyomi)
{
    // 対局時は検討タブ用モデルをクリア
    m_considerationModel = nullptr;

    m_matchHandler->handleEngineVsHumanOrEngineMatchCommunication(positionStr, positionPonderStr,
                                                                  outFrom, outTo, byoyomiMilliSec,
                                                                  btime, wtime, addEachMoveMilliSec1,
                                                                  addEachMoveMilliSec2, useByoyomi);
}

// ============================================================
// 棋譜解析
// ============================================================

void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    // 処理フロー:
    // 1. 既存の停止タイマーをキャンセル
    // 2. 局面SFENを保存（読み筋表示用）
    // 3. 盤面クローン + position送信
    // 4. MultiPV設定送信
    // 5. go infinite送信
    // 6. タイムアウト設定（秒読みありの場合）またはbestmove待機

    // 既存の停止タイマーをキャンセル
    if (m_analysisStopTimer) {
        m_analysisStopTimer->stop();
        m_analysisStopTimer->deleteLater();
        m_analysisStopTimer = nullptr;
    }

    // 思考開始時の局面SFENを保存（読み筋表示用）
    {
        QString baseSfen = m_matchHandler->computeBaseSfenFromBoard();
        if (!baseSfen.isEmpty()) {
            m_presenter->setBaseSfen(baseSfen);
        }
    }

    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");

    if (byoyomiMilliSec <= 0) {
        (void)m_protocolHandler->keepWaitingForBestMove();
    } else {
        // タイムアウト後にstop送信（メンバータイマーを使用）
        m_analysisStopTimer = new QTimer(this);
        m_analysisStopTimer->setSingleShot(true);
        connect(m_analysisStopTimer, &QTimer::timeout, this, &Usi::onAnalysisStopTimeout);
        m_analysisStopTimer->start(byoyomiMilliSec);

        static constexpr int kPostStopGraceMs = 4000;
        const int waitBudget = qMax(byoyomiMilliSec + kPostStopGraceMs, 2500);
        (void)m_protocolHandler->waitForBestMove(waitBudget);
    }
}

void Usi::sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    qCDebug(lcEngine) << "sendAnalysisCommands:"
                      << "positionStr=" << positionStr
                      << "byoyomiMilliSec=" << byoyomiMilliSec
                      << "multiPV=" << multiPV;

    // 既存の停止タイマーをキャンセル（前回の検討のタイマーが残っている可能性がある）
    if (m_analysisStopTimer) {
        m_analysisStopTimer->stop();
        m_analysisStopTimer->deleteLater();
        m_analysisStopTimer = nullptr;
    }

    // 思考開始時の局面SFENを保存（読み筋表示用）
    {
        QString baseSfen = m_matchHandler->computeBaseSfenFromBoard();
        if (!baseSfen.isEmpty()) {
            m_presenter->setBaseSfen(baseSfen);
        }
    }

    m_matchHandler->cloneCurrentBoardData();
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendRaw(QStringLiteral("setoption name MultiPV value %1").arg(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendRaw("go infinite");

    // タイムアウト後にstop送信（byoyomiMilliSec > 0 の場合のみ）
    // メンバータイマーを使用して、次回の sendAnalysisCommands でキャンセル可能にする
    if (byoyomiMilliSec > 0) {
        m_analysisStopTimer = new QTimer(this);
        m_analysisStopTimer->setSingleShot(true);
        connect(m_analysisStopTimer, &QTimer::timeout, this, &Usi::onConsiderationStopTimeout);
        m_analysisStopTimer->start(byoyomiMilliSec);
    }

    // waitForBestMove は呼ばない（非ブロッキング）
    // bestmove はシグナル経由で通知される
}
