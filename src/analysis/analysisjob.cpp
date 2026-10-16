/// @file analysisjob.cpp
/// @brief 棋譜解析1回（エンジンの起動から全局面の解析まで）の実装

#include "analysisjob.h"

#include "thinkinginfoformatter.h"
#include "usi.h"
#include "usienginesession.h"

#include <utility>

namespace {

/// 手順の各局面（[0] が開始局面）。局面だけを送り、そこに至った手を読み筋の「同」の判定に使う
QList<UsiPosition> positionsOf(const RecordLine& line)
{
    QList<UsiPosition> positions;
    if (line.isEmpty()) return positions;
    positions.reserve(line.moveCount() + 1);
    for (int ply = 0; ply <= line.moveCount(); ++ply) {
        const std::optional<UsiMove> lastMove = ply > 0 ? std::optional<UsiMove>(line.usiMove(ply)) : std::nullopt;
        positions << UsiPosition::fromSfen(line.sfenAt(ply), lastMove).value_or(UsiPosition{});
    }
    return positions;
}

} // namespace

AnalysisJob::AnalysisJob(AnalysisRequest request, const Deps& deps, QObject* parent)
    : QObject(parent), m_request(std::move(request)), m_coordinator({positionsOf(m_request.line)}), m_engine(deps.engine)
{
    Q_ASSERT(m_engine);
    m_engine->setLogIdentity(QStringLiteral("E1"), {}, m_request.engineName);
    m_coordinator.setOptions(m_request.options);
    connect(&m_coordinator, &AnalysisCoordinator::requestSetOption, m_engine, &Usi::sendSetOptionCommand);
    connect(&m_coordinator, &AnalysisCoordinator::requestSearch, this, &AnalysisJob::onSearchRequested);
    connect(&m_coordinator, &AnalysisCoordinator::requestStop, m_engine, &Usi::stop);
    connect(&m_coordinator, &AnalysisCoordinator::positionPrepared, this, &AnalysisJob::onPositionPrepared);
    connect(&m_coordinator, &AnalysisCoordinator::analysisProgress, this, &AnalysisJob::progress);
    connect(&m_coordinator, &AnalysisCoordinator::analysisFinished, this, &AnalysisJob::onFinished);
    connect(m_engine, &Usi::engineInitialized, this, &AnalysisJob::onInitialized);
    connect(m_engine, &Usi::errorOccurred, this, &AnalysisJob::onError);
    connect(m_engine->protocol(), &UsiProtocolHandler::bestMoveReceived, this, &AnalysisJob::onBestMove);
    connect(m_engine->protocol(), &UsiProtocolHandler::infoReceived, this, &AnalysisJob::onInfo);
    connect(m_engine->thinking(), &ThinkingInfoFormatter::thinkingInfoUpdated, this, &AnalysisJob::onThinkingInfo);
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
        disconnect(m_engine->thinking(), nullptr, this, nullptr);
    }
    if (isRunning() && m_engine) m_engine->quit();
}

bool AnalysisJob::start()
{
    if (m_state != State::Ready || !m_engine) return false;
    const auto& line = m_request.line;
    if (line.isEmpty()) return false;
    m_state = State::Initializing;
    if (!m_engine->startEngine(m_request.enginePath, m_request.engineName)) {
        onFinished(AnalysisCoordinator::Idle);
        return false;
    }
    if (!m_engine->isInitializing()) onInitialized();
    return isRunning();
}

void AnalysisJob::onInitialized()
{
    if (m_state != State::Initializing) return;
    m_state = State::Analyzing;
    m_coordinator.startAnalyzeRange();
}

void AnalysisJob::stop()
{
    if (!isRunning()) return;
    m_coordinator.stop();
    onFinished(AnalysisCoordinator::Idle); // 初期化中は coordinator がまだ動いていない。
}

void AnalysisJob::onPositionPrepared(int ply, const QString& sfen)
{
    if (!m_engine || m_state != State::Analyzing) return;
    emit positionPrepared(ply, sfen);
}

void AnalysisJob::onSearchRequested(const UsiPosition& position, int thinkMs)
{
    if (!m_engine) return;
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::infinite();
    request.stopAfterMs = thinkMs;
    m_engine->search(request);
}

void AnalysisJob::onBestMove(const UsiBestMove& /*result*/)
{
    if (m_state != State::Analyzing || !m_engine) return;
    // bestmove の直前に届いた読み筋は、思考情報の整形（Usi が先に bestmove を受けて溜めた info を処理する）を終えている
    emit resultReady();
    if (m_state == State::Analyzing) m_coordinator.onEngineBestmoveReceived();
}

void AnalysisJob::onInfo(const UsiInfo& info)
{
    if (m_state == State::Analyzing) m_coordinator.onEngineInfo(info);
}

void AnalysisJob::onThinkingInfo(const ThinkingInfoRow& row)
{
    if (m_state == State::Analyzing) emit thinkingInfoUpdated(row);
}

void AnalysisJob::onFinished(AnalysisCoordinator::Mode mode)
{
    if (!isRunning()) return;
    m_state = State::Finished;
    if (m_engine) m_engine->quit();
    emit finished(mode);
}

void AnalysisJob::onError(const QString& message)
{
    if (!isRunning()) return;
    m_error = message;
    stop();
    emit errorOccurred(message);
}
