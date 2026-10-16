/// @file enginelifecyclemanager.cpp
/// @brief 対局で指すエンジンの生成・起動・破棄と、手番との対応の実装

#include "enginelifecyclemanager.h"

#include "logcategories.h"
#include "usi.h"

EngineLifecycleManager::EngineLifecycleManager(ShogiClock* clock, QObject* parent)
    : QObject(parent)
    , m_clockTime(clock)
{
}

EngineLifecycleManager::~EngineLifecycleManager() = default;

// ============================================================
// 生成・起動
// ============================================================

Usi* EngineLifecycleManager::createEngine(int slot, std::optional<Turn> side, const QString& engineName)
{
    Seat& target = seat(slot);
    destroySeat(target, false);

    const QString displayName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    auto* engine = new Usi(this);
    const QString sideTag = !side ? QString()
        : (*side == Turn::Black ? QStringLiteral("black") : QStringLiteral("white"));
    engine->setLogIdentity(slot == 2 ? QStringLiteral("E2") : QStringLiteral("E1"), sideTag, displayName);
    if (side) {
        // 対局のエンジンは対局の時計から持ち時間を読む（通信対局のエンジンはサーバーの残り時間を読む）
        engine->setMatchTimeSource(&m_clockTime);
        // 対局のエンジンの投了・宣言勝ちと、起動の失敗や異常終了を知らせる（起動の失敗は startEngine の中で
        // 届くことがある）。検討・詰み探索のエンジンのエラーは、使う側（AnalysisSession）が直接受け取る
        connect(engine, &Usi::resigned, this, &EngineLifecycleManager::engineResigned);
        connect(engine, &Usi::declaredWin, this, &EngineLifecycleManager::engineDeclaredWin);
        connect(engine, &Usi::errorOccurred, this, &EngineLifecycleManager::engineError);
    }

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

void EngineLifecycleManager::disconnectArbiterSignals()
{
    for (const Seat& s : m_seats) {
        if (!s.engine) continue;
        disconnect(s.engine, &Usi::resigned, this, nullptr);
        disconnect(s.engine, &Usi::declaredWin, this, nullptr);
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
        // 終局後の bestmove で着手・先読みを再開させない（取り消した探索の結果は届かない）。
        // quit に応答しない場合も非同期の終了監視へ引き渡し、アプリを閉じるまでプロセスを残さない
        s.engine->cancelCurrentOperation();
        s.engine->stop();
        s.engine->sendGameOverCommand(result);
        s.engine->quit(false);
    }
}

void EngineLifecycleManager::destroySeat(Seat& target, bool clearThinking)
{
    Usi* engine = std::exchange(target.engine, nullptr);
    target.side.reset();
    if (!engine) return;
    disconnect(engine, nullptr, this, nullptr);
    engine->quit(clearThinking);
    engine->deleteLater();
    emit enginesChanged();
}

void EngineLifecycleManager::destroyEngine(Usi* engine)
{
    for (Seat& s : m_seats) {
        if (engine && s.engine == engine) destroySeat(s, false);
    }
}

void EngineLifecycleManager::destroyEngines(bool clearDisplay)
{
    qCDebug(lcGame).noquote() << "destroyEngines called, clearDisplay=" << clearDisplay;
    for (Seat& s : m_seats) destroySeat(s, clearDisplay);
    if (clearDisplay) emit displayClearRequested();
}
