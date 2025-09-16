#include "matchcoordinator.h"
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
    displayResultsAndUpdateGui_(QStringLiteral("Human resigned"));
}

void MatchCoordinator::handleEngineResign(int idx) {
    stopClockAndSendStops_();
    displayResultsAndUpdateGui_(QStringLiteral("Engine %1 resigned").arg(idx));
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
    // 必要なら「残り0で増加0」等の調整をここに加える
    return t;
}

void MatchCoordinator::stopClockAndSendStops_() {
    if (m_hooks.sendStopToEngine) {
        if (m_usi1) m_hooks.sendStopToEngine(m_usi1);
        if (m_usi2) m_hooks.sendStopToEngine(m_usi2);
    }
}

void MatchCoordinator::displayResultsAndUpdateGui_(const QString& reason) {
    setGameInProgressActions_(false);
    if (m_hooks.showGameOverDialog) m_hooks.showGameOverDialog(tr("Game Over"), reason);
    if (m_hooks.log) m_hooks.log(QStringLiteral("Game ended: ") + reason);
    emit gameEnded();
    emit gameOverShown();
}
