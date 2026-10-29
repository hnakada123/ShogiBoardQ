/// @file engineanalysisrunner.cpp
/// @brief 局面の列を1つのエンジンで順に一定時間ずつ解析する実行器の実装

#include "engineanalysisrunner.h"

#include "logcategories.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"

// ============================================================
// 1局面の結果
// ============================================================

void PlyAnalysisCollector::begin(int ply, const UsiPosition& position)
{
    m_ply = ply;
    m_position = position;
    m_anyInfo = false;
    m_bestPv.clear();
    m_bestDepth = -1;
}

void PlyAnalysisCollector::addInfo(const UsiInfo& info)
{
    m_anyInfo = true;
    // エンジン設定で MultiPV を2以上にしていても、結果は最善の読み筋で確定する
    if (info.multipv <= 1 && info.hasPv()) {
        m_bestPv = info.pv;
        m_bestDepth = info.depth;
    }
}

PlyAnalysis PlyAnalysisCollector::finish(const UsiBestMove& bestMove) const
{
    PlyAnalysis result;
    result.ply = m_ply;
    result.book = !m_anyInfo;
    if (result.book) return result;
    result.score = bestMove.score;
    result.depth = m_bestDepth;

    const std::optional<UsiMove>& last = m_position.lastMove();
    result.pv = PvText::play(m_position.sfen(), m_bestPv, last ? last->to : BoardSquare{});
    return result;
}

// ============================================================
// 解析の進行
// ============================================================

EngineAnalysisRunner::EngineAnalysisRunner(UsiEngineSession* session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    Q_ASSERT(m_session);
    connect(m_session, &UsiEngineSession::ready, this, &EngineAnalysisRunner::onSessionReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &EngineAnalysisRunner::onSessionError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &EngineAnalysisRunner::onNoResponse);
    connect(m_session->handler(), &UsiProtocolHandler::infoReceived, this, &EngineAnalysisRunner::onInfo);
    connect(m_session->handler(), &UsiProtocolHandler::bestMoveReceived, this, &EngineAnalysisRunner::onBestMove);
}

EngineAnalysisRunner::~EngineAnalysisRunner()
{
    if (!m_session) return;
    // 借りたエンジンは持ち主のもとに残るので、知らせを外してから終える
    disconnect(m_session, nullptr, this, nullptr);
    disconnect(m_session->handler(), nullptr, this, nullptr);
    if (isRunning()) m_session->quit();
}

void EngineAnalysisRunner::start(const Request& request)
{
    m_latest.clear();
    m_stopRequested = false;
    m_timedOut = false;
    m_index = 0;
    m_request = request;
    m_elapsed.invalidate();
    m_state = State::Initializing;
    // 起動の失敗は、この呼び出しから戻った後に errorOccurred で届く
    m_session->startAsync(request.enginePath, request.engineName);
}

void EngineAnalysisRunner::onSessionReady()
{
    if (m_state != State::Initializing) return;
    m_state = State::Analyzing;
    if (m_request.multiPv > 0)
        m_session->handler()->sendSetOption(QStringLiteral("MultiPV"), QString::number(m_request.multiPv));
    analyzeFrom(0);
}

void EngineAnalysisRunner::analyzeFrom(int index)
{
    // 読めない局面（壊れた SFEN）はエンジンに送らずに飛ばす（前の局面の結果をこの手数の結果にしない）
    const int count = static_cast<int>(m_request.positions.size());
    while (index < count && !m_request.positions.at(index)) {
        qCWarning(lcAnalysis) << "analyzeFrom: skip unreadable position at ply" << m_request.firstPly + index;
        ++index;
    }
    if (index >= count) {
        finish(false);
        return;
    }
    m_index = index;
    m_latest.clear();
    const UsiPosition& position = *m_request.positions.at(index);
    const int ply = m_request.firstPly + index;
    qCDebug(lcAnalysis).noquote() << "analyzeFrom: ply=" << ply << "position=" << position.command();
    m_collector.begin(ply, position);
    emit positionStarted(ply);
    // 知らせを受けた側が止めたら探索しない
    if (m_state != State::Analyzing || !m_session) return;
    UsiSearchRequest search;
    search.position = position;
    search.go = UsiGo::infinite();
    search.stopAfterMs = qMax(100, m_request.thinkMs);
    search.replyTimeoutMs = m_request.replyTimeoutMs;
    m_elapsed.start();
    if (!m_session->search(search)) {
        fail(QStringLiteral("No position to analyse"));
        return;
    }
    // 始めた知らせを受けた側が止めていたら、始めてすぐ止める（bestmove を待って結果を返す）
    if (m_stopRequested) m_session->stop();
}

void EngineAnalysisRunner::stop()
{
    if (!isRunning() || m_stopRequested) return;
    m_stopRequested = true;
    // 初期化が終わる前（探索を送る前）なら、そのまま終える
    if (m_state == State::Initializing) {
        finish(true);
        return;
    }
    if (m_session) m_session->stop();
}

void EngineAnalysisRunner::cancel()
{
    if (!isRunning()) return;
    m_stopRequested = true;
    finish(true);
}

void EngineAnalysisRunner::onInfo(const UsiInfo& info)
{
    if (m_state != State::Analyzing) return;
    m_collector.addInfo(info);
    if (!info.hasPv()) return;
    m_latest.insert(info.multipv, info);
    emit lineUpdated(info);
}

void EngineAnalysisRunner::onBestMove(const UsiBestMove& result)
{
    if (m_state != State::Analyzing) return;
    // 定跡（info なしで bestmove が来た）でも次へ進む。結果の表に定跡と書くかは結果を受ける側が決める
    emit positionFinished(m_collector.finish(result), result);
    if (m_state != State::Analyzing) return;
    if (m_stopRequested) {
        finish(true);
        return;
    }
    analyzeFrom(m_index + 1);
}

void EngineAnalysisRunner::onNoResponse()
{
    if (m_state != State::Analyzing) return;
    m_timedOut = true;
    fail(QStringLiteral("The engine did not answer bestmove after stop"));
}

void EngineAnalysisRunner::onSessionError(const QString& message)
{
    fail(message);
}

void EngineAnalysisRunner::fail(const QString& message)
{
    if (!isRunning()) return;
    m_state = State::Finished;
    if (m_session) m_session->quit();
    emit errorOccurred(message);
}

void EngineAnalysisRunner::finish(bool stopped)
{
    if (!isRunning()) return;
    m_state = State::Finished;
    if (m_session) m_session->quit();
    emit finished(stopped);
}
