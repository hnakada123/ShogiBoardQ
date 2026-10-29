/// @file kifuanalysissession.cpp
/// @brief 棋譜解析の実行（エンジンの枠と動作状態のリース・手順の局面の列の解析）の実装

#include "kifuanalysissession.h"

#include "logcategories.h"
#include "usi.h"

#include <utility>

KifuAnalysisSession::KifuAnalysisSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.activity && (m_deps.engines || m_deps.engine));
    if (m_deps.engines)
        connect(m_deps.engines, &EngineSlots::engineRevoked, this, &KifuAnalysisSession::onEngineRevoked);
    connect(m_deps.activity, &AppActivityState::revoked, this, &KifuAnalysisSession::onActivityRevoked);
}

// 解析中に壊すときは、実行器がエンジンを終える（画面の部品にはもう知らせない）
KifuAnalysisSession::~KifuAnalysisSession() = default;

Usi* KifuAnalysisSession::engine() const
{
    return m_deps.engine ? m_deps.engine : m_lease.engine();
}

bool KifuAnalysisSession::start(AnalysisRequest request)
{
    if (request.line.isEmpty()) return false;
    stop();
    // 範囲を手順に収める（終わりの -1 は末尾まで）
    const int last = request.line.moveCount();
    AnalysisRequest::Options& options = request.options;
    options.startPly = qBound(0, options.startPly, last);
    options.endPly = options.endPly < 0 ? last : qBound(options.startPly, options.endPly, last);
    m_request = std::move(request);

    // 解析のエンジンは枠 E1 に置く（思考情報・通信ログは E1 の表示に出る）
    if (!m_deps.engine) m_lease = m_deps.engines->acquire(this, 1, m_request.engineName);
    Usi* const usi = engine();
    if (!usi) return false;
    usi->setLogIdentity(QStringLiteral("E1"), {}, m_request.engineName);

    m_runner = std::make_unique<EngineAnalysisRunner>(usi->session());
    connect(m_runner.get(), &EngineAnalysisRunner::positionStarted, this, &KifuAnalysisSession::plyStarted);
    connect(m_runner.get(), &EngineAnalysisRunner::positionFinished, this, &KifuAnalysisSession::onPositionFinished);
    connect(m_runner.get(), &EngineAnalysisRunner::finished, this, &KifuAnalysisSession::onRunnerFinished);
    connect(m_runner.get(), &EngineAnalysisRunner::errorOccurred, this, &KifuAnalysisSession::onRunnerError);

    // 局面は CLI の analyze-kifu と同じく開始局面からの手順で送る。候補手の数は送らない（エンジンの設定のまま。
    // 結果は最善の読み筋で決める）。stop の後に bestmove を返さないエンジンは、CLI と同じ時間で打ち切る
    // （席を外して回していても、止まったまま「解析中」にならない）
    EngineAnalysisRunner::Request run;
    run.enginePath = m_request.enginePath;
    run.engineName = m_request.engineName;
    run.positions = m_request.line.usiPositions(options.startPly, options.endPly);
    run.firstPly = options.startPly;
    run.thinkMs = options.movetimeMs;
    run.multiPv = 0;
    run.replyTimeoutMs = EngineAnalysisRunner::kDefaultReplyTimeoutMs;

    m_activityLease = m_deps.activity->begin(AppActivityState::Activity::DuringAnalysis, this);
    // 起動の失敗は、戻った後に onRunnerError で届く
    m_runner->start(run);
    return true;
}

void KifuAnalysisSession::stop()
{
    if (isRunning()) m_runner->cancel();
}

void KifuAnalysisSession::onPositionFinished(const PlyAnalysis& result, const UsiBestMove& /*bestMove*/)
{
    emit plyAnalyzed(result);
}

void KifuAnalysisSession::onRunnerFinished(bool stopped)
{
    qCDebug(lcAnalysis) << "kifu analysis finished, stopped=" << stopped;
    finishRun(!stopped);
}

void KifuAnalysisSession::onRunnerError(const QString& message)
{
    qCWarning(lcAnalysis).noquote() << "Engine error during analysis:" << message;
    // 実行器の文は CLI 向けの英語。応答がなくて打ち切ったときは訳した文にする
    const QString text = m_runner && m_runner->isTimedOut()
        ? tr("エンジンが停止の指示に%1秒以内に応答しませんでした。")
              .arg(EngineAnalysisRunner::kDefaultReplyTimeoutMs / 1000)
        : message;
    finishRun(false);
    emit errorOccurred(text);
}

void KifuAnalysisSession::onEngineRevoked(QObject* owner)
{
    if (owner == this) stop();
}

void KifuAnalysisSession::onActivityRevoked(QObject* owner)
{
    if (owner == this) stop();
}

void KifuAnalysisSession::finishRun(bool completed)
{
    // 実行器はエンジンを終えている。思考情報の表示も消す（枠は次の解析まで持つ）
    if (Usi* const usi = engine()) usi->quit();
    // 自分の動作状態のときだけ待機中に戻す（ほかの部品が始めた動作は変えない）
    m_activityLease.reset();
    emit finished(completed);
}
