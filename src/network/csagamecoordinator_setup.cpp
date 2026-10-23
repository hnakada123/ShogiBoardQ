/// @file csagamecoordinator_setup.cpp
/// @brief CSA通信対局コーディネータのセットアップ系実装

#include "csagamecoordinator.h"

#include "matchengineseat.h"
#include "usi.h"
#include "enginelistsettings.h"
#include "csamoveconverter.h"
#include "logcategories.h"
#include "parsecommon.h"
#include "sfencsapositionconverter.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include <QSignalBlocker>

void CsaGameCoordinator::setGameState(GameState state)
{
    if (m_gameState != state) {
        m_gameState = state;
        emit gameStateChanged(state);
    }
}

void CsaGameCoordinator::setupInitialPosition()
{
    if (!m_gameController || !m_gameController->board()) {
        return;
    }

    const QSignalBlocker replayNotifications(m_gameController);
    const QString hiratePosition = SfenUtils::hirateSfen();

    QString parseError;
    auto startSfen = SfenCsaPositionConverter::fromCsaPositionLines(
        m_progress.summary.positionLines, &parseError);
    if (!startSfen || startSfen->isEmpty()) {
        qCWarning(lcNetwork).noquote()
            << "CSA position parse failed. fallback to hirate. error=" << parseError;
        startSfen = hiratePosition;
    }
    m_gameController->board()->setSfen(*startSfen);
    m_gameController->board()->setMoveNumber(1);

    CsaGameProgress& p = m_progress;
    p.record = RecordLine{};
    p.record.startSfen = m_gameController->board()->toSfen();

    for (const QString& move : std::as_const(p.summary.moves)) {
        const std::optional<RecordEntry> played = playCsaMove(move);
        if (!played) {
            qCWarning(lcNetwork).noquote()
                << "Failed to replay CSA move from Game_Summary:" << move;
            emit logMessage(tr("初期局面の指し手再生に失敗しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーから受信した初期局面データが不正です: %1").arg(move));
            setGameState(GameState::Error);
            return;
        }

        // 再開局面の指し手は Game_Summary の消費時間（,T）を付けて記録する
        RecordEntry entry = *played;
        entry.time = p.initialMoveTime(p.moveCount());
        p.record.entries.append(entry);
    }
}

void CsaGameCoordinator::setupClock()
{
    // 再開局面（仕様 3.2.3）: 初期局面と規定の持時間から、Game_Summary の指し手が既に完了した状態にする
    m_progress.startClockFromSummary();
    m_turnSide = m_progress.summary.toMove;
    m_turnTimer.start();

    if (!m_clock) {
        return;
    }

    const CsaClient::GameSummary& s = m_progress.summary;
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
    // ダイアログで選んだエンジンのパスを優先し、無ければ名前で登録一覧から引く
    // （番号は登録一覧が変わるとずれるため使わない）
    MatchEngineSeat::Params params;
    params.enginePath = m_options.enginePath.isEmpty() ? EngineListSettings::pathForName(m_options.engineName)
                                                       : m_options.enginePath;
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

QString CsaGameCoordinator::unsupportedConditionReason(const CsaClient::GameSummary& summary,
                                                      PlayerType playerType)
{
    // Format は Position の書式と指し手表記を決める。将棋以外は解釈できない
    if (!summary.format.isEmpty() && !summary.format.startsWith(QStringLiteral("Shogi"))) {
        return tr("未対応の Format です: %1").arg(summary.format);
    }
    // 時間無制限ではエンジンに思考時間を与えられない（ローカル対局と同じ方針）
    if (playerType == PlayerType::Engine && summary.isTimeUnlimited()) {
        return tr("時間無制限の対局はエンジンでは行えません");
    }
    return QString();
}

void CsaGameCoordinator::logGameConditions(const CsaClient::GameSummary& summary)
{
    if (summary.isTimeUnlimited()) {
        emit logMessage(tr("持時間: 無制限"));
    } else {
        for (const bool black : {true, false}) {
            if (!black && !summary.hasIndividualTime) break;
            const CsaClient::TimeControl& t = summary.timeFor(black);
            QString text = tr("持時間: %1秒, 秒読み: %2秒, 加算: %3秒")
                               .arg(t.totalTimeMs() / 1000)
                               .arg(t.byoyomiMs() / 1000)
                               .arg(t.incrementMs() / 1000);
            if (t.delay > 0) text += tr(", 遅延: %1秒").arg(t.delayMs() / 1000);
            if (t.leastTimePerMove > 0) text += tr(", 最少消費: %1秒").arg(t.leastTimePerMoveMs() / 1000);
            if (summary.hasIndividualTime) {
                text = (black ? tr("先手の") : tr("後手の")) + text;
            }
            emit logMessage(text);
        }
    }
    if (summary.maxMoves > 0) {
        emit logMessage(tr("最大手数: %1手").arg(summary.maxMoves));
    }
    emit logMessage(summary.allowsJishogiDeclaration()
                        ? tr("入玉宣言ルール: %1").arg(summary.declaration)
                        : tr("入玉宣言ルール: なし"));
    if (!summary.protocolVersion.isEmpty() && !summary.protocolVersion.startsWith(QStringLiteral("1."))) {
        emit logMessage(tr("未知のプロトコルバージョンです: %1").arg(summary.protocolVersion), true);
    }
    if (summary.protocolMode != QStringLiteral("Server")) {
        emit logMessage(tr("Protocol_Mode が Server ではありません: %1").arg(summary.protocolMode), true);
    }
}

void CsaGameCoordinator::onGameSummaryReceived(const CsaClient::GameSummary& summary)
{
    // 終局後も同じ接続で次の対局条件を受けるので、前局の記録はここで消す
    m_progress = CsaGameProgress::forSummary(summary);

    emit logMessage(tr("対局条件を受信しました"));
    emit logMessage(tr("先手: %1, 後手: %2").arg(summary.blackName, summary.whiteName));
    logGameConditions(summary);

    setGameState(GameState::WaitingForAgree);

    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数を終えているので、対局条件を拒否します"), true);
        m_client->reject(summary.gameId);
        setGameState(GameState::WaitingForMatch);
        m_client->disconnectFromServer();
        return;
    }

    const QString rejectReason = unsupportedConditionReason(summary, m_playerType);
    if (!rejectReason.isEmpty()) {
        emit logMessage(tr("対局条件を拒否します: %1").arg(rejectReason), true);
        m_client->reject(summary.gameId);
        setGameState(GameState::WaitingForMatch);
        return;
    }

    if (m_playerType == PlayerType::Engine && (!m_engineSeat || !m_engineSeat->isReady())) {
        // 初めての対局か、前局の終局で終えたエンジンを起動し直す
        startEngine();
        // 起動に失敗したら対局条件は拒否済み
        if (m_gameState != GameState::WaitingForAgree) return;
    }

    emit logMessage(tr("対局条件に同意します..."));
    // エンジンの初期化中は readyok を待って同意する（onEngineInitialized）
    if (!m_engineSeat || !m_engineSeat->isInitializing()) m_client->agree(summary.gameId);
}

// ============================================================
// 対局数と再接続
// ============================================================

bool CsaGameCoordinator::moreGamesWanted() const
{
    return m_options.gameLimit <= 0 || m_completedGames < m_options.gameLimit;
}

void CsaGameCoordinator::connectForGame()
{
    m_lastGameEndedByServer = false;
    setGameState(GameState::Connecting);
    emit logMessage(tr("サーバー %1:%2 に接続中...").arg(m_options.host).arg(m_options.port));
    m_client->connectToServer(m_options.host, m_options.port, m_options.transport);
}

void CsaGameCoordinator::handleGameCountAfterEnd()
{
    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数（%1局）を終えたのでログアウトします").arg(m_completedGames));
        m_client->disconnectFromServer();
        return;
    }
    const QString progress = m_options.gameLimit > 0
        ? tr("%1/%2局").arg(m_completedGames).arg(m_options.gameLimit)
        : tr("%1局目まで終了").arg(m_completedGames);
    if (m_client->isConnected()) {
        emit logMessage(tr("次の対局条件を待ちます（%1）").arg(progress));
    }
}

void CsaGameCoordinator::reconnectForNextGame()
{
    if (m_gameState != GameState::GameOver || !moreGamesWanted()) return;
    if (m_client->connectionState() != CsaClient::ConnectionState::Disconnected) return;
    emit logMessage(tr("次の対局のために再接続します（%1局目まで終了）").arg(m_completedGames));
    connectForGame();
}

void CsaGameCoordinator::cleanup()
{
    clearPendingVerdict();
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
