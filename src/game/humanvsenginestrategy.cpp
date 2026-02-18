/// @file humanvsenginestrategy.cpp
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "humanvsenginestrategy.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchCoordinator* coordinator,
                                               bool engineIsP1,
                                               const QString& enginePath,
                                               const QString& engineName)
    : m_coordinator(coordinator)
    , m_engineIsP1(engineIsP1)
    , m_enginePath(enginePath)
    , m_engineName(engineName)
{
}

// ============================================================
// 対局開始
// ============================================================

void HumanVsEngineStrategy::start()
{
    auto* c = m_coordinator;

    if (c->m_hooks.log) {
        c->m_hooks.log(QStringLiteral("[Match] Start HvE (engineIsP1=%1)").arg(m_engineIsP1));
    }

    // 以前のエンジンは破棄（安全化）
    c->destroyEngines();

    // HvEでは思考モデルは常にエンジン1スロット（m_comm1/m_think1）を使用
    UsiCommLogModel*          comm  = c->m_comm1;
    ShogiEngineThinkingModel* think = c->m_think1;
    if (!comm)  { comm  = new UsiCommLogModel(c);          c->m_comm1  = comm;  }
    if (!think) { think = new ShogiEngineThinkingModel(c); c->m_think1 = think; }

    // 思考タブのエンジン名表示用（EngineAnalysisTab は log model を参照）
    const QString dispName = m_engineName.isEmpty() ? QStringLiteral("Engine") : m_engineName;
    if (comm) comm->setEngineName(dispName);

    // USI を生成（この時点ではプロセス未起動）
    // HvEでは常に m_usi1 を使用
    c->m_usi1 = new Usi(comm, think, c->m_gc, c->m_playMode, c);
    c->m_usi2 = nullptr;

    // 投了配線
    c->wireResignToArbiter(c->m_usi1, /*asP1=*/m_engineIsP1);
    // 入玉宣言勝ち配線
    c->wireWinToArbiter(c->m_usi1, /*asP1=*/m_engineIsP1);

    // ログ識別（UI 表示用）
    if (c->m_usi1) {
        c->m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), dispName);
        c->m_usi1->setSquelchResignLogging(false);
    }

    // USI エンジンを起動（path/name 必須）
    const MatchCoordinator::Player engineSide =
        m_engineIsP1 ? MatchCoordinator::P1 : MatchCoordinator::P2;
    c->initializeAndStartEngineFor(engineSide, m_enginePath, m_engineName);

    // --- 手番の単一ソースを確立：GC → m_cur → 表示 ---
    ShogiGameController::Player side =
        c->m_gc ? c->m_gc->currentPlayer() : ShogiGameController::NoPlayer;
    if (side == ShogiGameController::NoPlayer) {
        // 既定は先手（SFEN未指定時）
        side = ShogiGameController::Player1;
        if (c->m_gc) c->m_gc->setCurrentPlayer(side);
    }
    c->m_cur = (side == ShogiGameController::Player2) ? MatchCoordinator::P2 : MatchCoordinator::P1;

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    c->updateTurnDisplay(c->m_cur);
}

// ============================================================
// 人間の着手処理（3引数版：考慮時間確定→棋譜追記→エンジン返し手）
// ============================================================

void HumanVsEngineStrategy::onHumanMove(const QPoint& humanFrom,
                                          const QPoint& humanTo,
                                          const QString& prettyMove)
{
    auto* c = m_coordinator;

    const bool humanIsP1 =
        (c->m_playMode == PlayMode::EvenHumanVsEngine) || (c->m_playMode == PlayMode::HandicapHumanVsEngine);
    const ShogiGameController::Player engineSeat =
        humanIsP1 ? ShogiGameController::Player2 : ShogiGameController::Player1;

    // 0) 人間手のハイライト
    if (c->m_hooks.showMoveHighlights) {
        c->m_hooks.showMoveHighlights(humanFrom, humanTo);
    }

    // 1) 人間側の考慮時間を確定 → byoyomi/inc を適用 → KIF 追記
    if (c->m_clock) {
        if (humanIsP1) {
            const qint64 ms = c->m_clock->player1ConsiderationMs();
            c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(ms));
            c->m_clock->applyByoyomiAndResetConsideration1();

            if (c->m_hooks.appendKifuLine) {
                c->m_hooks.appendKifuLine(prettyMove, c->m_clock->getPlayer1ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            c->m_clock->setPlayer1ConsiderationTime(0);
        } else {
            const qint64 ms = c->m_clock->player2ConsiderationMs();
            c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(ms));
            c->m_clock->applyByoyomiAndResetConsideration2();

            if (c->m_hooks.appendKifuLine) {
                c->m_hooks.appendKifuLine(prettyMove, c->m_clock->getPlayer2ConsiderationAndTotalTime());
            }
            // 従来互換：クリア
            c->m_clock->setPlayer2ConsiderationTime(0);
        }

        // ラベルなど即時更新
        c->pokeTimeUpdateNow();
    }

    // 人間手の棋譜追記中にUI側の手番同期が割り込み、GC手番が人間側へ戻ることがある。
    // エンジン返し手の直前で、司令塔の期待手番（エンジン席）へ補正しておく。
    if (c->m_gc && c->m_gc->currentPlayer() != engineSeat) {
        qCDebug(lcGame).noquote()
            << "HvE(pretty): correcting GC turn before engine reply"
            << " current=" << static_cast<int>(c->m_gc->currentPlayer())
            << " expected(engineSeat)=" << static_cast<int>(engineSeat);
        c->m_gc->setCurrentPlayer(engineSeat);
    }

    // 「次の手番」ラベルをエンジン側に切り替える
    // （TurnSyncBridge 経由のシグナルは棋譜追記中の再入で確実に到達しない場合がある）
    c->updateTurnDisplay(humanIsP1 ? MatchCoordinator::P2 : MatchCoordinator::P1);

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
    auto* c = m_coordinator;

    auto extractMoveNumber = [](const QString& sfen) -> int {
        const QStringList tok = sfen.split(' ', Qt::SkipEmptyParts);
        // SFEN は <board> <turn> <hands> <move> の 4 トークン
        if (tok.size() >= 4) return tok.last().toInt();
        return -1;
    };

    // sfenRecordと手数インデックスを同期
    int mcCur = c->m_currentMoveIndex;
    if (c->m_sfenHistory) {
        const int fromRec = static_cast<int>(qMax(qsizetype(0), c->m_sfenHistory->size() - 1));
        if (fromRec != mcCur) {
            qCDebug(lcGame) << "HvE sync mcCur" << mcCur << "->" << fromRec
                            << "(by recSize=" << c->m_sfenHistory->size() << ")";
            mcCur = fromRec;
            c->m_currentMoveIndex = fromRec;
        }
    }

    const qsizetype recSizeBefore = c->m_sfenHistory ? c->m_sfenHistory->size() : -1;
    const QString recTailBefore = (c->m_sfenHistory && !c->m_sfenHistory->isEmpty()) ? c->m_sfenHistory->last() : QString();
    qCDebug(lcGame).noquote() << "HvE enter  mcCur=" << mcCur
                             << " recSizeBefore=" << recSizeBefore
                             << " recTailBefore='" << recTailBefore << "'"
                             << " humanFrom=" << humanFrom << " humanTo=" << humanTo;

    // 人間側のストップウォッチ締め＆考慮確定（既存）
    finishHumanTimerInternal();

    if (Usi* eng = c->primaryEngine()) {
        eng->setPreviousFileTo(humanTo.x());
        eng->setPreviousRankTo(humanTo.y());
    }

    // USIに渡す残り時間
    qint64 bMs = 0, wMs = 0;
    c->computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const ShogiGameController::Player engineSeat =
        m_engineIsP1 ? ShogiGameController::Player1 : ShogiGameController::Player2;

    // 千日手チェック（人間の手の後）
    if (c->checkAndHandleSennichite()) return;

    const bool engineTurnNow = (c->m_gc && (c->m_gc->currentPlayer() == engineSeat));
    qCDebug(lcGame).noquote() << "HvE engineTurnNow=" << engineTurnNow
                             << " engineSeat=" << int(engineSeat);
    if (!engineTurnNow) { if (!c->gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    Usi* eng = c->primaryEngine();
    if (!eng || !c->m_gc || !c->m_sfenHistory) { if (!c->gameOverState().isOver) armTurnTimerIfNeeded(); return; }

    if (c->m_positionStr1.isEmpty()) { c->initPositionStringsFromSfen(QString()); }
    if (!c->m_positionStr1.startsWith(QLatin1String("position "))) {
        c->m_positionStr1 = QStringLiteral("position startpos moves");
    }

    const auto tc = c->timeControl();
    const int byoyomiMs = m_engineIsP1 ? tc.byoyomiMs1 : tc.byoyomiMs2;

    QPoint eFrom = humanFrom, eTo = humanTo;
    c->m_gc->setPromote(false);

    eng->handleHumanVsEngineCommunication(
        c->m_positionStr1, c->m_positionPonder1,
        eFrom, eTo,
        byoyomiMs,
        bTime, wTime,
        c->m_positionStrHistory,
        tc.incMs1, tc.incMs2,
        tc.useByoyomi
        );

    QString rec;
    int nextIdx = mcCur + 1;
    const bool ok = c->m_gc->validateAndMove(
        eFrom, eTo, rec, c->m_playMode,
        nextIdx, c->m_sfenHistory, c->gameMovesRef());

    const QString recTailAfter = (c->m_sfenHistory && !c->m_sfenHistory->isEmpty()) ? c->m_sfenHistory->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qCDebug(lcGame).noquote() << "HvE v&m=" << ok
                             << " argMove(nextIdx)=" << nextIdx
                             << " mcCur(before sync calc)=" << mcCur
                             << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (ok) {
        c->m_currentMoveIndex = nextIdx;
        qCDebug(lcGame).noquote() << "HvE mcCur ->" << c->m_currentMoveIndex;
    } else {
        if (!c->gameOverState().isOver) armTurnTimerIfNeeded();
        return;
    }

    // 千日手チェック（エンジンの手の後）
    if (c->checkAndHandleSennichite()) return;

    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(eFrom, eTo);

    // エンジンの考慮時間を確定してから棋譜に追記する
    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    if (c->m_clock) {
        if (c->m_gc->currentPlayer() == ShogiGameController::Player1) {
            // 直前に指したのは後手(P2)
            c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration2();
        } else {
            // 直前に指したのは先手(P1)
            c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration1();
        }
    }

    if (c->m_hooks.appendKifuLine && c->m_clock) {
        const QString elapsed = (c->m_gc->currentPlayer() == ShogiGameController::Player1)
        ? c->m_clock->getPlayer2ConsiderationAndTotalTime()
        : c->m_clock->getPlayer1ConsiderationAndTotalTime();
        c->m_hooks.appendKifuLine(rec, elapsed);
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    c->m_cur = (c->m_gc->currentPlayer() == ShogiGameController::Player2) ? MatchCoordinator::P2 : MatchCoordinator::P1;
    c->updateTurnDisplay(c->m_cur);

    // エンジン着手後の評価値追加
    qCDebug(lcGame) << "onHumanMove: about to call appendEval, engineIsP1=" << m_engineIsP1;
    if (m_engineIsP1) {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP1, hook set=" << (c->m_hooks.appendEvalP1 ? "YES" : "NO");
        if (c->m_hooks.appendEvalP1) c->m_hooks.appendEvalP1();
    } else {
        qCDebug(lcGame) << "onHumanMove: calling appendEvalP2, hook set=" << (c->m_hooks.appendEvalP2 ? "YES" : "NO");
        if (c->m_hooks.appendEvalP2) c->m_hooks.appendEvalP2();
    }

    // 最大手数チェック
    if (c->m_maxMoves > 0 && c->m_currentMoveIndex >= c->m_maxMoves) {
        c->handleMaxMovesJishogi();
        return;
    }

    if (!c->gameOverState().isOver) armTurnTimerIfNeeded();
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
    auto* c = m_coordinator;

    // どちらが「人間側」かは Main からのフックで取得（HvE想定）
    if (!c->m_hooks.humanPlayerSide) return;
    const auto side = c->m_hooks.humanPlayerSide();

    // ShogiClock 内部の考慮msをそのまま反映
    if (c->m_clock) {
        const qint64 clkMs = (side == MatchCoordinator::P1) ? c->m_clock->player1ConsiderationMs()
                                                            : c->m_clock->player2ConsiderationMs();
        if (side == MatchCoordinator::P1) c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(clkMs));
        else                              c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(clkMs));
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

void HumanVsEngineStrategy::startInitialMoveIfNeeded()
{
    auto* c = m_coordinator;
    if (!c->m_gc) return;

    const auto sideToMove = c->m_gc->currentPlayer();

    if (m_engineIsP1 && sideToMove == ShogiGameController::Player1) {
        startInitialEngineMoveFor(static_cast<int>(MatchCoordinator::P1));
    } else if (!m_engineIsP1 && sideToMove == ShogiGameController::Player2) {
        startInitialEngineMoveFor(static_cast<int>(MatchCoordinator::P2));
    }
}

// （内部）指定したエンジン側で 1手だけ指す
void HumanVsEngineStrategy::startInitialEngineMoveFor(int engineSideInt)
{
    auto* c = m_coordinator;
    const auto engineSide = static_cast<MatchCoordinator::Player>(engineSideInt);

    Usi* eng = c->primaryEngine();
    if (!eng || !c->m_gc) return;

    auto extractMoveNumber = [](const QString& sfen) -> int {
        const QStringList tok = sfen.split(' ', Qt::SkipEmptyParts);
        if (tok.size() >= 5) return tok.last().toInt();
        return -1;
    };

    if (c->m_positionStr1.isEmpty()) {
        c->initPositionStringsFromSfen(QString()); // startpos moves
    }
    if (!c->m_positionStr1.startsWith(QLatin1String("position "))) {
        c->m_positionStr1 = QStringLiteral("position startpos moves");
    }

    const int mcCur = c->m_currentMoveIndex;
    const qsizetype recSizeBefore = c->m_sfenHistory ? c->m_sfenHistory->size() : -1;
    const QString recTailBefore = (c->m_sfenHistory && !c->m_sfenHistory->isEmpty()) ? c->m_sfenHistory->last() : QString();
    qCDebug(lcGame).noquote() << "HvE:init enter  mcCur=" << mcCur
                             << " recSizeBefore=" << recSizeBefore
                             << " recTailBefore='" << recTailBefore << "'";

    qint64 bMs = 0, wMs = 0;
    c->computeGoTimesForUSI(bMs, wMs);
    const QString bTime = QString::number(bMs);
    const QString wTime = QString::number(wMs);

    const auto tc = c->timeControl();
    const int  byoyomiMs = (engineSide == MatchCoordinator::P1) ? tc.byoyomiMs1 : tc.byoyomiMs2;

    QPoint eFrom(-1, -1), eTo(-1, -1);
    c->m_gc->setPromote(false);

    eng->handleEngineVsHumanOrEngineMatchCommunication(
        c->m_positionStr1,
        c->m_positionPonder1,
        eFrom, eTo,
        byoyomiMs,
        bTime, wTime,
        tc.incMs1, tc.incMs2,
        tc.useByoyomi
        );

    QString rec;
    int nextIdx = mcCur + 1;
    const bool ok = c->m_gc->validateAndMove(
        eFrom, eTo, rec, c->m_playMode,
        nextIdx, c->m_sfenHistory, c->gameMovesRef());

    const QString recTailAfter = (c->m_sfenHistory && !c->m_sfenHistory->isEmpty()) ? c->m_sfenHistory->last() : QString();
    const int recTailNum = recTailAfter.isEmpty() ? -1 : extractMoveNumber(recTailAfter);

    qCDebug(lcGame).noquote() << "HvE:init v&m=" << ok
                             << " nextIdx=" << nextIdx
                             << " recTailAfter='" << recTailAfter << "' num=" << recTailNum;

    if (!ok) return;

    // エンジン初手の手数インデックスを更新（同期漏れ防止）
    c->m_currentMoveIndex = nextIdx;

    const qint64 thinkMs = eng->lastBestmoveElapsedMs();
    if (c->m_clock) {
        if (engineSide == MatchCoordinator::P1) {
            c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration1();
        } else {
            c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration2();
        }
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        const QString elapsed = (engineSide == MatchCoordinator::P1)
        ? c->m_clock->getPlayer1ConsiderationAndTotalTime()
        : c->m_clock->getPlayer2ConsiderationAndTotalTime();
        c->m_hooks.appendKifuLine(rec, elapsed);
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    c->m_cur = (c->m_gc->currentPlayer() == ShogiGameController::Player2) ? MatchCoordinator::P2 : MatchCoordinator::P1;
    c->updateTurnDisplay(c->m_cur);

    armTurnTimerIfNeeded();

    qCDebug(lcGame) << "about to call appendEval, engineSide=" << (engineSide == MatchCoordinator::P1 ? "P1" : "P2");
    if (engineSide == MatchCoordinator::P1) {
        qCDebug(lcGame) << "calling appendEvalP1, hook set=" << (c->m_hooks.appendEvalP1 ? "YES" : "NO");
        if (c->m_hooks.appendEvalP1) c->m_hooks.appendEvalP1();
    } else {
        qCDebug(lcGame) << "calling appendEvalP2, hook set=" << (c->m_hooks.appendEvalP2 ? "YES" : "NO");
        if (c->m_hooks.appendEvalP2) c->m_hooks.appendEvalP2();
    }

    // 千日手チェック
    if (c->checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (c->m_maxMoves > 0 && nextIdx >= c->m_maxMoves) {
        c->handleMaxMovesJishogi();
        return;
    }
}
