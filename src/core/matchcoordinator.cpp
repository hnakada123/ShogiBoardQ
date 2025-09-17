#include "matchcoordinator.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include <QObject>
#include <QDebug>

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_clock(d.clock)
    , m_view(d.view)
    , m_usi1(d.usi1)
    , m_usi2(d.usi2)
    , m_hooks(d.hooks)
{
}

MatchCoordinator::~MatchCoordinator() = default;

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    m_usi1 = e1;
    m_usi2 = e2;
}

void MatchCoordinator::startNewGame(const QString& sfenStart) {
    // 1) GUI固有の初期化
    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(sfenStart);

    // 2) 表示/アクション
    setPlayersNamesForMode_();
    setEngineNamesBasedOnMode_();
    setGameInProgressActions_(true);

    // 3) 盤面描画
    renderShogiBoard_();

    // 4) 初手は P1（必要なら gc 側の現手番から同期）
    m_cur = P1;
    updateTurnDisplay_(m_cur);

    if (m_hooks.log) m_hooks.log(QStringLiteral("MatchCoordinator: startNewGame done"));
    emit gameStarted();
}

void MatchCoordinator::handleResign() {
    stopClockAndSendStops_();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = m_cur; // 現在手番側が投了したとみなす（必要に応じて呼び出し側で明示管理）

    // エンジンへ通知（任意）
    if (m_hooks.sendRawToEngine) {
        if (info.loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::handleEngineResign(int idx) {
    stopClockAndSendStops_();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (idx == 1 ? P1 : P2);

    if (m_hooks.sendRawToEngine) {
        if (info.loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::notifyTimeout(Player loser) {
    stopClockAndSendStops_();

    // 任意：エンジンへ gameover 通知（生コマンドで十分）
    if (m_hooks.sendRawToEngine) {
        if (loser == P1) {
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover lose"));
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover win"));
        } else {
            if (m_usi2) m_hooks.sendRawToEngine(m_usi2, QStringLiteral("gameover lose"));
            if (m_usi1) m_hooks.sendRawToEngine(m_usi1, QStringLiteral("gameover win"));
        }
    }

    GameEndInfo info;
    info.cause = Cause::Timeout;
    info.loser = loser;

    displayResultsAndUpdateGui_(info);
}

void MatchCoordinator::flipBoard() {
    // 実際の反転は GUI 側で実施（レイアウト/ラベル入替等を考慮）
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    emit boardFlipped(true);
}

void MatchCoordinator::onTurnFinishedAndSwitch() {
    // 1) 現在の時間を読み、次手の go を計算
    const GoTimes t = computeGoTimes_();

    // 2) 手番を入れ替え
    m_cur = (m_cur == P1 ? P2 : P1);
    updateTurnDisplay_(m_cur);

    // 3) 次に指す側のエンジンへ go 送信（対人/対エンジン/エンジンvsエンジンは上位で制御可）
    if (m_hooks.sendGoToEngine) {
        if (m_cur == P1 && m_usi1) m_hooks.sendGoToEngine(m_usi1, t);
        if (m_cur == P2 && m_usi2) m_hooks.sendGoToEngine(m_usi2, t);
    }
}

// ---- private helpers ----

void MatchCoordinator::setPlayersNamesForMode_() {
    if (m_hooks.setPlayersNames) {
        // TODO: 実際の名前決定ロジックを移設
        m_hooks.setPlayersNames(QStringLiteral("Player1"), QStringLiteral("Player2"));
    }
}

void MatchCoordinator::setEngineNamesBasedOnMode_() {
    if (m_hooks.setEngineNames) {
        // TODO: 実際のエンジン名取得ロジックを移設（Usi から取得など）
        m_hooks.setEngineNames(QStringLiteral("Engine#1"), QStringLiteral("Engine#2"));
    }
}

void MatchCoordinator::setGameInProgressActions_(bool inProgress) {
    if (m_hooks.setGameActions) m_hooks.setGameActions(inProgress);
}

void MatchCoordinator::renderShogiBoard_() {
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
}

void MatchCoordinator::updateTurnDisplay_(Player p) {
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(p);
}

MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes_() const {
    GoTimes t;
    if (m_hooks.remainingMsFor) {
        t.btime = m_hooks.remainingMsFor(P1);
        t.wtime = m_hooks.remainingMsFor(P2);
    }
    if (m_hooks.incrementMsFor) {
        t.binc = m_hooks.incrementMsFor(P1);
        t.winc = m_hooks.incrementMsFor(P2);
    }
    if (m_hooks.byoyomiMs) {
        t.byoyomi = m_hooks.byoyomiMs();
    }
    return t;
}

void MatchCoordinator::stopClockAndSendStops_() {
    if (m_clock) m_clock->stopClock();
    if (m_hooks.sendStopToEngine) {
        if (m_usi1) m_hooks.sendStopToEngine(m_usi1);
        if (m_usi2) m_hooks.sendStopToEngine(m_usi2);
    }
}

void MatchCoordinator::displayResultsAndUpdateGui_(const GameEndInfo& info) {
    setGameInProgressActions_(false);

    if (m_hooks.showGameOverDialog) {
        const QString loserStr = (info.loser == P1) ? QStringLiteral("P1") : QStringLiteral("P2");
        const QString msg = (info.cause == Cause::Timeout)
                                ? tr("Timeout: %1 lost").arg(loserStr)
                                : tr("Resignation: %1 lost").arg(loserStr);
        m_hooks.showGameOverDialog(tr("Game Over"), msg);
    }

    if (m_hooks.log) m_hooks.log(QStringLiteral("Game ended"));

    emit gameEnded(info);
    emit gameOverShown();
}

void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    Usi*& eng = (side == P1 ? m_usi1 : m_usi2);

    if (!eng) {
        // HvE フォールバック：m_usi1 しか無い場合は m_usi1 を使う
        if (m_usi1 && !m_usi2) {
            if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] fallback to m_usi1 for HvE"));
            eng = m_usi1;
        } else {
            if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] engine ptr is null (side=%1)").arg(side == P1 ? "P1" : "P2"));
            return;
        }
    }

    QString path = enginePathIn;
    QString name = engineNameIn;

    try {
        eng->initializeAndStartEngineCommunication(path, name);
    } catch (...) {
        if (m_hooks.log) m_hooks.log(QStringLiteral("[Match] init/start failed"));
        throw;
    }

    wireResignToArbiter_(eng, (eng == m_usi1)); // m_usi1=先手扱い（HvE でも OK）
}

void MatchCoordinator::wireResignSignals()
{
    if (m_usi1) wireResignToArbiter_(m_usi1, /*asP1=*/true);
    if (m_usi2) wireResignToArbiter_(m_usi2, /*asP1=*/false);
}

void MatchCoordinator::wireResignToArbiter_(Usi* engine, bool asP1)
{
    if (!engine) return;

    // 既存接続の掃除は「該当シグナルのみ」を明示して行う
    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, nullptr);

    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &MatchCoordinator::onEngine1Resign,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &MatchCoordinator::onEngine2Resign,
                         Qt::UniqueConnection);
    }
}

void MatchCoordinator::onEngine1Resign()
{
    // 既存のハンドラへ委譲（番号は 1 = P1）
    this->handleEngineResign(1);
}

void MatchCoordinator::onEngine2Resign()
{
    // 既存のハンドラへ委譲（番号は 2 = P2）
    this->handleEngineResign(2);
}

void MatchCoordinator::sendGameOverWinAndQuitTo(int idx)
{
    Usi* target = (idx == 1 ? m_usi1 : m_usi2);
    if (!target) return;

    // HvE のときは opponent が nullptr の可能性があるが、WIN 側だけ送れば良い。
    target->sendGameOverWinAndQuitCommands();
}

void MatchCoordinator::destroyEngine(int idx)
{
    Usi*& ref = (idx == 1 ? m_usi1 : m_usi2);
    if (ref) {
        delete ref;
        ref = nullptr;
    }
}

void MatchCoordinator::destroyEngines()
{
    destroyEngine(1);
    destroyEngine(2);
}

Usi* MatchCoordinator::enginePtr(int idx) const
{
    return (idx == 1 ? m_usi1 : (idx == 2 ? m_usi2 : nullptr));
}

int MatchCoordinator::indexForEngine_(const Usi* p) const
{
    if (!p) return 0;
    if (p == m_usi1) return 1;
    if (p == m_usi2) return 2;
    return 0;
}
