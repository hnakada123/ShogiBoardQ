/// @file matchengineseat.cpp
/// @brief 対局で指すエンジン1台の実装

#include "matchengineseat.h"

#include "usi.h"

MatchEngineSeat::MatchEngineSeat(EngineLifecycleManager* engines, QObject* parent)
    : QObject(parent)
    , m_engines(engines)
{
    Q_ASSERT(m_engines);
    connect(m_engines, &EngineLifecycleManager::engineRevoked, this, &MatchEngineSeat::onEngineRevoked);
}

MatchEngineSeat::~MatchEngineSeat() = default;

void MatchEngineSeat::start(const Params& params)
{
    release();
    m_side = params.side;
    m_lease = m_engines->acquire(this, params.slot, params.engineName);
    Usi* engine = m_lease.engine();
    const QString role = !params.role.isEmpty() ? params.role
        : (params.side == Turn::Black ? QStringLiteral("black") : QStringLiteral("white"));
    engine->setLogIdentity(params.slot == 2 ? QStringLiteral("E2") : QStringLiteral("E1"), role,
                           params.engineName.isEmpty() ? QStringLiteral("Engine") : params.engineName);
    engine->setMatchTimeSource(params.timeSource);
    connect(engine, &Usi::engineInitialized, this, &MatchEngineSeat::initialized);
    connect(engine, &Usi::matchMoveReady, this, &MatchEngineSeat::moveReady);
    connect(engine, &Usi::resigned, this, &MatchEngineSeat::resigned);
    connect(engine, &Usi::declaredWin, this, &MatchEngineSeat::declaredWin);
    connect(engine, &Usi::errorOccurred, this, &MatchEngineSeat::errorOccurred);
    // 起動の失敗は戻った後に errorOccurred で届く
    engine->startEngine(params.enginePath, params.engineName);
}

Usi* MatchEngineSeat::engine() const
{
    return m_lease.engine();
}

bool MatchEngineSeat::isInitializing() const
{
    const Usi* engine = this->engine();
    return engine && engine->isInitializing();
}

bool MatchEngineSeat::isReady() const
{
    const Usi* engine = this->engine();
    return engine && engine->isRunning() && !engine->isInitializing();
}

void MatchEngineSeat::finish(GameOverResult result)
{
    Usi* engine = this->engine();
    if (!engine) return;
    // 終局を知らせた後に届く知らせ（投了の bestmove など）は受けない
    disconnect(engine, nullptr, this, nullptr);
    // 終局後の bestmove で着手・先読みを再開させない（取り消した探索の結果は届かない）
    engine->cancelCurrentOperation();
    engine->stop();
    engine->sendGameOverCommand(result);
    engine->quit(false);
}

void MatchEngineSeat::release()
{
    if (Usi* engine = this->engine()) disconnect(engine, nullptr, this, nullptr);
    // 枠を空ける（quit を送り、後で破棄する。思考情報・通信ログの表示は残す）
    m_lease.reset();
}

void MatchEngineSeat::onEngineRevoked(QObject* owner)
{
    if (owner != this) return;
    // 対局の最中にほかの処理が枠を取った。エンジンの異常と同じく知らせる
    emit errorOccurred(tr("エンジンを別の処理が使い始めたため、対局のエンジンを止めました"));
}
