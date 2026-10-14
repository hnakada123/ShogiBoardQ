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
/// private メンバにアクセスできる。Strategy には状態の読み取りと操作を公開し、
/// 手数・棋譜・探索局面の書き換えをこの境界内で行う。
class MatchCoordinator::StrategyContext {
public:
    explicit StrategyContext(MatchCoordinator& c) : c_(c) {}

    // ---- 状態アクセサ ----

    const Hooks& hooks() const { return c_.m_hooks; }
    ShogiGameController* gc() { return c_.m_gc; }
    ShogiClock* clock() { return c_.m_timekeeper ? c_.m_timekeeper->clock() : nullptr; }

    /// 手番（対局中の正は GC）
    Turn currentTurn() const { return c_.m_gc ? c_.m_gc->currentPlayer() : Turn::Black; }

    PlayMode playMode() const { return c_.m_playMode; }
    int maxMoves() const { return c_.m_maxMoves; }

    Usi* usi1() const { return c_.m_engineManager ? c_.m_engineManager->usi1() : nullptr; }
    void setUsi1(Usi* u) { if (c_.m_engineManager) c_.m_engineManager->setUsi1(u); }
    Usi* usi2() const { return c_.m_engineManager ? c_.m_engineManager->usi2() : nullptr; }
    void setUsi2(Usi* u) { if (c_.m_engineManager) c_.m_engineManager->setUsi2(u); }

    UsiCommLogModel* comm1() const {
        auto pair = c_.m_engineManager ? c_.m_engineManager->ensureEngineModels(1) : EngineLifecycleManager::EngineModelPair{};
        return pair.comm;
    }
    ShogiEngineThinkingModel* think1() const {
        auto pair = c_.m_engineManager ? c_.m_engineManager->ensureEngineModels(1) : EngineLifecycleManager::EngineModelPair{};
        return pair.think;
    }

    int currentMoveIndex() const { return c_.m_currentMoveIndex; }

    const QStringList* sfenHistory() const { return c_.m_sfenHistory; }

    void setEnginePosition(int slot, const QString& position, const QString& ponder);
    void syncOpponentPosition(int moverSlot, const QString& position);
    void requestEngineMove(Usi* engine, int slot);
    void requestHumanReply(Usi* engine, const UsiMove& humanMove);

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

    /// 適用済みの人間手と、検証・適用が必要なエンジン手の入口。
    bool completeHumanMove(Turn mover, const QString& text, const QPoint& from, const QPoint& to);
    bool applyEngineMove(const UsiMove& move, Turn mover, qint64 elapsedMs, bool appendEvaluation);

    // ---- public メソッド委譲 ----

    bool checkAndHandleSennichite() {
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
    bool completeMove(Turn mover, const QString& text, const QPoint& from, const QPoint& to,
                      bool appendEvaluation);
    MatchCoordinator& c_;
};

#endif // STRATEGYCONTEXT_H
