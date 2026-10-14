/// @file humanvsenginestrategy.cpp
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "humanvsenginestrategy.h"
#include "boardcoordinates.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "usitimingparams.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "logcategories.h"

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                               Turn engineSide,
                                               QString enginePath,
                                               QString engineName)
    : QObject(ctx.coordinatorAsParent())
    , m_ctx(ctx)
    , m_engineSide(engineSide)
    , m_enginePath(std::move(enginePath))
    , m_engineName(std::move(engineName))
{
}

// ============================================================
// 対局開始
// ============================================================

void HumanVsEngineStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvE (engineSide=" << m_engineSide << ")";

    // 以前のエンジンは破棄（安全化）
    m_ctx.destroyEngines();

    // HvEでは思考モデルは常にエンジン1スロット（m_comm1/m_think1）を使用
    UsiCommLogModel*          comm  = m_ctx.comm1();
    ShogiEngineThinkingModel* think = m_ctx.think1();

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName = m_engineName.isEmpty() ? QStringLiteral("Engine") : m_engineName;
    if (comm) comm->setEngineName(dispName);

    // USI を生成（この時点ではプロセス未起動）
    // HvEでは常に m_usi1 を使用
    // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks) - Qt parent-child 所有権により管理される
    m_ctx.setUsi1(new Usi(comm, think, m_ctx.gc(), m_ctx.coordinatorAsParent()));
    m_ctx.setUsi2(nullptr); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    // 投了・入玉宣言勝ちは、エンジンの手番の知らせとして配線する
    m_ctx.wireResignToArbiter(m_ctx.usi1(), m_engineSide);
    m_ctx.wireWinToArbiter(m_ctx.usi1(), m_engineSide);

    // ログ識別（UI の通信ログは枠の E1、デバッグログは手番も付ける）
    if (m_ctx.usi1()) {
        m_ctx.usi1()->setLogIdentity(QStringLiteral("[E1]"),
                                     m_engineSide == Turn::Black ? QStringLiteral("black") : QStringLiteral("white"),
                                     dispName);
        m_ctx.usi1()->setSquelchResignLogging(false);
    }

    connect(m_ctx.usi1(), &Usi::engineInitialized, this, &HumanVsEngineStrategy::onEngineInitialized);
    connect(m_ctx.usi1(), &Usi::matchMoveReady, this, &HumanVsEngineStrategy::onEngineMoveReady);

    // USI エンジンを起動（path/name 必須）
    m_ctx.initializeAndStartEngineFor(m_engineSide, m_enginePath, m_engineName);

    // --- 手番の単一ソースを確立：GC → m_cur → 表示 ---
    const Turn side = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::Black;
    m_ctx.setCurrentTurn(side);

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_ctx.hooks().ui.renderBoardFromGc) m_ctx.hooks().ui.renderBoardFromGc();
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());
}

// ============================================================
// 人間の着手処理（3引数版：考慮時間確定→棋譜追記→エンジン返し手）
// ============================================================

void HumanVsEngineStrategy::onHumanMove(const QPoint& humanFrom,
                                          const QPoint& humanTo,
                                          const QString& prettyMove)
{
    const Turn humanSide = (m_engineSide == Turn::Black) ? Turn::White : Turn::Black;

    finishHumanTimerInternal();
    if (m_ctx.completeHumanMove(humanSide, prettyMove, humanFrom, humanTo))
        onHumanMoveEngineReply(humanFrom, humanTo);
}

// ============================================================
// エンジン返し手（2引数版：エンジン go→bestmove→盤反映）
// ============================================================

void HumanVsEngineStrategy::onHumanMoveEngineReply(const QPoint& /*humanFrom*/,
                                                     const QPoint& humanTo)
{
    if (Usi* eng = m_ctx.primaryEngine()) {
        eng->setPreviousFileTo(humanTo.x());
        eng->setPreviousRankTo(humanTo.y());
    }


    const bool engineTurnNow = (m_ctx.gc() && (m_ctx.gc()->currentPlayer() == m_engineSide));
    qCDebug(lcGame).noquote() << "HvE engineTurnNow=" << engineTurnNow
                             << " engineSide=" << m_engineSide;
    if (!engineTurnNow) { if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    Usi* eng = m_ctx.primaryEngine();
    if (!eng || !m_ctx.gc() || !m_ctx.sfenHistory()) { if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    m_waitingForMove = true;
    m_ctx.requestHumanReply(eng, m_ctx.gc()->lastMove());
}

void HumanVsEngineStrategy::onEngineMoveReady(const UsiMove& move,
                                              const QString& position, const QString& ponder)
{
    Usi* eng = m_ctx.primaryEngine();
    if (!m_waitingForMove || sender() != eng || m_ctx.gameOverState().isOver || !m_ctx.gc()) return;
    m_waitingForMove = false;
    m_ctx.setEnginePosition(1, position, ponder);
    if (m_ctx.applyEngineMove(move, m_engineSide, eng->lastBestmoveElapsedMs(), true))
        armTurnTimerIfNeeded();
}

// ============================================================
// タイマー管理
// ============================================================

void HumanVsEngineStrategy::armTurnTimerIfNeeded()
{
    if (!m_humanTimerArmed) {
        m_humanTurnTimer.start();
        m_humanTimerArmed = true;
    }
}

void HumanVsEngineStrategy::finishTurnTimerAndSetConsideration(Turn /*mover*/)
{
    finishHumanTimerInternal();
}

void HumanVsEngineStrategy::finishHumanTimerInternal()
{
    // 考慮時間は ShogiClock が計測済みなので、ここでは人間用の計測タイマーを止めるだけ
    if (m_humanTurnTimer.isValid()) m_humanTurnTimer.invalidate();
    m_humanTimerArmed = false;
}

void HumanVsEngineStrategy::disarmTurnTimer()
{
    if (!m_humanTimerArmed) return;
    m_humanTimerArmed = false;
    m_humanTurnTimer.invalidate();
}

// ============================================================
// 初手がエンジン手番なら go を発行する
// ============================================================

void HumanVsEngineStrategy::onEngineInitialized()
{
    if (!m_initialMoveRequested || m_ctx.gameOverState().isOver) return;
    if (m_ctx.clock()) m_ctx.clock()->startClock();
    startInitialMoveIfNeeded();
}

void HumanVsEngineStrategy::startInitialMoveIfNeeded()
{
    if (m_ctx.gameOverState().isOver) return;
    if (m_waitingForMove) return;
    if (!m_ctx.gc()) return;

    // 人間の初手は、相手エンジンの初期化中も計時を続ける。
    if (m_ctx.gc()->currentPlayer() != m_engineSide) return;

    m_initialMoveRequested = true;
    if (m_ctx.primaryEngine() && m_ctx.primaryEngine()->isInitializing()) {
        if (m_ctx.clock()) m_ctx.clock()->stopClock();
        return;
    }
    startInitialEngineMove();
}

// （内部）エンジンの手番で初手を指す
void HumanVsEngineStrategy::startInitialEngineMove()
{
    Usi* eng = m_ctx.primaryEngine();
    if (!eng || !m_ctx.gc()) return;

    m_waitingForMove = true;
    m_ctx.requestEngineMove(eng, 1);
}
