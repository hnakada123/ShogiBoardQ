/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "playmode.h"

#include <QTimer>

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchCoordinator* coordinator,
                                                 const MatchCoordinator::StartOptions& opt,
                                                 QObject* parent)
    : QObject(parent)
    , m_coordinator(coordinator)
    , m_opt(opt)
{
}

// ============================================================
// EvE用SFEN/指し手コンテナへのアクセサ
// ============================================================

QStringList* EngineVsEngineStrategy::sfenRecordForEvE()
{
    auto* c = m_coordinator;
    return c->m_sfenHistory ? c->m_sfenHistory : &m_eveSfenRecord;
}

QVector<ShogiMove>& EngineVsEngineStrategy::gameMovesForEvE()
{
    auto* c = m_coordinator;
    return c->m_sfenHistory ? c->m_gameMoves : m_eveGameMoves;
}

// ============================================================
// 対局開始
// ============================================================

void EngineVsEngineStrategy::start()
{
    auto* c = m_coordinator;

    if (!c->m_usi1 || !c->m_usi2 || !c->m_gc) return;

    // EvE 用の内部棋譜コンテナを初期化
    m_eveSfenRecord.clear();
    m_eveGameMoves.clear();
    m_eveMoveIndex = 0;

    // EvE対局で初手からタイマーを動作させるため、ここで時計を開始する
    if (c->m_clock) {
        c->m_clock->startClock();
        qCDebug(lcGame) << "Clock started";
    }

    // 駒落ちの場合、SFENで手番が「w」（後手番）になっている
    // GCの currentPlayer() がその手番を持っているはず
    if (c->m_gc->currentPlayer() == ShogiGameController::NoPlayer) {
        // 平手なら先手から、駒落ちならSFENに従う
        c->m_gc->setCurrentPlayer(ShogiGameController::Player1);
    }
    c->m_cur = (c->m_gc->currentPlayer() == ShogiGameController::Player2)
                   ? MatchCoordinator::P2 : MatchCoordinator::P1;
    c->updateTurnDisplay(c->m_cur);

    initPositionStringsForEvE(m_opt.sfenStart);

    // 駒落ちの場合は後手（上手）から開始
    const bool isHandicap = (c->m_playMode == PlayMode::HandicapEngineVsEngine);
    const bool whiteToMove = (c->m_gc->currentPlayer() == ShogiGameController::Player2);

    if (isHandicap && whiteToMove) {
        startEvEFirstMoveByWhite();
    } else {
        startEvEFirstMoveByBlack();
    }
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
// EvE用position文字列の初期化
// ============================================================

void EngineVsEngineStrategy::initPositionStringsForEvE(const QString& sfenStart)
{
    auto* c = m_coordinator;

    c->m_positionStr1.clear();
    c->m_positionPonder1.clear();
    c->m_positionStr2.clear();
    c->m_positionPonder2.clear();

    // 平手の場合は startpos を使用、駒落ちの場合は sfen を使用
    static const QString kStartBoard =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    QString base;
    if (c->m_playMode == PlayMode::HandicapEngineVsEngine && !sfenStart.isEmpty()) {
        // SFENから盤面部分を抽出して平手かどうか判定
        QString checkSfen = sfenStart;
        if (checkSfen.startsWith(QLatin1String("position sfen "))) {
            checkSfen = checkSfen.mid(QStringLiteral("position sfen ").size()).trimmed();
        }
        const QStringList tok = checkSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const bool isStandardStart = (!tok.isEmpty() && tok[0] == kStartBoard);

        if (!isStandardStart) {
            // 駒落ち：position sfen <sfen> moves の形式を使用
            if (sfenStart.startsWith(QLatin1String("position "))) {
                base = sfenStart;
                // 末尾に " moves" がなければ追加
                if (!base.contains(QLatin1String(" moves"))) {
                    base += QStringLiteral(" moves");
                }
            } else {
                base = QStringLiteral("position sfen ") + sfenStart + QStringLiteral(" moves");
            }
        } else {
            base = QStringLiteral("position startpos moves");
        }
    } else {
        base = QStringLiteral("position startpos moves");
    }
    c->m_positionStr1 = base;
    c->m_positionStr2 = base;
}

// ============================================================
// 平手EvE：先手から開始
// ============================================================

void EngineVsEngineStrategy::startEvEFirstMoveByBlack()
{
    auto* c = m_coordinator;

    const MatchCoordinator::GoTimes t1 = c->computeGoTimes();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    c->m_gc->setPromote(false);

    c->m_usi1->handleEngineVsHumanOrEngineMatchCommunication(
        c->m_positionStr1, c->m_positionPonder1,
        p1From, p1To,
        static_cast<int>(t1.byoyomi),
        btimeStr1, wtimeStr1,
        static_cast<int>(t1.binc), static_cast<int>(t1.winc),
        (t1.byoyomi > 0)
        );

    QString rec1;

    // 先手1手目：次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!c->m_gc->validateAndMove(
            p1From, p1To, rec1,
            c->m_playMode,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (c->m_clock) {
        const qint64 thinkMs = c->m_usi1 ? c->m_usi1->lastBestmoveElapsedMs() : 0;
        c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        c->m_clock->applyByoyomiAndResetConsideration1();
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        c->m_hooks.appendKifuLine(rec1, c->m_clock->getPlayer1ConsiderationAndTotalTime());
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(p1From, p1To);
    c->updateTurnDisplay((c->m_gc->currentPlayer() == ShogiGameController::Player1)
                             ? MatchCoordinator::P1 : MatchCoordinator::P2);

    if (c->m_usi2) {
        c->m_usi2->setPreviousFileTo(p1To.x());
        c->m_usi2->setPreviousRankTo(p1To.y());
    }

    c->m_positionStr2     = c->m_positionStr1;
    c->m_positionPonder2.clear();

    const MatchCoordinator::GoTimes t2 = c->computeGoTimes();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    c->m_gc->setPromote(false);

    c->m_usi2->handleEngineVsHumanOrEngineMatchCommunication(
        c->m_positionStr2, c->m_positionPonder2,
        p2From, p2To,
        static_cast<int>(t2.byoyomi),
        btimeStr2, wtimeStr2,
        static_cast<int>(t2.binc), static_cast<int>(t2.winc),
        (t2.byoyomi > 0)
        );

    QString rec2;

    // 後手1手目：次の手を渡す
    nextEve = m_eveMoveIndex + 1;
    if (!c->m_gc->validateAndMove(
            p2From, p2To, rec2,
            c->m_playMode,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (c->m_clock) {
        const qint64 thinkMs = c->m_usi2 ? c->m_usi2->lastBestmoveElapsedMs() : 0;
        c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        c->m_clock->applyByoyomiAndResetConsideration2();
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        c->m_hooks.appendKifuLine(rec2, c->m_clock->getPlayer2ConsiderationAndTotalTime());
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(p2From, p2To);
    c->updateTurnDisplay((c->m_gc->currentPlayer() == ShogiGameController::Player1)
                             ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // P2の手をP1のポジション文字列に同期
    c->m_positionStr1 = c->m_positionStr2;

    QTimer::singleShot(std::chrono::milliseconds(0), this, &EngineVsEngineStrategy::kickNextEvETurn);
}

// ============================================================
// 駒落ちEvE：後手（上手）から開始
// ============================================================

void EngineVsEngineStrategy::startEvEFirstMoveByWhite()
{
    auto* c = m_coordinator;

    // 後手（上手 = m_usi2）が初手を指す
    const MatchCoordinator::GoTimes t2 = c->computeGoTimes();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    c->m_gc->setPromote(false);

    c->m_usi2->handleEngineVsHumanOrEngineMatchCommunication(
        c->m_positionStr2, c->m_positionPonder2,
        p2From, p2To,
        static_cast<int>(t2.byoyomi),
        btimeStr2, wtimeStr2,
        static_cast<int>(t2.binc), static_cast<int>(t2.winc),
        (t2.byoyomi > 0)
        );

    QString rec2;

    // 後手（上手）1手目
    int nextEve = m_eveMoveIndex + 1;
    if (!c->m_gc->validateAndMove(
            p2From, p2To, rec2,
            c->m_playMode,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (c->m_clock) {
        const qint64 thinkMs = c->m_usi2 ? c->m_usi2->lastBestmoveElapsedMs() : 0;
        c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        c->m_clock->applyByoyomiAndResetConsideration2();
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        c->m_hooks.appendKifuLine(rec2, c->m_clock->getPlayer2ConsiderationAndTotalTime());
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(p2From, p2To);
    c->updateTurnDisplay((c->m_gc->currentPlayer() == ShogiGameController::Player1)
                             ? MatchCoordinator::P1 : MatchCoordinator::P2);

    if (c->m_usi1) {
        c->m_usi1->setPreviousFileTo(p2To.x());
        c->m_usi1->setPreviousRankTo(p2To.y());
    }

    c->m_positionStr1     = c->m_positionStr2;
    c->m_positionPonder1.clear();

    // 先手（下手 = m_usi1）が2手目を指す
    const MatchCoordinator::GoTimes t1 = c->computeGoTimes();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    c->m_gc->setPromote(false);

    c->m_usi1->handleEngineVsHumanOrEngineMatchCommunication(
        c->m_positionStr1, c->m_positionPonder1,
        p1From, p1To,
        static_cast<int>(t1.byoyomi),
        btimeStr1, wtimeStr1,
        static_cast<int>(t1.binc), static_cast<int>(t1.winc),
        (t1.byoyomi > 0)
        );

    QString rec1;

    // 先手（下手）2手目
    nextEve = m_eveMoveIndex + 1;
    if (!c->m_gc->validateAndMove(
            p1From, p1To, rec1,
            c->m_playMode,
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (c->m_clock) {
        const qint64 thinkMs = c->m_usi1 ? c->m_usi1->lastBestmoveElapsedMs() : 0;
        c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        c->m_clock->applyByoyomiAndResetConsideration1();
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        c->m_hooks.appendKifuLine(rec1, c->m_clock->getPlayer1ConsiderationAndTotalTime());
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(p1From, p1To);
    c->updateTurnDisplay((c->m_gc->currentPlayer() == ShogiGameController::Player1)
                             ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // P1の手をP2のポジション文字列に同期
    c->m_positionStr2 = c->m_positionStr1;

    QTimer::singleShot(std::chrono::milliseconds(0), this, &EngineVsEngineStrategy::kickNextEvETurn);
}

// ============================================================
// EvE ターンループ
// ============================================================

void EngineVsEngineStrategy::kickNextEvETurn()
{
    auto* c = m_coordinator;

    if (c->m_playMode != PlayMode::EvenEngineVsEngine
        && c->m_playMode != PlayMode::HandicapEngineVsEngine)
        return;
    if (!c->m_usi1 || !c->m_usi2 || !c->m_gc) return;

    const bool p1ToMove = (c->m_gc->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? c->m_usi1 : c->m_usi2;
    Usi* receiver = p1ToMove ? c->m_usi2 : c->m_usi1;

    QString& pos    = p1ToMove ? c->m_positionStr1     : c->m_positionStr2;
    QString& ponder = p1ToMove ? c->m_positionPonder1  : c->m_positionPonder2;

    QPoint from(-1,-1), to(-1,-1);
    if (!c->engineThinkApplyMove(mover, pos, ponder, &from, &to))
        return;

    QString rec;

    // 次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!c->m_gc->validateAndMove(from, to, rec, c->m_playMode,
                                   nextEve, sfenRecordForEvE(), gameMovesForEvE())) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    // 相手側のポジション文字列を同期
    if (p1ToMove) {
        c->m_positionStr2 = c->m_positionStr1;
    } else {
        c->m_positionStr1 = c->m_positionStr2;
    }

    if (c->m_clock) {
        const qint64 thinkMs = mover ? mover->lastBestmoveElapsedMs() : 0;
        if (p1ToMove) {
            c->m_clock->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration1();
        } else {
            c->m_clock->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            c->m_clock->applyByoyomiAndResetConsideration2();
        }
    }
    if (c->m_hooks.appendKifuLine && c->m_clock) {
        const QString elapsed = p1ToMove
                                    ? c->m_clock->getPlayer1ConsiderationAndTotalTime()
                                    : c->m_clock->getPlayer2ConsiderationAndTotalTime();
        c->m_hooks.appendKifuLine(rec, elapsed);
    }

    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    if (c->m_hooks.renderBoardFromGc) c->m_hooks.renderBoardFromGc();
    if (c->m_hooks.showMoveHighlights) c->m_hooks.showMoveHighlights(from, to);
    c->updateTurnDisplay(
        (c->m_gc->currentPlayer() == ShogiGameController::Player1)
            ? MatchCoordinator::P1 : MatchCoordinator::P2
        );

    // 千日手チェック
    if (c->checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (c->m_maxMoves > 0 && m_eveMoveIndex >= c->m_maxMoves) {
        c->handleMaxMovesJishogi();
        return;
    }

    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn);
}
