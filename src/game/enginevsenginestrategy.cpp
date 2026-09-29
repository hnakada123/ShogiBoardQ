/// @file enginevsenginestrategy.cpp
/// @brief エンジン vs エンジンモードの Strategy 実装

#include "enginevsenginestrategy.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "usitimingparams.h"
#include "playmode.h"

#include <QTimer>

EngineVsEngineStrategy::EngineVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                                 MatchCoordinator::StartOptions opt,
                                                 QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
    , m_opt(std::move(opt))
{
}

// ============================================================
// EvE用SFEN/指し手コンテナへのアクセサ
// ============================================================

QStringList* EngineVsEngineStrategy::sfenRecordForEvE()
{
    return m_ctx.sfenHistory() ? m_ctx.sfenHistory() : &m_eveSfenRecord;
}

QList<ShogiMove>& EngineVsEngineStrategy::gameMovesForEvE()
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

    const bool p1ToMove = (m_ctx.gc()->currentPlayer() == ShogiGameController::Player1);
    Usi* mover    = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();

    QString& pos    = p1ToMove ? m_ctx.positionStr1()     : m_ctx.positionStr2();
    QString& ponder = p1ToMove ? m_ctx.positionPonder1()  : m_ctx.positionPonder2();

    const auto times = m_ctx.computeGoTimes();
    const UsiTimingParams timing{static_cast<int>(times.byoyomi), QString::number(times.btime),
                                 QString::number(times.wtime), static_cast<int>(times.binc),
                                 static_cast<int>(times.winc), times.byoyomi > 0};
    m_ctx.gc()->setPromote(false);
    m_waitingForMove = true;
    mover->requestMatchMove(pos, ponder, timing);
}

void EngineVsEngineStrategy::onEngineMoveReady(QPoint from, QPoint to,
                                               const QString& position, const QString& ponder)
{
    if (!m_waitingForMove || m_ctx.gameOverState().isOver || !m_ctx.gc()) return;
    const bool p1ToMove = m_ctx.gc()->currentPlayer() == ShogiGameController::Player1;
    Usi* mover = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();
    if (sender() != mover) return;
    m_waitingForMove = false;
    Usi* receiver = p1ToMove ? m_ctx.usi2() : m_ctx.usi1();
    (p1ToMove ? m_ctx.positionStr1() : m_ctx.positionStr2()) = position;
    (p1ToMove ? m_ctx.positionPonder1() : m_ctx.positionPonder2()) = ponder;
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
    if (m_ctx.hooks().game.appendKifuLine && m_ctx.clock()) {
        const QString elapsed = p1ToMove
                                    ? m_ctx.clock()->player1ConsiderationAndTotalTime()
                                    : m_ctx.clock()->player2ConsiderationAndTotalTime();
        m_ctx.hooks().game.appendKifuLine(rec, elapsed);
    }

    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    if (m_ctx.hooks().ui.renderBoardFromGc) m_ctx.hooks().ui.renderBoardFromGc();
    if (m_ctx.hooks().ui.showMoveHighlights) m_ctx.hooks().ui.showMoveHighlights(from, to);
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
