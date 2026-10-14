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
                                                 MatchCoordinator::StartOptions opt,
                                                 QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
    , m_opt(std::move(opt))
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

    if (!m_opt.resumeFromBreakOff) initPositionStringsForEvE(m_opt.sfenStart);

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
    m_ctx.setEnginePosition(1, base, {});
    m_ctx.setEnginePosition(2, base, {});
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
                                               const QString& position, const QString& ponder)
{
    if (!m_waitingForMove || m_ctx.gameOverState().isOver || !m_ctx.gc()) return;
    const bool p1ToMove = m_ctx.gc()->currentPlayer() == Turn::Black;
    Usi* mover = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();
    if (sender() != mover) return;
    m_waitingForMove = false;
    Usi* receiver = p1ToMove ? m_ctx.usi2() : m_ctx.usi1();
    m_ctx.setEnginePosition(p1ToMove ? 1 : 2, position, ponder);
    // 次の探索用の局面を両エンジンで共有する。
    m_ctx.syncOpponentPosition(p1ToMove ? 1 : 2, position);
    const QPoint to = BoardCoordinates::toSquare(move);
    if (receiver) {
        receiver->setPreviousMove(BoardSquare{to.x(), to.y()});
    }
    if (!m_ctx.applyEngineMove(move, p1ToMove ? Turn::Black : Turn::White,
                               mover->lastBestmoveElapsedMs(), false)) return;

    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn);
}
