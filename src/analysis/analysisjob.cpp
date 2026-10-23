/// @file analysisjob.cpp
/// @brief 棋譜解析1回（エンジンの起動から全局面の解析まで）の実装

#include "analysisjob.h"

#include "logcategories.h"
#include "usi.h"
#include "usienginesession.h"

#include <utility>

namespace {

/// 手順の各局面（[0] が開始局面。読めない局面は nullopt）。局面だけを送り、そこに至った手を読み筋の「同」の判定に使う
QList<std::optional<UsiPosition>> positionsOf(const RecordLine& line)
{
    QList<std::optional<UsiPosition>> positions;
    if (line.isEmpty()) return positions;
    positions.reserve(line.moveCount() + 1);
    for (int ply = 0; ply <= line.moveCount(); ++ply) {
        const std::optional<UsiMove> lastMove = ply > 0 ? std::optional<UsiMove>(line.usiMove(ply)) : std::nullopt;
        positions << UsiPosition::fromSfen(line.sfenAt(ply), lastMove);
    }
    return positions;
}

} // namespace

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

AnalysisJob::AnalysisJob(AnalysisRequest request, const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_request(std::move(request))
    , m_positions(positionsOf(m_request.line))
    , m_engine(deps.engine)
{
    Q_ASSERT(m_engine);
    m_engine->setLogIdentity(QStringLiteral("E1"), {}, m_request.engineName);
    connect(m_engine, &Usi::engineInitialized, this, &AnalysisJob::onInitialized);
    connect(m_engine, &Usi::errorOccurred, this, &AnalysisJob::onError);
    connect(m_engine->protocol(), &UsiProtocolHandler::bestMoveReceived, this, &AnalysisJob::onBestMove);
    connect(m_engine->protocol(), &UsiProtocolHandler::infoReceived, this, &AnalysisJob::onInfo);
}

Usi* AnalysisJob::engine() const
{
    return m_engine;
}

AnalysisJob::~AnalysisJob()
{
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
        disconnect(m_engine->protocol(), nullptr, this, nullptr);
    }
    if (isRunning() && m_engine) m_engine->quit();
}

bool AnalysisJob::start()
{
    if (m_state != State::Ready || !m_engine || m_positions.isEmpty()) return false;
    // 範囲を手順に収める（終わりの -1 は末尾まで）
    const int last = static_cast<int>(m_positions.size()) - 1;
    AnalysisRequest::Options& options = m_request.options;
    if (options.endPly < 0 || options.endPly > last) options.endPly = last;
    options.startPly = qBound(0, options.startPly, options.endPly);

    m_state = State::Initializing;
    // 初期化が終われば onInitialized、起動できなければ戻った後に onError が届く
    m_engine->startEngine(m_request.enginePath, m_request.engineName);
    return true;
}

void AnalysisJob::onInitialized()
{
    if (m_state != State::Initializing) return;
    m_state = State::Analyzing;
    analyzePly(m_request.options.startPly);
}

void AnalysisJob::analyzePly(int ply)
{
    // 読めない局面（壊れた SFEN）はエンジンに送らずに飛ばす（前の局面の結果をこの手数の結果にしない）
    while (ply <= m_request.options.endPly && !m_positions.at(ply)) {
        qCWarning(lcAnalysis) << "analyzePly: skip unreadable position at ply" << ply;
        ++ply;
    }
    if (ply > m_request.options.endPly) {
        finish(true);
        return;
    }
    m_ply = ply;
    const UsiPosition& position = *m_positions.at(ply);
    qCDebug(lcAnalysis).noquote() << "analyzePly: ply=" << ply << "position=" << position.command();
    m_collector.begin(ply, position);
    emit plyStarted(ply);
    if (m_state != State::Analyzing || !m_engine) return;
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::infinite();
    request.stopAfterMs = m_request.options.movetimeMs;
    m_engine->search(request);
}

void AnalysisJob::stop()
{
    if (!isRunning()) return;
    if (m_state == State::Analyzing && m_engine) m_engine->stop();
    finish(false);
}

void AnalysisJob::onInfo(const UsiInfo& info)
{
    if (m_state == State::Analyzing) m_collector.addInfo(info);
}

void AnalysisJob::onBestMove(const UsiBestMove& result)
{
    if (m_state != State::Analyzing || !m_engine) return;
    // 定跡（info なしで bestmove が来た）でも次へ進む。結果の表に定跡と書くかは結果の表の側が決める
    emit plyAnalyzed(m_collector.finish(result));
    if (m_state != State::Analyzing) return;
    if (m_ply >= m_request.options.endPly) {
        finish(true);
        return;
    }
    analyzePly(m_ply + 1);
}

void AnalysisJob::finish(bool completed)
{
    if (!isRunning()) return;
    m_state = State::Finished;
    m_ply = -1;
    if (m_engine) m_engine->quit();
    emit finished(completed);
}

void AnalysisJob::onError(const QString& message)
{
    if (!isRunning()) return;
    m_error = message;
    stop();
    emit errorOccurred(message);
}
