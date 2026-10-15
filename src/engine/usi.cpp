/// @file usi.cpp
/// @brief USIプロトコル通信ファサードクラスの実装（構築・配線・アクセサ・コマンド送信）

#include "usi.h"
#include "kifupresentation.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "usimatchhandler.h"
#include "matchsearchsnapshot.h"

#include <QTimer>
#include <utility>
#include "shogiinforecord.h"

// ============================================================
// 構築・破棄
// ============================================================

Usi::Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking, QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
    , m_processManager(m_session->process())
    , m_protocolHandler(m_session->handler())
    , m_presenter(std::make_unique<ThinkingInfoPresenter>())
    , m_matchHandler(std::make_unique<UsiMatchHandler>(m_protocolHandler))
    , m_commLogModel(model)
    , m_thinkingModel(modelThinking)
{
    connect(m_session, &UsiEngineSession::ready, this, &Usi::onEngineInitialized);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &Usi::onProtocolError);
    setupConnections();

    // UsiMatchHandlerのフック設定
    m_matchHandler->setHooks({
        /*.onBestmoveTimeout =*/ [this]() {
            cancelCurrentOperation();
            emit errorOccurred(tr("Timeout waiting for bestmove."));
        },
        /*.onMoveReady =*/ [this](const UsiMove& move, const QString& position, const QString& ponder) {
            emit matchMoveReady(move, position, ponder);
        },
        [this](const UsiTimingParams& timing, Turn sideToMove, bool pondering) {
            return matchSearchSnapshot(m_matchClock, sideToMove, timing, pondering);
        },
        [this]() {
            if (!m_matchClock) return true;
            m_matchClock->finishTurn();
            return !m_matchClock->isGameOver();
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
    m_session->quit();
    // m_presenter->requestClearThinkingInfo() は呼ばない
}

void Usi::setupConnections()
{
    connect(m_protocolHandler, &UsiProtocolHandler::searchCandidateChanged,
            this, &Usi::searchCandidateChanged);
    // 処理フロー:
    // 1. ProcessManager → Usi（プロセスイベント転送）
    // 2. ProtocolHandler → Usi（USIプロトコルイベント転送）
    // 3. Presenter → Usi（GUI表示更新の中継）

    // ProcessManagerからのシグナル
    connect(m_processManager, &EngineProcessManager::commandSent,
            this, &Usi::onCommandSent);
    connect(m_processManager, &EngineProcessManager::dataReceived,
            this, &Usi::onDataReceived);
    connect(m_processManager, &EngineProcessManager::stderrReceived,
            this, &Usi::onStderrReceived);

    connect(m_protocolHandler, &UsiProtocolHandler::positionChanged,
            m_presenter.get(), &ThinkingInfoPresenter::setBaseSfen);
    connect(m_protocolHandler, &UsiProtocolHandler::searchStarted,
            m_presenter.get(), &ThinkingInfoPresenter::requestClearThinkingInfo);
    connect(m_protocolHandler, &UsiProtocolHandler::infoReceived,
            m_presenter.get(), &ThinkingInfoPresenter::onInfoReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::checkmateSolved,
            m_presenter.get(), &ThinkingInfoPresenter::presentCheckmate);

    // ProtocolHandlerからのシグナル
    connect(m_protocolHandler, &UsiProtocolHandler::usiOkReceived,
            this, &Usi::usiOkReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::readyOkReceived,
            this, &Usi::readyOkReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveReceived,
            this, &Usi::onMatchBestMoveReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveReceived,
            this, &Usi::bestMoveReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveResignReceived,
            this, &Usi::bestMoveResignReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveWinReceived,
            this, &Usi::bestMoveWinReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::infoReceived,
            this, &Usi::infoReceived);
    connect(m_protocolHandler, &UsiProtocolHandler::checkmateSolved,
            this, &Usi::checkmateSolved);
    connect(m_protocolHandler, &UsiProtocolHandler::checkmateNoMate,
            this, &Usi::checkmateNoMate);
    connect(m_protocolHandler, &UsiProtocolHandler::checkmateNotImplemented,
            this, &Usi::checkmateNotImplemented);
    connect(m_protocolHandler, &UsiProtocolHandler::checkmateUnknown,
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
// プロセスイベントスロット実装
// ============================================================

void Usi::onCommandSent(const QString& command)
{
    m_presenter->logSentCommand(m_processManager->logPrefix(), command);
}

void Usi::onDataReceived(const QString& line)
{
    m_presenter->logReceivedData(m_processManager->logPrefix(), line);
}

void Usi::onMatchBestMoveReceived()
{
    if (m_commLogModel) {
        QString prediction;
        const QString afterBest = m_protocolHandler->predictedMove().isEmpty()
            ? QString() : SfenReplay::positionAfter(m_presenter->baseSfen(), m_protocolHandler->bestMove());
        if (!afterBest.isEmpty()) {
            prediction = KifuPresentation::move(afterBest, m_protocolHandler->predictedMove(), KifuPresentation::options());
        }
        m_commLogModel->setPredictiveMove(prediction);
    }
    m_matchHandler->onBestMoveReceived();
}

void Usi::onStderrReceived(const QString& line)
{
    m_presenter->logStderrData(m_processManager->logPrefix(), line);
}

// ============================================================
// 公開インターフェース実装
// ============================================================

bool Usi::isResignMove() const
{
    return m_protocolHandler->isResignMove();
}

bool Usi::isWinMove() const
{
    return m_protocolHandler->isWinMove();
}

QString Usi::lastScoreMate() const
{
    return m_presenter->lastScoreMate();
}

int Usi::lastScoreCp() const
{
    return m_presenter->lastScoreCp();
}

qint64 Usi::lastBestmoveElapsedMs() const
{
    return m_protocolHandler->lastBestmoveElapsedMs();
}

void Usi::setPreviousMove(BoardSquare to)
{
    qCDebug(lcEngine) << "setPreviousMove: file=" << to.file << "rank=" << to.rank;
    m_previousMove = to;
    m_presenter->setPreviousMove(to.file, to.rank);
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
    m_protocolHandler->setSquelchResignLogging(on);
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
    m_matchHandler->cancelAsync();
    m_pendingActions.clear();
    m_protocolHandler->markHardTimeout();
}

void Usi::clearHardTimeout()
{
    m_protocolHandler->clearHardTimeout();
}

QString Usi::currentEnginePath() const
{
    return m_processManager->currentEnginePath();
}

void Usi::cancelCurrentOperation()
{
    ++m_asyncGeneration;
    m_pendingActions.clear();
    m_matchHandler->cancelAsync();
    m_session->cancelCurrentOperation();
}

// ============================================================
// エンジン起動・初期化
// ============================================================

void Usi::cleanupEngineProcessAndThread(bool clearThinking)
{
    cancelCurrentOperation();
    m_session->quit();
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
    cancelCurrentOperation();
    m_session->quit();
}

void Usi::sendStopCommand()
{
    if (isInitializing()) { m_pendingActions.clear(); return; }
    m_protocolHandler->sendStop();
    // 検討モデルはクリアしない（再開時に必要なため）
    // モデルのクリアはエンジン破棄時に自動的に行われる
}

void Usi::requestImmediateMove()
{
    // 準備中に sendStopCommand を呼ぶと保留中の position・go まで破棄され、エンジンが指さずに対局が止まる。
    // 保留中の処理の後ろに並べ、探索を始めた直後に止めて指させる
    if (deferUntilReady([this]() { sendStopCommand(); })) return;
    sendStopCommand();
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

void Usi::sendSetOptionCommand(const QString& name, const QString& value)
{
    if (deferUntilReady([this, name, value]() { sendSetOptionCommand(name, value); })) return;
    m_protocolHandler->sendSetOption(name, value);
}

void Usi::sendConsoleCommand(const QString& command)
{
    if (deferUntilReady([this, command]() { sendConsoleCommand(command); })) return;
    m_protocolHandler->sendConsoleCommand(command);
}

bool Usi::isEngineRunning() const
{
    return m_processManager && m_processManager->isRunning();
}

void Usi::setBaseSfen(const QString& sfen)
{
    qCDebug(lcEngine) << "setBaseSfen:" << sfen.left(50);
    if (m_presenter) {
        m_protocolHandler->setBaseSfen(sfen);
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

    m_session->quit();
}

void Usi::sendGameOverWinAndQuitCommands()
{
    if (!m_processManager->isRunning()) return;

    if (!m_gameoverSent) {
        m_protocolHandler->sendGameOver(GameOverResult::Win);
        m_gameoverSent = true;
    }

    m_session->quit();
}

// ============================================================
// USI初期化と対局要求の非同期制御
// ============================================================

bool Usi::startEngine(const QString& engineFile, const QString& engineName)
{
    if (engineFile.isEmpty()) {
        cleanupEngineProcessAndThread();
        emit errorOccurred(tr("Engine file path is empty."));
        return false;
    }
    cancelCurrentOperation();
    QString error;
    if (!m_session->startAsync(engineFile, engineName, &error)) {
        emit errorOccurred(error);
        return false;
    }
    return true;
}

void Usi::onEngineInitialized()
{
    auto actions = std::exchange(m_pendingActions, {});
    const QPointer<Usi> guard(this);
    const auto generation = m_asyncGeneration;
    for (auto& action : actions) {
        action();
        if (!guard || generation != m_asyncGeneration) return;
    }
    emit engineInitialized();
}

void Usi::onProtocolError(const QString& message)
{
    cleanupEngineProcessAndThread();
    emit errorOccurred(message);
}

bool Usi::deferUntilReady(std::function<void()> action) const
{
    if (!isInitializing()) return false;
    m_pendingActions.push_back(std::move(action));
    return true;
}

void Usi::requestMatchMove(const QString& position, const QString& ponder, const UsiTimingParams& timing)
{
    if (deferUntilReady([this, position, ponder, timing]() { requestMatchMove(position, ponder, timing); })) return;
    m_considerationModel = nullptr;
    m_matchHandler->requestMove(position, ponder, timing);
}

// ============================================================
// Usi クラスの通信処理実装（詰将棋・対局・解析）
// ============================================================

// ============================================================
// 詰将棋関連
// ============================================================

void Usi::startMateSearch(const QString& positionStr, int timeLimitMs)
{
    if (deferUntilReady([this, positionStr, timeLimitMs]() { startMateSearch(positionStr, timeLimitMs); })) return;
    const QString baseSfen = SfenReplay::sfenFromPositionCommand(positionStr);
    if (!baseSfen.isEmpty()) m_presenter->setBaseSfen(baseSfen);
    m_presenter->requestClearThinkingInfo();
    sendMateSearch(positionStr, timeLimitMs);
}

void Usi::sendMateSearch(const QString& positionStr, int timeLimitMs)
{
    if (deferUntilReady([this, positionStr, timeLimitMs]() { sendMateSearch(positionStr, timeLimitMs); })) return;
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoMate(timeLimitMs);
}

// ============================================================
// 棋譜解析
// ============================================================

void Usi::resetAnalysisStopTimer()
{
    if (!m_analysisStopTimer) {
        return;
    }
    m_analysisStopTimer->stop();
    m_analysisStopTimer->deleteLater();
    m_analysisStopTimer = nullptr;
}

void Usi::prepareAnalysisSession(const QString& positionStr, int multiPV)
{
    resetAnalysisStopTimer();

    // 思考開始時の局面SFENを保存（読み筋表示用）
    const QString baseSfen = SfenReplay::sfenFromPositionCommand(positionStr);
    if (!baseSfen.isEmpty()) m_presenter->setBaseSfen(baseSfen);
    m_considerationMaxMultiPV = qBound(1, multiPV, 10);
    m_protocolHandler->sendPosition(positionStr);

    // MultiPV を設定（常に送信して、前回の設定をリセット）
    m_protocolHandler->sendSetOption(QStringLiteral("MultiPV"), QString::number(multiPV));

    m_presenter->requestClearThinkingInfo();
    m_protocolHandler->sendGoInfinite();
}

void Usi::sendPositionAndGoInfinite(const QString& positionStr)
{
    if (deferUntilReady([this, positionStr]() { sendPositionAndGoInfinite(positionStr); })) return;
    m_protocolHandler->sendPosition(positionStr);
    m_protocolHandler->sendGoInfinite();
}

void Usi::sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV)
{
    if (deferUntilReady([this, positionStr, byoyomiMilliSec, multiPV]() {
        sendAnalysisCommands(positionStr, byoyomiMilliSec, multiPV);
    })) return;
    qCDebug(lcEngine) << "sendAnalysisCommands:"
                      << "positionStr=" << positionStr
                      << "byoyomiMilliSec=" << byoyomiMilliSec
                      << "multiPV=" << multiPV;

    prepareAnalysisSession(positionStr, multiPV);

    // タイムアウト後にstop送信（byoyomiMilliSec > 0 の場合のみ）
    // メンバータイマーを使用して、次回の sendAnalysisCommands でキャンセル可能にする
    if (byoyomiMilliSec > 0) {
        m_analysisStopTimer = new QTimer(this);
        m_analysisStopTimer->setSingleShot(true);
        connect(m_analysisStopTimer, &QTimer::timeout, this, &Usi::onConsiderationStopTimeout);
        m_analysisStopTimer->start(byoyomiMilliSec);
    }

    // 応答はシグナルで受信する。
    // bestmove はシグナル経由で通知される
}

// ============================================================
// Usi クラスのモデル更新スロット実装
// ============================================================

namespace {
constexpr int kMaxThinkingRows = 500;

[[nodiscard]] bool isSameThinkingPayload(const ShogiInfoRecord* record,
                                         const ThinkingInfoRow& row,
                                         const QString& lastUsiMove)
{
    if (!record) {
        return false;
    }

    // 頻繁に変化するフィールドを先に比較して早期リターン
    return record->scoreCp() == row.scoreCp
        && record->multipv() == row.multipv
        && record->depth() == row.depth
        && record->pv() == row.pvKanji
        && record->time() == row.time
        && record->nodes() == row.nodes
        && record->score() == row.score
        && record->usiPv() == row.usiPv
        && record->enginePv() == row.enginePv
        && record->baseSfen() == row.baseSfen
        && record->lastUsiMove() == lastUsiMove;
}

ShogiInfoRecord* makeRecord(const ThinkingInfoRow& row, const QString& lastUsiMove)
{
    auto* record = new ShogiInfoRecord(row.time, row.depth, row.nodes, row.score, row.pvKanji);
    record->setUsiPv(row.usiPv);
    record->setEnginePv(row.enginePv);
    record->setBaseSfen(row.baseSfen);
    record->setLastUsiMove(lastUsiMove);
    record->setMultipv(row.multipv);
    record->setScoreCp(row.scoreCp);
    return record;
}
} // anonymous namespace

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

void Usi::onConsiderationStopTimeout()
{
    if (m_processManager && m_processManager->isRunning()) {
        qCDebug(lcEngine) << "検討停止タイマー発火";
        m_protocolHandler->sendStop();
    }
    resetAnalysisStopTimer();
}

void Usi::onClearThinkingInfoRequested()
{
    qCDebug(lcEngine) << "思考情報クリア要求";
    if (m_thinkingModel) {
        qCDebug(lcEngine) << "思考モデルクリア実行";
        m_thinkingModel->clearAllItems();
    }
}

void Usi::onThinkingInfoUpdated(const ThinkingInfoRow& row)
{
    // 処理フロー:
    // 1. ShogiInfoRecordを生成して思考タブへ追記（先頭に追加）
    // 2. 検討タブへ追記（MultiPVモードで行を更新/挿入）
    // 3. 外部へシグナルで通知
    const QString lastUsiMove = m_matchHandler->lastUsiMove();
    qCDebug(lcEngine) << "思考情報更新: m_lastUsiMove=" << lastUsiMove
                      << "baseSfen=" << row.baseSfen.left(50)
                      << "multipv=" << row.multipv << "scoreCp=" << row.scoreCp;

    // 思考タブへ追記（通常モード: 先頭に追加）
    // 読み筋（PV）が空の行は表示しない（詰み探索の中間結果など）
    if (m_thinkingModel && !row.pvKanji.isEmpty()) {
        const ShogiInfoRecord* topRecord =
            (m_thinkingModel->rowCount() > 0) ? m_thinkingModel->recordAt(0) : nullptr;
        if (!isSameThinkingPayload(topRecord, row, lastUsiMove)) {
            m_thinkingModel->prependItem(makeRecord(row, lastUsiMove));
            m_thinkingModel->trimToMaxRows(kMaxThinkingRows);
        }
    }

    // 検討タブへ追記（MultiPVモード: multipv値に基づいて行を更新/挿入）
    // PV のない info（停止時の time/nodes だけの行など）は multipv 1 扱いになり、
    // 最善手の行を空の読み筋で上書きしてしまうため反映しない
    if (m_considerationModel && !row.pvKanji.isEmpty()) {
        const auto existingRow = m_considerationModel->findRowByMultipv(row.multipv);
        const ShogiInfoRecord* existingMultipvRecord = existingRow.has_value()
            ? m_considerationModel->recordAt(*existingRow) : nullptr;

        if (!isSameThinkingPayload(existingMultipvRecord, row, lastUsiMove)) {
            m_considerationModel->updateByMultipv(makeRecord(row, lastUsiMove), m_considerationMaxMultiPV);
        }
    }

    // 外部への通知
    emit thinkingInfoUpdated(row);
}
