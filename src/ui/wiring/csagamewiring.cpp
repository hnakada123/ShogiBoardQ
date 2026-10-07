/// @file csagamewiring.cpp
/// @brief CSA通信対局配線クラスの実装

#include "csagamewiring.h"
#include "uistatepolicymanager.h"
#include "gamerecordupdateservice.h"
#include "sfenpositiontracer.h"
#include "uinotificationservice.h"

#include "logcategories.h"
#include <QMessageBox>
#include <QStatusBar>
#include <QModelIndex>
#include <QTableView>
#include <QWidget>

#include "playmode.h"
#include "csagamecoordinator.h"
#include "csamoveconverter.h"
#include "parsecommon.h"
#include "csagamedialog.h"
#include "csawaitingdialog.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "kifurecordlistmodel.h"
#include "recordpane.h"
#include "boardinteractioncontroller.h"
#include "kifudisplay.h"
#include "engineanalysistab.h"
#include "boardsetupcontroller.h"
#include "timecontrolcontroller.h"

CsaGameWiring::CsaGameWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_coordinator(deps.coordinator)
    , m_gameController(deps.gameController)
    , m_shogiView(deps.shogiView)
    , m_kifuRecordModel(deps.kifuRecordModel)
    , m_recordPane(deps.recordPane)
    , m_boardController(deps.boardController)
    , m_statusBar(deps.statusBar)
    , m_sfenHistory(deps.sfenRecord)
    , m_analysisTab(deps.analysisTab)
    , m_boardSetupController(deps.boardSetupController)
    , m_usiCommLog(deps.usiCommLog)
    , m_engineThinking(deps.engineThinking)
    , m_timeController(deps.timeController)
    , m_gameMoves(deps.gameMoves)
    , m_playMode(deps.playMode)
    , m_parentWidget(deps.parentWidget)
    , m_prepareRecord(deps.prepareRecord)
    , m_syncPly(deps.syncPly)
    , m_recordGameEnd(deps.recordGameEnd)
{
}

void CsaGameWiring::setCoordinator(CsaGameCoordinator* coordinator)
{
    if (m_coordinator == coordinator) {
        return;
    }

    if (m_coordinator) {
        disconnect(m_coordinator, nullptr, this, nullptr);
    }

    m_coordinator = coordinator;
}

void CsaGameWiring::wire()
{
    if (!m_coordinator) {
        qCWarning(lcUi) << "wire: coordinator is null";
        return;
    }

    connect(m_coordinator, &CsaGameCoordinator::gameStarted,
            this, &CsaGameWiring::onGameStarted, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::gameEnded,
            this, &CsaGameWiring::onGameEnded, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::moveMade,
            this, &CsaGameWiring::onMoveMade, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::turnChanged,
            this, &CsaGameWiring::onTurnChanged, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::logMessage,
            this, &CsaGameWiring::onLogMessage, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::moveHighlightRequested,
            this, &CsaGameWiring::onMoveHighlightRequested, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::errorOccurred,
            this, &CsaGameWiring::errorMessageRequested, Qt::UniqueConnection);
    connect(m_coordinator, &CsaGameCoordinator::moveAppliedToBoard,
            this, &CsaGameWiring::moveAppliedToBoard, Qt::UniqueConnection);

    // PlayMode変更・対局終了ダイアログを内部で処理
    connect(this, &CsaGameWiring::playModeChanged,
            this, &CsaGameWiring::onPlayModeChangedInternal, Qt::UniqueConnection);
    connect(this, &CsaGameWiring::showGameEndDialogRequested,
            this, &CsaGameWiring::showGameEndDialogInternal, Qt::UniqueConnection);

    qCDebug(lcUi) << "wire: connected all signals";
}

void CsaGameWiring::unwire()
{
    if (!m_coordinator) return;

    disconnect(m_coordinator, nullptr, this, nullptr);
    qCDebug(lcUi) << "unwire: disconnected all signals";
}

void CsaGameWiring::onGameStarted(const QString& blackName, const QString& whiteName,
                                  const QStringList& initialPrettyMoves)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;

    // 対局情報と棋譜の書き出しに使う持ち時間と開始日時を、サーバーの対局条件で置き換える
    if (m_timeController && m_coordinator) {
        const CsaClient::GameSummary& summary = m_coordinator->gameSummary();
        m_timeController->beginGameWithTimeControl(summary.totalTimeMs(true), summary.byoyomiMs(true),
                                                   summary.incrementMs());
    }

    if (m_prepareRecord && m_sfenHistory && !m_sfenHistory->isEmpty()) {
        const QStringList positions = *m_sfenHistory;
        m_prepareRecord(positions.first(), blackName, whiteName);
        *m_sfenHistory = positions;
    }

    // ナビゲーション無効化を要求
    Q_EMIT disableNavigationRequested();

    // 将棋盤横のプレイヤー名ラベルを更新
    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(QStringLiteral("▲") + blackName);
        m_shogiView->setWhitePlayerName(QStringLiteral("▽") + whiteName);
    }

    // 棋譜モデルをクリア
    if (m_kifuRecordModel) {
        m_kifuRecordModel->clearAllItems();
        // 見出し行を追加
        m_kifuRecordModel->appendItem(
            new KifuDisplay(tr("=== 開始局面 ==="),
                            tr("（１手 / 合計）")));
    }

    for (qsizetype i = 0; i < initialPrettyMoves.size(); ++i) {
        const QString sfen = m_sfenHistory ? m_sfenHistory->value(i + 1) : QString();
        if (m_gameMoves && m_gameMoves->size() == i && m_coordinator && m_sfenHistory)
            m_gameMoves->append(SfenPositionTracer::buildGameMoves(
                m_sfenHistory->value(i), {m_coordinator->usiMoves().value(i)}));
        appendInitialKifuLine(initialPrettyMoves.at(i), sfen);
        if (m_kifuRecordModel && m_coordinator) {
            if (auto* item = m_kifuRecordModel->item(static_cast<int>(i + 1))) {
                item->beforeSfen = m_sfenHistory ? m_sfenHistory->value(i) : QString();
                item->usiMove = m_coordinator->usiMoves().value(i);
            }
        }
    }

    // 手数カウンタを、Game_Summary に含まれていた既存手順の末尾へ合わせる
    const int initialMoveCount = static_cast<int>(initialPrettyMoves.size());
    m_activePly = initialMoveCount;

    if (m_kifuRecordModel && m_kifuRecordModel->rowCount() > 0) {
        const int currentRow = m_kifuRecordModel->rowCount() - 1;
        m_activePly = currentRow;
        m_kifuRecordModel->setCurrentHighlightRow(currentRow);

        if (m_recordPane && m_recordPane->kifuView()) {
            const QModelIndex idx = m_kifuRecordModel->index(currentRow, 0);
            m_recordPane->kifuView()->setCurrentIndex(idx);
            m_recordPane->kifuView()->scrollTo(idx);
        }
    }

    if (m_shogiView) {
        m_shogiView->update();
    }
    if (m_syncPly) m_syncPly(m_activePly);

    // ステータスバーに表示
    if (m_statusBar) {
        m_statusBar->showMessage(
            tr("CSA通信対局開始: %1 vs %2").arg(blackName, whiteName), 5000);
    }
}

void CsaGameWiring::onGameEnded(CsaClient::GameResult result,
                                CsaClient::GameEndCause cause,
                                int consumedTimeMs)
{
    const QString resultText = CsaMoveConverter::gameResultToString(result);
    const QString causeText = CsaMoveConverter::gameEndCauseToString(cause);

    qCDebug(lcUi) << "onGameEnded:" << resultText << "(" << causeText << ")"
                  << "consumedTimeMs=" << consumedTimeMs;

    if (!m_coordinator) return;

    // 敗者の判定
    const bool iAmLoser = (result == CsaClient::GameResult::Lose);
    const bool isBlackSide = m_coordinator->isBlackSide();
    const bool loserIsBlack = (iAmLoser == isBlackSide);

    // 終局行テキストを生成（連続王手の千日手は終局した局面の手番で決まるので、最後の SFEN から手番を取る）
    const bool isDraw = (result == CsaClient::GameResult::Draw);
    const QString lastSfen = (m_sfenHistory && !m_sfenHistory->isEmpty()) ? m_sfenHistory->last() : QString();
    const bool blackToMove = lastSfen.isEmpty() ? (m_coordinator->isMyTurn() == isBlackSide)
                                                : (lastSfen.section(QLatin1Char(' '), 1, 1) != QLatin1String("w"));
    const QString endLine = buildEndLineText(cause, loserIsBlack, isDraw, blackToMove);

    // 消費時間は終局行を行った手番側（入玉宣言勝ちは宣言した勝者、連続王手の千日手は終局した局面の手番側）
    const bool moverIsBlack = (cause == CsaClient::GameEndCause::OuteSennichite) ? blackToMove
        : (cause == CsaClient::GameEndCause::Jishogi && !isDraw) ? !loserIsBlack : loserIsBlack;
    const int totalMs = moverIsBlack ? m_coordinator->blackTotalTimeMs()
                                     : m_coordinator->whiteTotalTimeMs();

    const QString elapsedStr = KifuParseCommon::formatTimeText(consumedTimeMs, totalMs + consumedTimeMs);

    // 棋譜欄に追加
    Q_EMIT appendKifuLineRequested(endLine, elapsedStr);
    if (m_recordGameEnd) m_recordGameEnd();

    // m_sfenHistoryにも終局行用のダミーエントリを追加
    if (m_sfenHistory && !m_sfenHistory->isEmpty()) {
        qCDebug(lcUi) << "onGameEnded: appending last SFEN to sfenRecord";
        m_sfenHistory->append(m_sfenHistory->last());
    }

    // 手数を更新
    if (m_kifuRecordModel) {
        const int currentRow = m_kifuRecordModel->rowCount() - 1;
        m_activePly = currentRow;

        // 棋譜欄で終局行を選択状態にする
        if (m_recordPane && m_recordPane->kifuView()) {
            QModelIndex idx = m_kifuRecordModel->index(currentRow, 0);
            m_recordPane->kifuView()->setCurrentIndex(idx);
            m_recordPane->kifuView()->scrollTo(idx);
        }
    }

    // 対局終了ダイアログを表示
    const QString message =
        tr("対局が終了しました。\n\n結果: %1\n原因: %2").arg(resultText, causeText);

    // プレイモードをリセット
    Q_EMIT playModeChanged(0);  // NotStarted

    // ナビゲーション有効化を要求
    Q_EMIT enableNavigationRequested();
    if (m_syncPly) m_syncPly(m_activePly);
    Q_EMIT showGameEndDialogRequested(tr("対局終了"), message);

    // ステータスバーに表示
    if (m_statusBar) {
        m_statusBar->showMessage(tr("対局終了: %1 (%2)").arg(resultText, causeText), 5000);
    }
}

void CsaGameWiring::onMoveMade(const QString& csaMove, const QString& usiMove,
                               const QString& prettyMove, int consumedTimeMs)
{
    qCDebug(lcUi) << "onMoveMade:" << prettyMove;

    if (!m_coordinator) return;

    // CSA形式から手番を判定
    const bool isBlackMove = (csaMove.length() > 0 && csaMove[0] == QLatin1Char('+'));

    // 累計消費時間を取得
    const int totalMs = isBlackMove ? m_coordinator->blackTotalTimeMs()
                                    : m_coordinator->whiteTotalTimeMs();
    const QString elapsedStr = KifuParseCommon::formatTimeText(consumedTimeMs, totalMs);

    // 棋譜欄に追記
    if (m_gameMoves && m_sfenHistory && m_sfenHistory->size() >= 2
        && m_gameMoves->size() + 1 == m_coordinator->usiMoves().size())
        m_gameMoves->append(SfenPositionTracer::buildGameMoves(
            m_sfenHistory->at(m_sfenHistory->size() - 2), {usiMove}));
    const int previousRows = m_kifuRecordModel ? m_kifuRecordModel->rowCount() : 0;
    Q_EMIT appendKifuLineRequested(prettyMove, elapsedStr);
    if (m_kifuRecordModel && m_kifuRecordModel->rowCount() > previousRows
        && m_sfenHistory && m_sfenHistory->size() >= 2) {
        if (auto* item = m_kifuRecordModel->item(m_kifuRecordModel->rowCount() - 1)) {
            item->beforeSfen = m_sfenHistory->at(m_sfenHistory->size() - 2);
            item->usiMove = usiMove;
        }
    }

    // 手数を更新
    if (m_kifuRecordModel) {
        const int currentRow = m_kifuRecordModel->rowCount() - 1;
        m_activePly = currentRow;
    }
    if (m_syncPly) m_syncPly(m_activePly);

    // 盤面を更新
    if (m_shogiView) {
        m_shogiView->update();
    }
}

void CsaGameWiring::onTurnChanged(bool isMyTurn)
{
    qCDebug(lcUi) << "onTurnChanged: myTurn =" << isMyTurn;

    if (!m_coordinator) return;

    // 手番表示の更新（ステータスバーで表示）
    const bool isBlackTurn = m_coordinator->isBlackSide() ? isMyTurn : !isMyTurn;
    const QString turnText = isBlackTurn ? tr("先手番") : tr("後手番");

    if (m_statusBar) {
        m_statusBar->showMessage(turnText, 2000);
    }
}

void CsaGameWiring::onLogMessage(const QString& message, bool isError)
{
    if (isError) {
        qCWarning(lcUi) << message;
    } else {
        qCDebug(lcUi) << message;
    }

    if (m_statusBar) {
        m_statusBar->showMessage(message, 3000);
    }
}

void CsaGameWiring::onMoveHighlightRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcUi) << "onMoveHighlightRequested: from=" << from << "to=" << to;

    if (m_boardController) {
        m_boardController->showMoveHighlights(from, to);
    }
}

QString CsaGameWiring::buildEndLineText(CsaClient::GameEndCause cause, bool loserIsBlack, bool isDraw, bool blackToMove) const
{
    const QString mark = loserIsBlack ? QStringLiteral("▲") : QStringLiteral("△");
    const QString winMark = loserIsBlack ? QStringLiteral("△") : QStringLiteral("▲");

    // Keep the record canonical; the presentation layer translates the terminal label.
    using Cause = CsaClient::GameEndCause;
    switch (cause) {
    case Cause::Resign: return mark + QStringLiteral("投了");
    case Cause::TimeUp: return mark + QStringLiteral("切れ負け");
    case Cause::IllegalMove:
    case Cause::IllegalAction: return mark + QStringLiteral("反則負け");
    case Cause::OuteSennichite: return KifuParseCommon::foulTerminalMove(loserIsBlack, blackToMove);
    case Cause::Sennichite: return QStringLiteral("千日手");
    // 入玉宣言は宣言した勝者の印を付ける（棋譜の保存時に勝者を判定するため）。点数による引き分けは持将棋
    case Cause::Jishogi: return isDraw ? QStringLiteral("持将棋") : winMark + QStringLiteral("入玉勝ち");
    case Cause::MaxMoves: return QStringLiteral("最大手数到達");
    default: return QStringLiteral("中断");
    }
}

void CsaGameWiring::onPlayModeChangedInternal(int mode)
{
    if (m_playMode) {
        *m_playMode = static_cast<PlayMode>(mode);
    }
    qCDebug(lcUi) << "onPlayModeChangedInternal: mode=" << mode;
}

void CsaGameWiring::showGameEndDialogInternal(const QString& title, const QString& message)
{
    QMessageBox::information(m_parentWidget, title, message);
}

void CsaGameWiring::appendInitialKifuLine(const QString& prettyMove, const QString& sfen)
{
    const QString trimmed = prettyMove.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const int beforeRows = m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1;
    if (m_recordService) {
        m_recordService->updateGameRecord(trimmed, QString(), sfen);
    }

    const bool appendedByService = m_kifuRecordModel && m_kifuRecordModel->rowCount() > beforeRows;
    if (!appendedByService && m_kifuRecordModel) {
        m_kifuRecordModel->appendItem(new KifuDisplay(buildNumberedKifuLine(trimmed), QString()));
    }
}

QString CsaGameWiring::buildNumberedKifuLine(const QString& prettyMove) const
{
    const int moveRows = m_kifuRecordModel ? qMax(0, m_kifuRecordModel->rowCount() - 1) : 0;

    const int nextMoveNumber = moveRows + 1;
    const QString moveNumberStr = QString::number(nextMoveNumber);
    const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
    return spaces + moveNumberStr + QLatin1Char(' ') + prettyMove;
}

void CsaGameWiring::onWaitingCancelled()
{
    qCDebug(lcUi) << "onWaitingCancelled: cancelled by user";

    // コーディネータの対局を停止
    if (m_coordinator) {
        m_coordinator->stopGame();
    }

    // プレイモードを未開始状態に戻す
    Q_EMIT playModeChanged(0);  // NotStarted

    // ステータスバーに通知
    if (m_statusBar) {
        m_statusBar->showMessage(tr("通信対局をキャンセルしました"), 3000);
    }
}

void CsaGameWiring::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

void CsaGameWiring::setBoardSetupController(BoardSetupController* controller)
{
    m_boardSetupController = controller;
}

bool CsaGameWiring::startCsaGame(CsaGameDialog* dialog, QWidget* parent)
{
    if (!dialog) {
        qCWarning(lcUi) << "startCsaGame: dialog is null";
        return false;
    }

    // CSA通信対局コーディネータが未作成の場合は作成する
    if (!m_coordinator) {
        m_coordinator = new CsaGameCoordinator(this);

        // 依存オブジェクトを設定
        CsaGameCoordinator::Dependencies deps;
        deps.gameController = m_gameController;
        deps.view = m_shogiView;
        deps.clock = m_timeController ? m_timeController->clock() : nullptr;
        deps.sfenRecord = m_sfenHistory;
        deps.gameMoves = m_gameMoves;
        deps.usiCommLog = m_usiCommLog;
        deps.engineThinking = m_engineThinking;
        m_coordinator->setDependencies(deps);

        // シグナル配線
        wire();

        // CSA通信ログをEngineAnalysisTabに転送
        if (m_analysisTab) {
            connect(m_coordinator, &CsaGameCoordinator::connectionStateChanged,
                    m_analysisTab, &EngineAnalysisTab::setCsaConnected,
                    Qt::UniqueConnection);
            connect(m_coordinator, &CsaGameCoordinator::csaCommLogAppended,
                    m_analysisTab, &EngineAnalysisTab::appendCsaLog,
                    Qt::UniqueConnection);
            // EngineAnalysisTabからのCSAコマンド送信シグナルを接続
            connect(m_analysisTab, &EngineAnalysisTab::csaRawCommandRequested,
                    m_coordinator, &CsaGameCoordinator::sendRawCommand,
                    Qt::UniqueConnection);
        }

        // BoardSetupControllerからの指し手をCsaGameCoordinatorに転送
        if (m_boardSetupController) {
            connect(m_boardSetupController, &BoardSetupController::csaMoveRequested,
                    m_coordinator, &CsaGameCoordinator::onHumanMove,
                    Qt::UniqueConnection);
        }
    }

    // 対局開始オプションを設定
    CsaGameCoordinator::StartOptions options;
    options.host = dialog->host();
    options.port = dialog->port();
    options.username = dialog->loginId();
    options.password = dialog->password();

    if (dialog->isHuman()) {
        options.playerType = CsaGameCoordinator::PlayerType::Human;
    } else {
        options.playerType = CsaGameCoordinator::PlayerType::Engine;
        options.engineName = dialog->engineName();
        options.engineNumber = dialog->engineNumber();
        options.enginePath = dialog->engineList().value(dialog->engineNumber()).path;
    }

    // プレイモード変更を通知
    Q_EMIT playModeChanged(static_cast<int>(PlayMode::CsaNetworkMode));

    // 待機ダイアログを作成
    CsaWaitingDialog waitingDialog(m_coordinator, parent);

    // キャンセル要求時の処理を接続
    connect(&waitingDialog, &CsaWaitingDialog::cancelRequested,
            this, &CsaGameWiring::onWaitingCancelled);

    // 対局を開始（シグナルがCsaWaitingDialogに届くようになった後に開始）
    m_coordinator->startGame(options);

    // 待機ダイアログを表示（対局開始またはエラーまでブロック）
    const int result = waitingDialog.exec();
    const bool started =
        (result == QDialog::Accepted
         && m_coordinator
         && m_coordinator->gameState() == CsaGameCoordinator::GameState::InGame);
    if (!started) {
        Q_EMIT playModeChanged(static_cast<int>(PlayMode::NotStarted));
        if (m_coordinator
            && m_coordinator->gameState() != CsaGameCoordinator::GameState::Idle) {
            m_coordinator->stopGame();
        }
    }

    return started;
}

void CsaGameWiring::wireExternalSignals(UiStatePolicyManager* uiPolicy,
                                          GameRecordUpdateService* recordService,
                                          UiNotificationService* notifService)
{
    m_recordService = recordService;

    if (uiPolicy) {
        connect(this, &CsaGameWiring::disableNavigationRequested,
                uiPolicy, &UiStatePolicyManager::transitionToDuringCsaGame,
                Qt::UniqueConnection);
        connect(this, &CsaGameWiring::enableNavigationRequested,
                uiPolicy, &UiStatePolicyManager::transitionToIdle,
                Qt::UniqueConnection);
    }
    if (recordService) {
        connect(this, &CsaGameWiring::appendKifuLineRequested,
                recordService, &GameRecordUpdateService::appendKifuLine,
                Qt::UniqueConnection);
    }
    if (notifService) {
        connect(this, &CsaGameWiring::errorMessageRequested,
                notifService, &UiNotificationService::displayErrorMessage,
                Qt::UniqueConnection);
    }
}
