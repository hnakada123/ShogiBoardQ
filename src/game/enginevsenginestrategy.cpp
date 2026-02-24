/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "playmode.h"

#include <QTimer>

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                                 const MatchCoordinator::StartOptions& opt,
                                                 QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
    , m_opt(opt)
{
}

// ============================================================
// EvE用SFEN/指し手コンテナへのアクセサ
// ============================================================

QStringList* EngineVsEngineStrategy::sfenRecordForEvE()
{
    return m_ctx.sfenHistory() ? m_ctx.sfenHistory() : &m_eveSfenRecord;
}

QVector<ShogiMove>& EngineVsEngineStrategy::gameMovesForEvE()
{
    return m_ctx.sfenHistory() ? m_ctx.gameMovesDirect() : m_eveGameMoves;
}

// ============================================================
// 対局開始
// ============================================================

void EngineVsEngineStrategy::start()
{
    if (!m_ctx.usi1() || !m_ctx.usi2() || !m_ctx.gc()) return;

    // EvE 用の内部棋譜コンテナを初期化
    m_eveSfenRecord.clear();
    m_eveGameMoves.clear();
    m_eveMoveIndex = 0;

    // EvE対局で初手からタイマーを動作させるため、ここで時計を開始する
    if (m_ctx.clock()) {
        m_ctx.clock()->startClock();
        qCDebug(lcGame) << "Clock started";
    }

    // 駒落ちの場合、SFENで手番が「w」（後手番）になっている
    // GCの currentPlayer() がその手番を持っているはず
    if (m_ctx.gc()->currentPlayer() == ShogiGameController::NoPlayer) {
        // 平手なら先手から、駒落ちならSFENに従う
        m_ctx.gc()->setCurrentPlayer(ShogiGameController::Player1);
    }
    m_ctx.setCurrentTurn((m_ctx.gc()->currentPlayer() == ShogiGameController::Player2)
                             ? MatchCoordinator::P2 : MatchCoordinator::P1);
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());

    initPositionStringsForEvE(m_opt.sfenStart);

    // 駒落ちの場合は後手（上手）から開始
    const bool isHandicap = (m_ctx.playMode() == PlayMode::HandicapEngineVsEngine);
    const bool whiteToMove = (m_ctx.gc()->currentPlayer() == ShogiGameController::Player2);

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
    m_ctx.positionStr1().clear();
    m_ctx.positionPonder1().clear();
    m_ctx.positionStr2().clear();
    m_ctx.positionPonder2().clear();

    // 平手の場合は startpos を使用、駒落ちの場合は sfen を使用
    static const QString kStartBoard =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    QString base;
    if (m_ctx.playMode() == PlayMode::HandicapEngineVsEngine && !sfenStart.isEmpty()) {
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
    m_ctx.positionStr1() = base;
    m_ctx.positionStr2() = base;
}

// ============================================================
// 平手EvE：先手から開始
// ============================================================

void EngineVsEngineStrategy::startEvEFirstMoveByBlack()
{
    const MatchCoordinator::GoTimes t1 = m_ctx.computeGoTimes();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    m_ctx.gc()->setPromote(false);

    m_ctx.usi1()->handleEngineVsHumanOrEngineMatchCommunication(
        m_ctx.positionStr1(), m_ctx.positionPonder1(),
        p1From, p1To,
        static_cast<int>(t1.byoyomi),
        btimeStr1, wtimeStr1,
        static_cast<int>(t1.binc), static_cast<int>(t1.winc),
        (t1.byoyomi > 0)
        );

    QString rec1;

    // 先手1手目：次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->validateAndMove(
            p1From, p1To, rec1,
            m_ctx.playModeRef(),
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_ctx.clock()) {
        const qint64 thinkMs = m_ctx.usi1() ? m_ctx.usi1()->lastBestmoveElapsedMs() : 0;
        m_ctx.clock()->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        m_ctx.clock()->applyByoyomiAndResetConsideration1();
    }
    if (m_ctx.hooks().appendKifuLine && m_ctx.clock()) {
        m_ctx.hooks().appendKifuLine(rec1, m_ctx.clock()->getPlayer1ConsiderationAndTotalTime());
    }

    if (m_ctx.hooks().renderBoardFromGc) m_ctx.hooks().renderBoardFromGc();
    if (m_ctx.hooks().showMoveHighlights) m_ctx.hooks().showMoveHighlights(p1From, p1To);
    m_ctx.updateTurnDisplay((m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
                                ? MatchCoordinator::P1 : MatchCoordinator::P2);

    if (m_ctx.usi2()) {
        m_ctx.usi2()->setPreviousFileTo(p1To.x());
        m_ctx.usi2()->setPreviousRankTo(p1To.y());
    }

    m_ctx.positionStr2()     = m_ctx.positionStr1();
    m_ctx.positionPonder2().clear();

    const MatchCoordinator::GoTimes t2 = m_ctx.computeGoTimes();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    m_ctx.gc()->setPromote(false);

    m_ctx.usi2()->handleEngineVsHumanOrEngineMatchCommunication(
        m_ctx.positionStr2(), m_ctx.positionPonder2(),
        p2From, p2To,
        static_cast<int>(t2.byoyomi),
        btimeStr2, wtimeStr2,
        static_cast<int>(t2.binc), static_cast<int>(t2.winc),
        (t2.byoyomi > 0)
        );

    QString rec2;

    // 後手1手目：次の手を渡す
    nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->validateAndMove(
            p2From, p2To, rec2,
            m_ctx.playModeRef(),
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_ctx.clock()) {
        const qint64 thinkMs = m_ctx.usi2() ? m_ctx.usi2()->lastBestmoveElapsedMs() : 0;
        m_ctx.clock()->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        m_ctx.clock()->applyByoyomiAndResetConsideration2();
    }
    if (m_ctx.hooks().appendKifuLine && m_ctx.clock()) {
        m_ctx.hooks().appendKifuLine(rec2, m_ctx.clock()->getPlayer2ConsiderationAndTotalTime());
    }

    if (m_ctx.hooks().renderBoardFromGc) m_ctx.hooks().renderBoardFromGc();
    if (m_ctx.hooks().showMoveHighlights) m_ctx.hooks().showMoveHighlights(p2From, p2To);
    m_ctx.updateTurnDisplay((m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
                                ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // P2の手をP1のポジション文字列に同期
    m_ctx.positionStr1() = m_ctx.positionStr2();

    QTimer::singleShot(std::chrono::milliseconds(0), this, &EngineVsEngineStrategy::kickNextEvETurn);
}

// ============================================================
// 駒落ちEvE：後手（上手）から開始
// ============================================================

void EngineVsEngineStrategy::startEvEFirstMoveByWhite()
{
    // 後手（上手 = m_usi2）が初手を指す
    const MatchCoordinator::GoTimes t2 = m_ctx.computeGoTimes();
    const QString btimeStr2 = QString::number(t2.btime);
    const QString wtimeStr2 = QString::number(t2.wtime);

    QPoint p2From(-1, -1), p2To(-1, -1);
    m_ctx.gc()->setPromote(false);

    m_ctx.usi2()->handleEngineVsHumanOrEngineMatchCommunication(
        m_ctx.positionStr2(), m_ctx.positionPonder2(),
        p2From, p2To,
        static_cast<int>(t2.byoyomi),
        btimeStr2, wtimeStr2,
        static_cast<int>(t2.binc), static_cast<int>(t2.winc),
        (t2.byoyomi > 0)
        );

    QString rec2;

    // 後手（上手）1手目
    int nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->validateAndMove(
            p2From, p2To, rec2,
            m_ctx.playModeRef(),
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_ctx.clock()) {
        const qint64 thinkMs = m_ctx.usi2() ? m_ctx.usi2()->lastBestmoveElapsedMs() : 0;
        m_ctx.clock()->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
        m_ctx.clock()->applyByoyomiAndResetConsideration2();
    }
    if (m_ctx.hooks().appendKifuLine && m_ctx.clock()) {
        m_ctx.hooks().appendKifuLine(rec2, m_ctx.clock()->getPlayer2ConsiderationAndTotalTime());
    }

    if (m_ctx.hooks().renderBoardFromGc) m_ctx.hooks().renderBoardFromGc();
    if (m_ctx.hooks().showMoveHighlights) m_ctx.hooks().showMoveHighlights(p2From, p2To);
    m_ctx.updateTurnDisplay((m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
                                ? MatchCoordinator::P1 : MatchCoordinator::P2);

    if (m_ctx.usi1()) {
        m_ctx.usi1()->setPreviousFileTo(p2To.x());
        m_ctx.usi1()->setPreviousRankTo(p2To.y());
    }

    m_ctx.positionStr1()     = m_ctx.positionStr2();
    m_ctx.positionPonder1().clear();

    // 先手（下手 = m_usi1）が2手目を指す
    const MatchCoordinator::GoTimes t1 = m_ctx.computeGoTimes();
    const QString btimeStr1 = QString::number(t1.btime);
    const QString wtimeStr1 = QString::number(t1.wtime);

    QPoint p1From(-1, -1), p1To(-1, -1);
    m_ctx.gc()->setPromote(false);

    m_ctx.usi1()->handleEngineVsHumanOrEngineMatchCommunication(
        m_ctx.positionStr1(), m_ctx.positionPonder1(),
        p1From, p1To,
        static_cast<int>(t1.byoyomi),
        btimeStr1, wtimeStr1,
        static_cast<int>(t1.binc), static_cast<int>(t1.winc),
        (t1.byoyomi > 0)
        );

    QString rec1;

    // 先手（下手）2手目
    nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->validateAndMove(
            p1From, p1To, rec1,
            m_ctx.playModeRef(),
            nextEve,
            sfenRecordForEvE(),
            gameMovesForEvE()
            )) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    if (m_ctx.clock()) {
        const qint64 thinkMs = m_ctx.usi1() ? m_ctx.usi1()->lastBestmoveElapsedMs() : 0;
        m_ctx.clock()->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
        m_ctx.clock()->applyByoyomiAndResetConsideration1();
    }
    if (m_ctx.hooks().appendKifuLine && m_ctx.clock()) {
        m_ctx.hooks().appendKifuLine(rec1, m_ctx.clock()->getPlayer1ConsiderationAndTotalTime());
    }

    if (m_ctx.hooks().renderBoardFromGc) m_ctx.hooks().renderBoardFromGc();
    if (m_ctx.hooks().showMoveHighlights) m_ctx.hooks().showMoveHighlights(p1From, p1To);
    m_ctx.updateTurnDisplay((m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
                                ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // P1の手をP2のポジション文字列に同期
    m_ctx.positionStr2() = m_ctx.positionStr1();

    QTimer::singleShot(std::chrono::milliseconds(0), this, &EngineVsEngineStrategy::kickNextEvETurn);
}

// ============================================================
// EvE ターンループ
// ============================================================

void EngineVsEngineStrategy::kickNextEvETurn()
{
    if (m_ctx.playMode() != PlayMode::EvenEngineVsEngine
        && m_ctx.playMode() != PlayMode::HandicapEngineVsEngine)
        return;
    if (!m_ctx.usi1() || !m_ctx.usi2() || !m_ctx.gc()) return;

    const bool p1ToMove = (m_ctx.gc()->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();
    Usi* receiver = p1ToMove ? m_ctx.usi2() : m_ctx.usi1();

    QString& pos    = p1ToMove ? m_ctx.positionStr1()     : m_ctx.positionStr2();
    QString& ponder = p1ToMove ? m_ctx.positionPonder1()  : m_ctx.positionPonder2();

    QPoint from(-1,-1), to(-1,-1);
    if (!m_ctx.engineThinkApplyMove(mover, pos, ponder, &from, &to))
        return;

    QString rec;

    // 次の手を渡す
    int nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->validateAndMove(from, to, rec, m_ctx.playModeRef(),
                                   nextEve, sfenRecordForEvE(), gameMovesForEvE())) {
        return;
    } else {
        m_eveMoveIndex = nextEve;
    }

    // 相手側のポジション文字列を同期
    if (p1ToMove) {
        m_ctx.positionStr2() = m_ctx.positionStr1();
    } else {
        m_ctx.positionStr1() = m_ctx.positionStr2();
    }

    if (m_ctx.clock()) {
        const qint64 thinkMs = mover ? mover->lastBestmoveElapsedMs() : 0;
        if (p1ToMove) {
            m_ctx.clock()->setPlayer1ConsiderationTime(static_cast<int>(thinkMs));
            m_ctx.clock()->applyByoyomiAndResetConsideration1();
        } else {
            m_ctx.clock()->setPlayer2ConsiderationTime(static_cast<int>(thinkMs));
            m_ctx.clock()->applyByoyomiAndResetConsideration2();
        }
    }
    if (m_ctx.hooks().appendKifuLine && m_ctx.clock()) {
        const QString elapsed = p1ToMove
                                    ? m_ctx.clock()->getPlayer1ConsiderationAndTotalTime()
                                    : m_ctx.clock()->getPlayer2ConsiderationAndTotalTime();
        m_ctx.hooks().appendKifuLine(rec, elapsed);
    }

    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    if (m_ctx.hooks().renderBoardFromGc) m_ctx.hooks().renderBoardFromGc();
    if (m_ctx.hooks().showMoveHighlights) m_ctx.hooks().showMoveHighlights(from, to);
    m_ctx.updateTurnDisplay(
        (m_ctx.gc()->currentPlayer() == ShogiGameController::Player1)
            ? MatchCoordinator::P1 : MatchCoordinator::P2
        );

    // 千日手チェック
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック
    if (m_ctx.maxMoves() > 0 && m_eveMoveIndex >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn);
}
