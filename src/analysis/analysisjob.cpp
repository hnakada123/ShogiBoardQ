#include "analysisjob.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "sfenutils.h"
#include <utility>

AnalysisJob::AnalysisJob(AnalysisRequest request, const Dependencies& deps, QObject* parent)
    : QObject(parent), m_request(std::move(request)), m_coordinator({m_request.line.sfens()}), m_engine(deps.engine)
{
    UsiCommLogModel* log = deps.log;
    if (!m_engine) {
        if (!log) log = new UsiCommLogModel(this);
        auto* thinking = deps.thinking ? deps.thinking : new ShogiEngineThinkingModel(this);
        m_engine = new Usi(log, thinking, this);
    }
    if (log) log->setEngineName(m_request.engineName);
    m_engine->setLogIdentity(QStringLiteral("[E1]"), {}, m_request.engineName);
    m_coordinator.setOptions(m_request.options);
    connect(&m_coordinator, &AnalysisCoordinator::requestSetOption, m_engine, &Usi::sendSetOptionCommand);
    connect(&m_coordinator, &AnalysisCoordinator::requestSearch, m_engine, &Usi::sendPositionAndGoInfinite);
    connect(&m_coordinator, &AnalysisCoordinator::requestStop, m_engine, &Usi::sendStopCommand);
    connect(&m_coordinator, &AnalysisCoordinator::positionPrepared, this, &AnalysisJob::onPositionPrepared);
    connect(&m_coordinator, &AnalysisCoordinator::analysisProgress, this, &AnalysisJob::progress);
    connect(&m_coordinator, &AnalysisCoordinator::analysisFinished, this, &AnalysisJob::onFinished);
    connect(m_engine, &Usi::engineInitialized, this, &AnalysisJob::onInitialized);
    connect(m_engine, &Usi::bestMoveReceived, this, &AnalysisJob::onBestMove);
    connect(m_engine, &Usi::infoReceived, this, &AnalysisJob::onInfo);
    connect(m_engine, &Usi::thinkingInfoUpdated, this, &AnalysisJob::onThinkingInfo);
    connect(m_engine, &Usi::errorOccurred, this, &AnalysisJob::onError);
}

AnalysisJob::~AnalysisJob()
{
    if (m_engine) disconnect(m_engine, nullptr, this, nullptr);
    if (isRunning() && m_engine) m_engine->sendQuitCommand();
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
    const auto move = m_request.line.usiMove(ply);
    m_engine->setBaseSfen(SfenUtils::normalizePositionLikeSfen(sfen));
    m_engine->setLastUsiMove(move.toString());
    m_engine->setPreviousMove(move.to);
    m_engine->requestClearThinkingInfo();
    emit positionPrepared(ply, sfen);
}

void AnalysisJob::onBestMove()
{
    if (m_state != State::Analyzing || !m_engine) return;
    m_engine->flushThinkingInfoBuffer();
    emit resultReady();
    if (m_state == State::Analyzing) m_coordinator.onEngineBestmoveReceived({});
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
    if (m_engine) m_engine->sendQuitCommand();
    emit finished(mode);
}

void AnalysisJob::onError(const QString& message)
{
    if (!isRunning()) return;
    m_error = message;
    stop();
    emit errorOccurred(message);
}
