#ifndef STRATEGYCONTEXT_H
#define STRATEGYCONTEXT_H

/// @file strategycontext.h
/// @brief Strategy クラスが MatchCoordinator の内部状態にアクセスするためのネストクラス定義
///
/// C++11 以降、ネストクラスは囲むクラスの private メンバに暗黙的にアクセス可能。
/// これにより friend 宣言を使わずに Strategy が必要な内部状態のみを参照できる。

#include "matchcoordinator.h"
#include "matchturnhandler.h"
#include "matchtimekeeper.h"
#include "gameendhandler.h"
#include "enginelifecyclemanager.h"

/// @brief Strategy クラスが MatchCoordinator の内部状態にアクセスするためのコンテキスト
///
/// MatchCoordinator のネストクラスとして定義されるため、MatchCoordinator の
/// private メンバに暗黙的にアクセスできる。全メソッドは inline の1行委譲。
class MatchCoordinator::StrategyContext {
public:
    explicit StrategyContext(MatchCoordinator& c) : c_(c) {}

    // ---- 状態アクセサ ----

    Hooks& hooks() { return c_.m_hooks; }
    ShogiGameController* gc() { return c_.m_gc; }
    ShogiClock* clock() { return c_.m_timekeeper ? c_.m_timekeeper->clock() : nullptr; }

    Turn currentTurn() const { return c_.m_cur; }
    void setCurrentTurn(Turn p) { c_.m_cur = p; }

    PlayMode playMode() const { return c_.m_playMode; }
    PlayMode& playModeRef() { return c_.m_playMode; }
    int maxMoves() const { return c_.m_maxMoves; }

    Usi* usi1() const { return c_.m_engineManager ? c_.m_engineManager->usi1() : nullptr; }
    void setUsi1(Usi* u) { if (c_.m_engineManager) c_.m_engineManager->setUsi1(u); }
    Usi* usi2() const { return c_.m_engineManager ? c_.m_engineManager->usi2() : nullptr; }
    void setUsi2(Usi* u) { if (c_.m_engineManager) c_.m_engineManager->setUsi2(u); }

    UsiCommLogModel* comm1() const {
        auto pair = c_.m_engineManager ? c_.m_engineManager->ensureEngineModels(1) : EngineLifecycleManager::EngineModelPair{};
        return pair.comm;
    }
    void setComm1(UsiCommLogModel*) { /* managed by EngineLifecycleManager */ }
    ShogiEngineThinkingModel* think1() const {
        auto pair = c_.m_engineManager ? c_.m_engineManager->ensureEngineModels(1) : EngineLifecycleManager::EngineModelPair{};
        return pair.think;
    }
    void setThink1(ShogiEngineThinkingModel*) { /* managed by EngineLifecycleManager */ }

    int currentMoveIndex() const { return c_.m_currentMoveIndex; }
    void setCurrentMoveIndex(int i) { c_.m_currentMoveIndex = i; }

    QStringList* sfenHistory() { return c_.m_sfenHistory; }

    QString& positionStr1() { return c_.m_positionStr1; }
    QString& positionPonder1() { return c_.m_positionPonder1; }
    QString& positionStr2() { return c_.m_positionStr2; }
    QString& positionPonder2() { return c_.m_positionPonder2; }

    QStringList& positionStrHistory() { return c_.m_positionStrHistory; }

    QList<ShogiMove>& gameMovesRef() { return c_.gameMovesRef(); }

    const GameOverState& gameOverState() const { return c_.m_gameOver; }

    /// MatchCoordinator を QObject 親として使う場合のアクセサ
    QObject* coordinatorAsParent() { return &c_; }

    // ---- private メソッド委譲 ----

    void updateTurnDisplay(Turn p) { c_.m_turnHandler->updateTurnDisplay(p); }
    void initPositionStringsFromSfen(const QString& s) { c_.m_turnHandler->initPositionStringsFromSfen(s); }
    void wireResignToArbiter(Usi* eng, Turn side) {
        if (c_.m_engineManager) c_.m_engineManager->wireResignToArbiter(eng, side);
    }
    void wireWinToArbiter(Usi* eng, Turn side) {
        if (c_.m_engineManager) c_.m_engineManager->wireWinToArbiter(eng, side);
    }

    // ---- public メソッド委譲 ----

    bool checkAndHandleSennichite() {
        c_.ensureGameEndHandler();
        return c_.m_gameEndHandler->checkAndHandleSennichite();
    }
    void destroyEngines(bool clearModels = true) { c_.destroyEngines(clearModels); }
    void initializeAndStartEngineFor(Turn side, const QString& path, const QString& name) {
        c_.initializeAndStartEngineFor(side, path, name);
    }
    Usi* primaryEngine() const { return c_.primaryEngine(); }
    void handleMaxMovesJishogi() { c_.handleMaxMovesJishogi(); }
    void handleEngineIllegalMove(Turn side) { c_.handleEngineIllegalMove(side); }
    void pokeTimeUpdateNow() { c_.pokeTimeUpdateNow(); }
private:
    MatchCoordinator& c_;
};

#endif // STRATEGYCONTEXT_H
