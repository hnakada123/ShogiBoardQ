#ifndef STRATEGYCONTEXT_H
#define STRATEGYCONTEXT_H

#include "matchsessionstate.h"
#include "matchcoordinatorhooks.h"
#include "matchtimekeeper.h"
#include "gameendhandler.h"
#include "enginelifecyclemanager.h"

/// 対局モードに必要なサービスを明示的に受け取る。司令塔の内部にはアクセスしない。
class MatchStrategyContext {
public:
    struct Deps {
        ShogiGameController* gc = nullptr;
        MatchSessionState* session = nullptr;
        EngineLifecycleManager* engines = nullptr;
        MatchTimekeeper* timekeeper = nullptr;
        GameEndHandler* gameEnd = nullptr;
        QStringList* sfens = nullptr;
        QList<ShogiMove>* moves = nullptr;
        MatchCoordinatorHooks hooks;
        QObject* parent = nullptr;
    };
    explicit MatchStrategyContext(const Deps& deps) : m_deps(deps) {}

    const MatchCoordinatorHooks& hooks() const { return m_deps.hooks; }
    ShogiGameController* gc() const { return m_deps.gc; }
    ShogiClock* clock() const { return m_deps.timekeeper->clock(); }
    Turn currentTurn() const { return gc() ? gc()->currentPlayer() : Turn::Black; }
    PlayMode playMode() const { return m_deps.session->playMode(); }
    int maxMoves() const { return m_deps.session->maxMoves(); }
    Usi* usi1() const { return m_deps.engines->usi1(); }
    void setUsi1(Usi* engine) { m_deps.engines->setUsi1(engine); }
    Usi* usi2() const { return m_deps.engines->usi2(); }
    void setUsi2(Usi* engine) { m_deps.engines->setUsi2(engine); }
    UsiCommLogModel* comm1() const { return m_deps.engines->ensureEngineModels(1).comm; }
    ShogiEngineThinkingModel* think1() const { return m_deps.engines->ensureEngineModels(1).think; }
    int currentMoveIndex() const { return static_cast<int>(qMax<qsizetype>(0, m_deps.sfens->size() - 1)); }
    const QStringList* sfenHistory() const { return m_deps.sfens; }
    const MatchGameOverState& gameOverState() const { return m_deps.session->gameOver(); }
    QObject* coordinatorAsParent() const { return m_deps.parent; }

    void setEnginePonder(int slot, const QString& ponder) { m_deps.session->setPonder(slot, ponder); }
    void requestEngineMove(Usi* engine, int slot);
    void updateTurnDisplay(Turn side) { if (hooks().ui.updateTurnDisplay) hooks().ui.updateTurnDisplay(side); }
    void wireResignToArbiter(Usi* engine, Turn side) { m_deps.engines->wireResignToArbiter(engine, side); }
    void wireWinToArbiter(Usi* engine, Turn side) { m_deps.engines->wireWinToArbiter(engine, side); }
    bool completeHumanMove(Turn mover, const QString& text, const QPoint& from, const QPoint& to);
    bool applyEngineMove(const UsiMove& move, Turn mover, qint64 elapsedMs, bool appendEvaluation);
    bool checkAndHandleSennichite() { return m_deps.gameEnd->checkAndHandleSennichite(); }
    void destroyEngines(bool clearModels = true) { m_deps.engines->destroyEngines(clearModels); }
    void initializeAndStartEngineFor(Turn side, const QString& path, const QString& name);
    Usi* primaryEngine() const { return usi1(); }
    void handleMaxMovesJishogi() { m_deps.gameEnd->handleMaxMovesJishogi(); }
    void handleEngineIllegalMove(Turn side) { m_deps.gameEnd->handleEngineIllegalMove(side); }
    void pokeTimeUpdateNow() { m_deps.timekeeper->pokeTimeUpdateNow(); }

private:
    bool completeMove(Turn mover, const QString& text, const QPoint& from, const QPoint& to,
                      bool appendEvaluation);
    Deps m_deps;
};

#endif
