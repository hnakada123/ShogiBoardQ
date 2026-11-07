/// @file csagamecoordinator_setup.cpp
/// @brief CSA通信対局コーディネータのセットアップ系実装

#include "csagamecoordinator.h"

#include "matchengineseat.h"
#include "usi.h"
#include "csamoveconverter.h"
#include "csaverdictwait.h"
#include "logcategories.h"
#include "parsecommon.h"
#include "sfencsapositionconverter.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "gameboard.h"
#include "usi.h"
#include <QSignalBlocker>

bool CsaGameCoordinator::setupInitialPosition()
{
    if (!m_gameBoard || !m_gameBoard->board()) {
        return true;
    }

    const QSignalBlocker replayNotifications(m_gameBoard);
    const QString hiratePosition = SfenUtils::hirateSfen();

    QString parseError;
    auto startSfen = SfenCsaPositionConverter::fromCsaPositionLines(
        m_progress.summary.positionLines, &parseError);
    if (!startSfen || startSfen->isEmpty()) {
        qCWarning(lcNetwork).noquote()
            << "CSA position parse failed. fallback to hirate. error=" << parseError;
        startSfen = hiratePosition;
    }
    m_gameBoard->board()->setSfen(*startSfen);
    m_gameBoard->board()->setMoveNumber(1);

    CsaGameProgress& p = m_progress;
    p.record = RecordLine{};
    p.record.startSfen = m_gameBoard->board()->toSfen();

    for (const QString& move : std::as_const(p.summary.moves)) {
        const std::optional<RecordEntry> played = playCsaMove(move);
        if (!played) {
            qCWarning(lcNetwork).noquote()
                << "Failed to replay CSA move from Game_Summary:" << move;
            emit logMessage(tr("初期局面の指し手再生に失敗しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーから受信した初期局面データが不正です: %1").arg(move));
            setPhase(Phase::Failed);
            return false;
        }

        // 再開局面の指し手は Game_Summary の消費時間（,T）を付けて記録する
        RecordEntry entry = *played;
        entry.time = p.initialMoveTime(p.moveCount());
        p.record.entries.append(entry);
    }
    return true;
}

void CsaGameCoordinator::setupClock()
{
    // 再開局面（仕様 3.2.3）: 初期局面と規定の持時間から、Game_Summary の指し手が既に完了した状態にする
    m_progress.startClockFromSummary();
    m_turnTimer.start();

    if (!m_clock) {
        return;
    }

    const CsaGameSummary& s = m_progress.summary;
    startDisplayClock(m_clock, s, m_progress.blackRemainingMs, m_progress.whiteRemainingMs);
    // 終局の裁定はサーバーを正とし、表示用時計の時間切れで対局を終了しない。
    m_clock->setLoseOnTimeout(false);
    m_clock->setCurrentPlayer(s.toMove);
    m_clock->startClock();
}


// ============================================================
// 対局条件の確認とエンジン起動
// ============================================================

void CsaGameCoordinator::startEngine()
{
    if (!m_engineSeat) {
        m_engineSeat = new MatchEngineSeat(m_engines, this);
        connect(m_engineSeat, &MatchEngineSeat::initialized, this, &CsaGameCoordinator::onEngineInitialized);
        connect(m_engineSeat, &MatchEngineSeat::errorOccurred, this, &CsaGameCoordinator::onEngineError);
        connect(m_engineSeat, &MatchEngineSeat::resigned, this, &CsaGameCoordinator::onEngineResigned);
        connect(m_engineSeat, &MatchEngineSeat::declaredWin, this, &CsaGameCoordinator::onEngineDeclaredWin);
        connect(m_engineSeat, &MatchEngineSeat::moveReady, this, &CsaGameCoordinator::onEngineMoveReady);
    }
    // ダイアログで選んだエンジンのパス（ダイアログは登録一覧から選んだエンジンのパスを必ず渡す）
    MatchEngineSeat::Params params;
    params.enginePath = m_options.enginePath;
    if (params.enginePath.isEmpty()) {
        onEngineError(tr("エンジンパスが指定されていません"));
        return;
    }
    params.slot = 1;
    params.side = m_progress.isBlackSide ? Turn::Black : Turn::White;
    params.engineName = m_options.engineName;
    params.role = QStringLiteral("CSA");
    // 持ち時間はこの司令塔（MatchTimeSource）がサーバーの残り時間から返す
    params.timeSource = this;
    // USI初期化の待機中にもTCPのGame_Summaryが届く。readyokを待って合意する。
    m_engineSeat->start(params);
}

void CsaGameCoordinator::onGameSummaryReceived(const CsaGameSummary& summary)
{
    // 終局後も同じ接続で次の対局条件を受けるので、前局の記録はここで消す
    m_progress = CsaGameProgress::forSummary(summary);

    emit logMessage(tr("対局条件を受信しました"));
    emit logMessage(tr("先手: %1, 後手: %2").arg(summary.blackName, summary.whiteName));
    for (const CsaGameSummary::ConditionLine& line : summary.conditionLines()) emit logMessage(line.text, line.warning);

    // 拒否するとクライアントが対局待ちに戻る（仕様 3.3）
    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数を終えているので、対局条件を拒否します"), true);
        m_client->reject(summary.gameId);
        m_client->disconnectFromServer();
        return;
    }

    const QString rejectReason = summary.unsupportedReason(m_options.playerType == PlayerType::Engine);
    if (!rejectReason.isEmpty()) {
        emit logMessage(tr("対局条件を拒否します: %1").arg(rejectReason), true);
        m_client->reject(summary.gameId);
        return;
    }

    // 初期化中のエンジンは起動し直さず、readyok を待つ（ログインのときに起動を始めている）
    if (!isHumanPlayer() && (!m_engineSeat || (!m_engineSeat->isReady() && !m_engineSeat->isInitializing()))) {
        // 前局の終局で終えたエンジンを起動し直す
        startEngine();
        // 起動に失敗したら対局条件は拒否済み
        if (gameState() != GameState::WaitingForAgree) return;
    }

    emit logMessage(tr("対局条件に同意します..."));
    // エンジンの初期化中は readyok を待って同意する（onEngineInitialized）
    if (!m_engineSeat || !m_engineSeat->isInitializing()) m_client->agree(summary.gameId);
}

// ============================================================
// 対局数と再接続
// ============================================================

void CsaGameCoordinator::connectForGame()
{
    m_series->setLastEndedByServer(false);
    // 段階を先に変えておき、接続の状態の知らせ（接続中）で知らせる。すぐ失敗したときは Error になる
    m_phase = Phase::Active;
    emit logMessage(tr("サーバー %1:%2 に接続中...").arg(m_options.host).arg(m_options.port));
    m_client->connectToServer(m_options.host, m_options.port, m_options.transport);
    publishGameState();
}

void CsaGameCoordinator::handleGameCountAfterEnd()
{
    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数（%1局）を終えたのでログアウトします").arg(m_series->completedGames()));
        m_client->disconnectFromServer();
        return;
    }
    const int completed = m_series->completedGames();
    const QString progress = m_series->gameLimit() > 0 ? tr("%1/%2局").arg(completed).arg(m_series->gameLimit())
                                                       : tr("%1局目まで終了").arg(completed);
    if (m_client->isConnected()) {
        emit logMessage(tr("次の対局条件を待ちます（%1）").arg(progress));
    }
}

void CsaGameCoordinator::reconnectForNextGame()
{
    if (m_phase != Phase::Ended || !moreGamesWanted()) return;
    if (m_client->connectionState() != CsaClient::ConnectionState::Disconnected) return;
    emit logMessage(tr("次の対局のために再接続します（%1局目まで終了）").arg(m_series->completedGames()));
    connectForGame();
}

void CsaGameCoordinator::cleanup()
{
    m_verdict->clear();
    if (m_engineSeat) m_engineSeat->release();
    m_client->disconnectFromServer();
}

void CsaGameCoordinator::sendRawCommand(const QString& command)
{
    if (!m_client) {
        qCWarning(lcNetwork) << "sendRawCommand: client is null";
        return;
    }

    m_client->sendRawCommand(command);
}
