/// @file humanvsenginestrategy.cpp
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "humanvsenginestrategy.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "usitimingparams.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "logcategories.h"

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                               bool engineIsP1,
                                               QString enginePath,
                                               QString engineName)
    : QObject(ctx.coordinatorAsParent())
    , m_ctx(ctx)
    , m_engineIsP1(engineIsP1)
    , m_enginePath(std::move(enginePath))
    , m_engineName(std::move(engineName))
{
}

// ============================================================
// 対局開始
// ============================================================

void HumanVsEngineStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvE (engineIsP1=" << m_engineIsP1 << ")";

    // 以前のエンジンは破棄（安全化）
    m_ctx.destroyEngines();

    // HvEでは思考モデルは常にエンジン1スロット（m_comm1/m_think1）を使用
    UsiCommLogModel*          comm  = m_ctx.comm1();
    ShogiEngineThinkingModel* think = m_ctx.think1();
    if (!comm)  { comm  = new UsiCommLogModel(m_ctx.coordinatorAsParent());          m_ctx.setComm1(comm);  }
    if (!think) { think = new ShogiEngineThinkingModel(m_ctx.coordinatorAsParent()); m_ctx.setThink1(think); }

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName = m_engineName.isEmpty() ? QStringLiteral("Engine") : m_engineName;
    if (comm) comm->setEngineName(dispName);

    // USI を生成（この時点ではプロセス未起動）
    // HvEでは常に m_usi1 を使用
    // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks) - Qt parent-child 所有権により管理される
    m_ctx.setUsi1(new Usi(comm, think, m_ctx.gc(), m_ctx.coordinatorAsParent()));
    m_ctx.setUsi2(nullptr); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    // 投了配線
    m_ctx.wireResignToArbiter(m_ctx.usi1(), /*asP1=*/m_engineIsP1);
    // 入玉宣言勝ち配線
    m_ctx.wireWinToArbiter(m_ctx.usi1(), /*asP1=*/m_engineIsP1);

    // ログ識別（UI 表示用）
    if (m_ctx.usi1()) {
        m_ctx.usi1()->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), dispName);
        m_ctx.usi1()->setSquelchResignLogging(false);
    }

    connect(m_ctx.usi1(), &Usi::engineInitialized, this, &HumanVsEngineStrategy::onEngineInitialized);
    connect(m_ctx.usi1(), &Usi::matchMoveReady, this, &HumanVsEngineStrategy::onEngineMoveReady);

    // USI エンジンを起動（path/name 必須）
    const MatchCoordinator::Player engineSide =
        m_engineIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2;
    m_ctx.initializeAndStartEngineFor(engineSide, m_enginePath, m_engineName);

    // --- 手番の単一ソースを確立：GC → m_cur → 表示 ---
    ShogiGameController::Player side =
        m_ctx.gc() ? m_ctx.gc()->currentPlayer() : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        // 既定は先手（SFEN未指定時）
        side = ShogiGameController::Player1;
        if (m_ctx.gc()) m_ctx.gc()->setCurrentPlayer(side);
    }
    m_ctx.setCurrentTurn((side == ShogiGameController::Player2) ? MatchCoordinator::P2 : MatchCoordinator::P1);

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
    const bool humanIsP1 =
        (m_ctx.playMode() == PlayMode::EvenHumanVsEngine) || (m_ctx.playMode() == PlayMode::HandicapHumanVsEngine);

    // 0) 人間手のハイライト
    if (m_ctx.hooks().ui.showMoveHighlights) {
        m_ctx.hooks().ui.showMoveHighlights(humanFrom, humanTo);
    }

    // 1) 人間側の考慮時間を確定 → byoyomi/inc を適用 → KIF 追記
    if (m_ctx.clock()) {
        if (humanIsP1) {
            const qint64 ms = m_ctx.clock()->player1ConsiderationMs();
            m_ctx.clock()->setPlayer1ConsiderationTime(static_cast<int>(ms));
            m_ctx.clock()->applyByoyomiAndResetConsideration1();

            if (m_ctx.hooks().game.appendKifuLine) {
                m_ctx.hooks().game.appendKifuLine(prettyMove, m_ctx.clock()->player1ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            m_ctx.clock()->setPlayer1ConsiderationTime(0);
        } else {
            const qint64 ms = m_ctx.clock()->player2ConsiderationMs();
            m_ctx.clock()->setPlayer2ConsiderationTime(static_cast<int>(ms));
            m_ctx.clock()->applyByoyomiAndResetConsideration2();

            if (m_ctx.hooks().game.appendKifuLine) {
                m_ctx.hooks().game.appendKifuLine(prettyMove, m_ctx.clock()->player2ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            m_ctx.clock()->setPlayer2ConsiderationTime(0);
        }

        // ラベルなど即時更新
        m_ctx.pokeTimeUpdateNow();
    }

    // 「次の手番」ラベルをエンジン側に切り替える
    // （TurnSyncBridge 経由のシグナルは棋譜追記中の再入で確実に到達しない場合がある）
    m_ctx.updateTurnDisplay(humanIsP1 ? MatchCoordinator::P2 : MatchCoordinator::P1);

    // 2) 以降（エンジン go → bestmove → 盤/棋譜反映）は2引数版に委譲
    //    finishHumanTimerInternal() は2引数版の先頭で呼ばれるが、二重でも実害が出ない想定。
    onHumanMoveEngineReply(humanFrom, humanTo);
}

// ============================================================
// エンジン返し手（2引数版：エンジン go→bestmove→盤反映）
// ============================================================

void HumanVsEngineStrategy::onHumanMoveEngineReply(const QPoint& humanFrom,
                                                     const QPoint& humanTo)
{
    // sfenRecordと手数インデックスを同期
    int mcCur = m_ctx.currentMoveIndex();
    if (m_ctx.sfenHistory()) {
        const int fromRec = static_cast<int>(qMax(qsizetype(0), m_ctx.sfenHistory()->size() - 1));
        if (fromRec != mcCur) {
            qCDebug(lcGame) << "HvE sync mcCur" << mcCur << "->" << fromRec
                            << "(by recSize=" << m_ctx.sfenHistory()->size() << ")";
            mcCur = fromRec;
            m_ctx.setCurrentMoveIndex(fromRec);
        }
    }

    const qsizetype recSizeBefore = m_ctx.sfenHistory() ? m_ctx.sfenHistory()->size() : -1;
    const QString recTailBefore = (m_ctx.sfenHistory() && !m_ctx.sfenHistory()->isEmpty()) ? m_ctx.sfenHistory()->last() : QString();
    qCDebug(lcGame).noquote() << "HvE enter  mcCur=" << mcCur
                             << " recSizeBefore=" << recSizeBefore
                             << " recTailBefore='" << recTailBefore << "'"
                             << " humanFrom=" << humanFrom << " humanTo=" << humanTo;

    // 人間側のストップウォッチ締め＆考慮確定（既存）
    finishHumanTimerInternal();

    if (Usi* eng = m_ctx.primaryEngine()) {
        eng->setPreviousFileTo(humanTo.x());
        eng->setPreviousRankTo(humanTo.y());
    }

    // USIに渡す残り時間
    qint64 bMs = 0, wMs = 0;
    m_ctx.computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const ShogiGameController::Player engineSeat =
        m_engineIsP1 ? ShogiGameController::Player1 : ShogiGameController::Player2;

    // 千日手チェック（人間の手の後）
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック（人間の手の後）。ここで止めないとエンジンが上限を超えて1手指す
    if (m_ctx.maxMoves() > 0 && m_ctx.currentMoveIndex() >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    const bool engineTurnNow = (m_ctx.gc() && (m_ctx.gc()->currentPlayer() == engineSeat));
    qCDebug(lcGame).noquote() << "HvE engineTurnNow=" << engineTurnNow
                             << " engineSeat=" << int(engineSeat);
    if (!engineTurnNow) { if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    Usi* eng = m_ctx.primaryEngine();
    if (!eng || !m_ctx.gc() || !m_ctx.sfenHistory()) { if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    if (m_ctx.positionStr1().isEmpty()) { m_ctx.initPositionStringsFromSfen(QString()); }
    if (!m_ctx.positionStr1().startsWith(QLatin1String("position "))) {
        m_ctx.positionStr1() = QStringLiteral("position startpos moves");
    }

    const auto tc = m_ctx.timeControl();
    const int byoyomiMs = m_engineIsP1 ? tc.byoyomiMs1 : tc.byoyomiMs2;

    const UsiTimingParams timing{byoyomiMs, bTime, wTime, tc.incMs1, tc.incMs2, tc.useByoyomi};
    m_waitingForMove = true;
    eng->requestHumanReply(m_ctx.positionStr1(), m_ctx.positionPonder1(), humanFrom, humanTo,
                           timing, m_ctx.positionStrHistory());
}

void HumanVsEngineStrategy::onEngineMoveReady(QPoint eFrom, QPoint eTo,
                                              const QString& position, const QString& ponder)
{
    Usi* eng = m_ctx.primaryEngine();
    if (!m_waitingForMove || sender() != eng || m_ctx.gameOverState().isOver || !m_ctx.gc()) return;
    m_waitingForMove = false;
    m_ctx.positionStr1() = position;
    m_ctx.positionPonder1() = ponder;
    const int mcCur = m_ctx.currentMoveIndex();
    QString rec;
    int nextIdx = mcCur + 1;
    const bool ok = m_ctx.gc()->validateAndMove(
        eFrom, eTo, rec, m_ctx.playModeRef(),
        nextIdx, m_ctx.sfenHistory(), m_ctx.gameMovesRef());

    if (ok) {
        m_ctx.setCurrentMoveIndex(nextIdx);
        qCDebug(lcGame).noquote() << "HvE mcCur ->" << m_ctx.currentMoveIndex();
    } else {
        if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded();
        return;
    }

    if (m_ctx.hooks().ui.showMoveHighlights) m_ctx.hooks().ui.showMoveHighlights(eFrom, eTo);

    // エンジンの考慮時間を確定してから棋譜に追記する
    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    if (m_ctx.clock()) {
        if (m_ctx.gc()->currentPlayer() == ShogiGameController::Player1) {
            // 直前に指したのは後手(P2)
            m_ctx.clock()->setMeasuredConsiderationTime(2, thinkMs);
            m_ctx.clock()->applyByoyomiAndResetConsideration2();
        } else {
            // 直前に指したのは先手(P1)
            m_ctx.clock()->setMeasuredConsiderationTime(1, thinkMs);
            m_ctx.clock()->applyByoyomiAndResetConsideration1();
        }
    }

    if (m_ctx.hooks().game.appendKifuLine && m_ctx.clock()) {
        const QString elapsed = (m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
        ? m_ctx.clock()->player2ConsiderationAndTotalTime()
        : m_ctx.clock()->player1ConsiderationAndTotalTime();
        m_ctx.hooks().game.appendKifuLine(rec, elapsed);
    }

    if (m_ctx.hooks().ui.renderBoardFromGc) m_ctx.hooks().ui.renderBoardFromGc();
    m_ctx.setCurrentTurn((m_ctx.gc()->currentPlayer() == ShogiGameController::Player2) ? MatchCoordinator::P2 : MatchCoordinator::P1);
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());

    // エンジン着手後の評価値追加
    qCDebug(lcGame) << "onHumanMove: about to call appendEval, engineIsP1=" << m_engineIsP1;
    if (m_engineIsP1) {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP1, hook set=" << (m_ctx.hooks().game.appendEvalP1 ? "YES" : "NO");
        if (m_ctx.hooks().game.appendEvalP1) m_ctx.hooks().game.appendEvalP1();
    } else {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP2, hook set=" << (m_ctx.hooks().game.appendEvalP2 ? "YES" : "NO");
        if (m_ctx.hooks().game.appendEvalP2) m_ctx.hooks().game.appendEvalP2();
    }

    // 千日手チェック（エンジンの手の後）。手を棋譜と盤面に反映してから判定し、終局行をその後に記録する
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (m_ctx.maxMoves() > 0 && m_ctx.currentMoveIndex() >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    if (!m_ctx.gameOverState().isOver) armTurnTimerIfNeeded();
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

void HumanVsEngineStrategy::finishTurnTimerAndSetConsideration(int /*moverPlayer*/)
{
    finishHumanTimerInternal();
}

void HumanVsEngineStrategy::finishHumanTimerInternal()
{
    const auto side = m_engineIsP1 ? MatchCoordinator::P2 : MatchCoordinator::P1;

    // ShogiClock 内部の考慮msをそのまま反映
    if (m_ctx.clock()) {
        const qint64 clkMs = (side == MatchCoordinator::P1) ? m_ctx.clock()->player1ConsiderationMs()
                                                            : m_ctx.clock()->player2ConsiderationMs();
        if (side == MatchCoordinator::P1) m_ctx.clock()->setPlayer1ConsiderationTime(static_cast<int>(clkMs));
        else                              m_ctx.clock()->setPlayer2ConsiderationTime(static_cast<int>(clkMs));
    }
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

    const auto sideToMove = m_ctx.gc()->currentPlayer();
    const auto engineSide = m_engineIsP1 ? ShogiGameController::Player1
                                        : ShogiGameController::Player2;
    // 人間の初手は、相手エンジンの初期化中も計時を続ける。
    if (sideToMove != engineSide) return;

    m_initialMoveRequested = true;
    if (m_ctx.primaryEngine() && m_ctx.primaryEngine()->isInitializing()) {
        if (m_ctx.clock()) m_ctx.clock()->stopClock();
        return;
    }
    if (m_engineIsP1 && sideToMove == ShogiGameController::Player1) {
        startInitialEngineMoveFor(static_cast<int>(MatchCoordinator::P1));
    } else if (!m_engineIsP1 && sideToMove == ShogiGameController::Player2) {
        startInitialEngineMoveFor(static_cast<int>(MatchCoordinator::P2));
    }
}

// （内部）指定したエンジン側で 1手だけ指す
void HumanVsEngineStrategy::startInitialEngineMoveFor(int engineSideInt)
{
    const auto engineSide = static_cast<MatchCoordinator::Player>(engineSideInt);

    Usi* eng = m_ctx.primaryEngine();
    if (!eng || !m_ctx.gc()) return;

    if (m_ctx.positionStr1().isEmpty()) {
        m_ctx.initPositionStringsFromSfen(QString()); // startpos moves
    }
    if (!m_ctx.positionStr1().startsWith(QLatin1String("position "))) {
        m_ctx.positionStr1() = QStringLiteral("position startpos moves");
    }

    qint64 bMs = 0, wMs = 0;
    m_ctx.computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const auto tc = m_ctx.timeControl();
    const int  byoyomiMs = (engineSide == MatchCoordinator::P1) ? tc.byoyomiMs1 : tc.byoyomiMs2;

    m_ctx.gc()->setPromote(false);
    const UsiTimingParams timing{byoyomiMs, bTime, wTime, tc.incMs1, tc.incMs2, tc.useByoyomi};
    m_waitingForMove = true;
    eng->requestMatchMove(m_ctx.positionStr1(), m_ctx.positionPonder1(), timing);
}
