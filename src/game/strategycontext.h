#ifndef STRATEGYCONTEXT_H
#define STRATEGYCONTEXT_H

/// @file strategycontext.h
/// @brief Strategy クラスが MatchCoordinator の内部状態にアクセスするためのネストクラス定義
///
/// C++11 以降、ネストクラスは囲むクラスの private メンバに暗黙的にアクセス可能。
/// これにより friend 宣言を使わずに Strategy が必要な内部状態のみを参照できる。

#include "matchcoordinator.h"

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
    ShogiClock* clock() { return c_.m_clock; }

    Player currentTurn() const { return c_.m_cur; }
    void setCurrentTurn(Player p) { c_.m_cur = p; }

    PlayMode playMode() const { return c_.m_playMode; }
    PlayMode& playModeRef() { return c_.m_playMode; }
    int maxMoves() const { return c_.m_maxMoves; }

    Usi* usi1() const { return c_.m_usi1; }
    void setUsi1(Usi* u) { c_.m_usi1 = u; }
    Usi* usi2() const { return c_.m_usi2; }
    void setUsi2(Usi* u) { c_.m_usi2 = u; }

    UsiCommLogModel* comm1() const { return c_.m_comm1; }
    void setComm1(UsiCommLogModel* m) { c_.m_comm1 = m; }
    ShogiEngineThinkingModel* think1() const { return c_.m_think1; }
    void setThink1(ShogiEngineThinkingModel* m) { c_.m_think1 = m; }

    int currentMoveIndex() const { return c_.m_currentMoveIndex; }
    void setCurrentMoveIndex(int i) { c_.m_currentMoveIndex = i; }

    QStringList* sfenHistory() { return c_.m_sfenHistory; }

    QString& positionStr1() { return c_.m_positionStr1; }
    QString& positionPonder1() { return c_.m_positionPonder1; }
    QString& positionStr2() { return c_.m_positionStr2; }
    QString& positionPonder2() { return c_.m_positionPonder2; }

    QStringList& positionStrHistory() { return c_.m_positionStrHistory; }

    QVector<ShogiMove>& gameMovesDirect() { return c_.m_gameMoves; }
    QVector<ShogiMove>& gameMovesRef() { return c_.gameMovesRef(); }

    const GameOverState& gameOverState() const { return c_.m_gameOver; }
    const TimeControl& timeControl() const { return c_.m_tc; }

    /// MatchCoordinator を QObject 親として使う場合のアクセサ
    QObject* coordinatorAsParent() { return &c_; }

    // ---- private メソッド委譲 ----

    void updateTurnDisplay(Player p) { c_.updateTurnDisplay(p); }
    GoTimes computeGoTimes() const { return c_.computeGoTimes(); }
    void initPositionStringsFromSfen(const QString& s) { c_.initPositionStringsFromSfen(s); }
    void wireResignToArbiter(Usi* eng, bool asP1) { c_.wireResignToArbiter(eng, asP1); }
    void wireWinToArbiter(Usi* eng, bool asP1) { c_.wireWinToArbiter(eng, asP1); }

    // ---- public メソッド委譲 ----

    bool checkAndHandleSennichite() { return c_.checkAndHandleSennichite(); }
    void destroyEngines(bool clearModels = true) { c_.destroyEngines(clearModels); }
    void initializeAndStartEngineFor(Player side, const QString& path, const QString& name) {
        c_.initializeAndStartEngineFor(side, path, name);
    }
    Usi* primaryEngine() const { return c_.primaryEngine(); }
    void computeGoTimesForUSI(qint64& outB, qint64& outW) const { c_.computeGoTimesForUSI(outB, outW); }
    void handleMaxMovesJishogi() { c_.handleMaxMovesJishogi(); }
    void handleTimeUpdated() { c_.handleTimeUpdated(); }
    void pokeTimeUpdateNow() { c_.pokeTimeUpdateNow(); }
    bool engineThinkApplyMove(Usi* engine, QString& positionStr, QString& ponderStr,
                              QPoint* outFrom, QPoint* outTo) {
        return c_.engineThinkApplyMove(engine, positionStr, ponderStr, outFrom, outTo);
    }

private:
    MatchCoordinator& c_;
};

#endif // STRATEGYCONTEXT_H
