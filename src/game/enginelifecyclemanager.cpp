/// @file enginelifecyclemanager.cpp
/// @brief 対局で指すエンジンの生成・起動・破棄と、手番との対応の実装

#include "enginelifecyclemanager.h"

#include "logcategories.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"
#include "usicommlogmodel.h"

EngineLifecycleManager::EngineLifecycleManager(ShogiClock* clock, QObject* parent)
    : QObject(parent)
    , m_clock(clock)
{
}

EngineLifecycleManager::~EngineLifecycleManager() = default;

void EngineLifecycleManager::setModels(const Models& models)
{
    m_models = models;
}

// ============================================================
// 生成・起動
// ============================================================

Usi* EngineLifecycleManager::createEngine(int slot, std::optional<Turn> side, const QString& engineName)
{
    Seat& target = seat(slot);
    destroySeat(target, false);

    UsiCommLogModel* comm = (slot == 2) ? m_models.comm2 : m_models.comm1;
    ShogiEngineThinkingModel* think = (slot == 2) ? m_models.think2 : m_models.think1;
    const QString displayName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    if (comm) comm->setEngineName(displayName);

    auto* engine = new Usi(comm, think, this);
    engine->setMatchClock(m_clock);
    const QString sideTag = !side ? QString()
        : (*side == Turn::Black ? QStringLiteral("black") : QStringLiteral("white"));
    engine->setLogIdentity(slot == 2 ? QStringLiteral("[E2]") : QStringLiteral("[E1]"), sideTag, displayName);
    engine->setSquelchResignLogging(false);
    connect(engine, &Usi::bestMoveResignReceived, this, &EngineLifecycleManager::onEngineResign);
    connect(engine, &Usi::bestMoveWinReceived, this, &EngineLifecycleManager::onEngineWin);
    // 対局のエンジンの起動の失敗や異常終了を知らせる（起動の失敗は startEngine の中で届くことがある）。
    // 検討・詰み探索のエンジンのエラーは、使う側（AnalysisSession）が直接受け取る
    if (side) connect(engine, &Usi::errorOccurred, this, &EngineLifecycleManager::engineError);

    target.engine = engine;
    target.side = side;
    emit enginesChanged();
    return engine;
}

void EngineLifecycleManager::startEngine(Usi* engine, const QString& enginePath, const QString& engineName)
{
    // 失敗は Usi::errorOccurred で知らせる
    if (engine) (void)engine->startEngine(enginePath, engineName);
}

// ============================================================
// 手番との対応
// ============================================================

Usi* EngineLifecycleManager::engineFor(Turn side) const
{
    for (const Seat& s : m_seats) {
        if (s.engine && s.side == side) return s.engine;
    }
    return nullptr;
}

Usi* EngineLifecycleManager::engineInSlot(int slot) const
{
    return seat(slot).engine;
}

int EngineLifecycleManager::slotFor(Turn side) const
{
    if (seat(1).engine && seat(1).side == side) return 1;
    if (seat(2).engine && seat(2).side == side) return 2;
    return 0;
}

std::optional<Turn> EngineLifecycleManager::sideOf(const QObject* engine) const
{
    for (const Seat& s : m_seats) {
        if (s.engine && s.engine == engine) return s.side;
    }
    return std::nullopt;
}

void EngineLifecycleManager::onEngineResign()
{
    if (const auto side = sideOf(sender())) emit engineResigned(*side);
}

void EngineLifecycleManager::onEngineWin()
{
    if (const auto side = sideOf(sender())) emit engineDeclaredWin(*side);
}

void EngineLifecycleManager::disconnectArbiterSignals()
{
    for (const Seat& s : m_seats) {
        if (!s.engine) continue;
        disconnect(s.engine, &Usi::bestMoveResignReceived, this, nullptr);
        disconnect(s.engine, &Usi::bestMoveWinReceived, this, nullptr);
    }
}

// ============================================================
// 終局・破棄
// ============================================================

void EngineLifecycleManager::notifyGameOver(std::optional<Turn> loser)
{
    for (const Seat& s : m_seats) {
        if (!s.engine || !s.side) continue;
        const GameOverResult result = !loser ? GameOverResult::Draw
            : (*loser == *s.side ? GameOverResult::Lose : GameOverResult::Win);
        // 終局後の bestmove で着手・先読みを再開させない。quit に応答しない場合も
        // 非同期の終了監視へ引き渡し、アプリを閉じるまでプロセスを残さない。
        s.engine->setSquelchResignLogging(true);
        s.engine->cancelCurrentOperation();
        s.engine->sendStopCommand();
        s.engine->sendGameOverCommand(result);
        s.engine->cleanupEngineProcessAndThread(false);
    }
}

void EngineLifecycleManager::destroySeat(Seat& target, bool clearThinking)
{
    Usi* engine = std::exchange(target.engine, nullptr);
    target.side.reset();
    if (!engine) return;
    disconnect(engine, nullptr, this, nullptr);
    engine->cleanupEngineProcessAndThread(clearThinking);
    engine->deleteLater();
    emit enginesChanged();
}

void EngineLifecycleManager::destroyEngines(bool clearModels)
{
    qCDebug(lcGame).noquote() << "destroyEngines called, clearModels=" << clearModels;
    for (Seat& s : m_seats) destroySeat(s, clearModels);
    if (clearModels) {
        if (m_models.comm1) m_models.comm1->clear();
        if (m_models.think1) m_models.think1->clearAllItems();
        if (m_models.comm2) m_models.comm2->clear();
        if (m_models.think2) m_models.think2->clearAllItems();
    }
}
