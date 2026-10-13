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

// ============================================================
// EvE用SFEN/指し手コンテナへのアクセサ
// ============================================================

QStringList* EngineVsEngineStrategy::sfenRecordForEvE()
{
    return m_ctx.sfenHistory() ? m_ctx.sfenHistory() : &m_eveSfenRecord;
}

QList<ShogiMove>& EngineVsEngineStrategy::gameMovesForEvE()
{
    // 棋譜欄の表記変換・USI指し手列・ライブセッションが参照する共有リストへ記録する。
    return m_ctx.sfenHistory() ? m_ctx.gameMovesRef() : m_eveGameMoves;
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
    m_eveMoveIndex = m_opt.resumeFromBreakOff && m_ctx.sfenHistory()
        ? static_cast<int>(m_ctx.sfenHistory()->size() - 1) : 0;

    // EvE対局で初手からタイマーを動作させるため、ここで時計を開始する
    if (m_ctx.clock()) {
        m_ctx.clock()->startClock();
        qCDebug(lcGame) << "Clock started";
    }

    // 駒落ちの場合、SFENで手番が「w」（後手番）になっている。GC の手番は SFEN に従う
    m_ctx.setCurrentTurn((m_ctx.gc()->currentPlayer() == Turn::White)
                             ? MatchCoordinator::P2 : MatchCoordinator::P1);
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

    const bool p1ToMove = (m_ctx.gc()->currentPlayer() == Turn::Black);
    Usi* mover    = p1ToMove ? m_ctx.usi1() : m_ctx.usi2();

    QString& pos    = p1ToMove ? m_ctx.positionStr1()     : m_ctx.positionStr2();
    QString& ponder = p1ToMove ? m_ctx.positionPonder1()  : m_ctx.positionPonder2();

    // 思考時間はエンジンにつないだ対局時計から、思考を始める時点で求める
    const UsiTimingParams timing;
    m_waitingForMove = true;
    mover->requestMatchMove(pos, ponder, timing);
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
    (p1ToMove ? m_ctx.positionStr1() : m_ctx.positionStr2()) = position;
    (p1ToMove ? m_ctx.positionPonder1() : m_ctx.positionPonder2()) = ponder;
    QString rec;
    const Turn moverTurn = p1ToMove ? Turn::Black : Turn::White;
    const QPoint from = BoardCoordinates::fromSquare(move, moverTurn);
    const QPoint to = BoardCoordinates::toSquare(move);

    // 次の手を渡す。反則手は盤に指さず、指したエンジンの反則負けにする
    int nextEve = m_eveMoveIndex + 1;
    if (!m_ctx.gc()->applyMove(move, rec, nextEve, sfenRecordForEvE(), gameMovesForEvE())) {
        m_ctx.handleEngineIllegalMove(p1ToMove ? 1 : 2);
        return;
    }
    m_eveMoveIndex = nextEve;

    // 相手側のポジション文字列を同期
    if (p1ToMove) {
        m_ctx.positionStr2() = m_ctx.positionStr1();
    } else {
        m_ctx.positionStr1() = m_ctx.positionStr2();
    }

    const int moverSide = p1ToMove ? 1 : 2;
    if (m_ctx.clock()) {
        m_ctx.clock()->setMeasuredConsiderationTime(moverSide, mover ? mover->lastBestmoveElapsedMs() : 0);
        m_ctx.clock()->commitMove(moverSide);
    }
    if (m_ctx.hooks().game.appendKifuLine && m_ctx.clock()) {
        m_ctx.hooks().game.appendKifuLine(rec, m_ctx.clock()->considerationAndTotalTime(moverSide));
    }

    if (receiver) {
        receiver->setPreviousFileTo(to.x());
        receiver->setPreviousRankTo(to.y());
    }

    if (m_ctx.hooks().ui.renderBoardFromGc) m_ctx.hooks().ui.renderBoardFromGc();
    if (m_ctx.hooks().ui.showMoveHighlights) m_ctx.hooks().ui.showMoveHighlights(from, to);
    m_ctx.updateTurnDisplay(
        (m_ctx.gc()->currentPlayer() == Turn::Black)
            ? MatchCoordinator::P1 : MatchCoordinator::P2
        );

    // 千日手チェック
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック（人間を含む対局と同じく、棋譜全体の手数で判定する。
    // 途中局面から開始した場合も、開始からの手数ではなく総手数が上限に達したら止める）
    const QStringList* history = m_ctx.sfenHistory();
    const int plies = history ? static_cast<int>(history->size() - 1) : m_eveMoveIndex;
    if (m_ctx.maxMoves() > 0 && plies >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn);
}
