/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"
#include "boardcoordinates.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "usitimingparams.h"
#include "playmode.h"

#include <QTimer>

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                                 QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
}

void EngineVsEngineStrategy::start()
{
    if (!m_ctx.usi1() || !m_ctx.usi2() || !m_ctx.gc()) return;

    // EvE対局で初手からタイマーを動作させるため、ここで時計を開始する
    if (m_ctx.clock()) {
        m_ctx.clock()->startClock();
        qCDebug(lcGame) << "Clock started";
    }

    // 駒落ちの場合、SFENで手番が「w」（後手番）になっている。GC の手番は SFEN に従う
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());


    connect(m_ctx.usi1(), &Usi::engineInitialized, this, &EngineVsEngineStrategy::kickNextEvETurn, Qt::UniqueConnection);
    connect(m_ctx.usi2(), &Usi::engineInitialized, this, &EngineVsEngineStrategy::kickNextEvETurn, Qt::UniqueConnection);
    connect(m_ctx.usi1(), &Usi::matchMoveReady, this, &EngineVsEngineStrategy::onEngineMoveReady, Qt::UniqueConnection);
    connect(m_ctx.usi2(), &Usi::matchMoveReady, this, &EngineVsEngineStrategy::onEngineMoveReady, Qt::UniqueConnection);
    kickNextEvETurn();
}

// ============================================================
// 人間の着手処理（EvEでは呼ばれない）
// ============================================================

void EngineVsEngineStrategy::onHumanMove(const QPoint& /*from*/, const QPoint& /*to*/,
                                           const QString& /*prettyMove*/)
{
    // EvE では人間の手はないため空実装
}

// ============================================================
// EvE ターンループ
// ============================================================

void EngineVsEngineStrategy::kickNextEvETurn()
{
    if (m_waitingForMove || m_ctx.gameOverState().isOver) return;
    if (m_ctx.playMode() != PlayMode::EvenEngineVsEngine
        && m_ctx.playMode() != PlayMode::HandicapEngineVsEngine)
        return;
    if (!m_ctx.usi1() || !m_ctx.usi2() || !m_ctx.gc()) return;
    if (m_ctx.usi1()->isInitializing() || m_ctx.usi2()->isInitializing()) {
        m_waitingInitialization = true;
        if (m_ctx.clock()) m_ctx.clock()->stopClock();
        return;
    }
    if (m_waitingInitialization) {
        m_waitingInitialization = false;
        if (m_ctx.clock()) m_ctx.clock()->startClock();
    }

    const bool p1ToMove = (m_ctx.gc()->currentPlayer() == Turn::Black);
    Usi* mover    = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();

    m_waitingForMove = true;
    m_ctx.requestEngineMove(mover, p1ToMove ? 1 : 2);
}

void EngineVsEngineStrategy::onEngineMoveReady(const UsiMove& move,
                                               const QString& /*position*/, const QString& ponder)
{
    if (!m_waitingForMove || m_ctx.gameOverState().isOver || !m_ctx.gc()) return;
    const bool p1ToMove = m_ctx.gc()->currentPlayer() == Turn::Black;
    Usi* mover = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();
    if (sender() != mover) return;
    m_waitingForMove = false;
    Usi* receiver = p1ToMove ? m_ctx.usi2() : m_ctx.usi1();
    m_ctx.setEnginePonder(p1ToMove ? 1 : 2, ponder);
    const QPoint to = BoardCoordinates::toSquare(move);
    if (receiver) {
        receiver->setPreviousMove(BoardSquare{to.x(), to.y()});
    }
    if (!m_ctx.applyEngineMove(move, p1ToMove ? Turn::Black : Turn::White,
                               mover->lastBestmoveElapsedMs(), false)) return;

    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn);
}
