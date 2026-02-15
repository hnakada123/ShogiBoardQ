/// @file csagamewiring.cpp
/// @brief CSA通信対局配線クラスの実装

#include "csagamewiring.h"

#include "loggingcategory.h"
#include <QStatusBar>
#include <QModelIndex>
#include <QTableView>
#include <QWidget>

#include "playmode.h"
#include "csagamecoordinator.h"
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
{
}

void CsaGameWiring::setCoordinator(CsaGameCoordinator* coordinator)
{
    if (m_coordinator) {
        unwire();
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
            this, &CsaGameWiring::onGameStarted);
    connect(m_coordinator, &CsaGameCoordinator::gameEnded,
            this, &CsaGameWiring::onGameEnded);
    connect(m_coordinator, &CsaGameCoordinator::moveMade,
            this, &CsaGameWiring::onMoveMade);
    connect(m_coordinator, &CsaGameCoordinator::turnChanged,
            this, &CsaGameWiring::onTurnChanged);
    connect(m_coordinator, &CsaGameCoordinator::logMessage,
            this, &CsaGameWiring::onLogMessage);
    connect(m_coordinator, &CsaGameCoordinator::moveHighlightRequested,
            this, &CsaGameWiring::onMoveHighlightRequested);
    connect(m_coordinator, &CsaGameCoordinator::errorOccurred,
            this, &CsaGameWiring::errorMessageRequested);

    qCDebug(lcUi) << "wire: connected all signals";
}

void CsaGameWiring::unwire()
{
    if (!m_coordinator) return;

    disconnect(m_coordinator, nullptr, this, nullptr);
    qCDebug(lcUi) << "unwire: disconnected all signals";
}

void CsaGameWiring::onGameStarted(const QString& blackName, const QString& whiteName)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;

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

    // m_sfenHistoryをクリアし、開始局面を追加
    const QString hiratePosition = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    if (m_sfenHistory) {
        m_sfenHistory->clear();
        m_sfenHistory->append(hiratePosition);
    }

    // 手数カウンタをリセット
    m_activePly = 0;
    m_currentSelectedPly = 0;

    // 盤面を初期化
    if (m_gameController && m_gameController->board()) {
        m_gameController->board()->setSfen(hiratePosition);
    }
    if (m_shogiView) {
        m_shogiView->update();
    }

    // ステータスバーに表示
    if (m_statusBar) {
        m_statusBar->showMessage(
            tr("CSA通信対局開始: %1 vs %2").arg(blackName, whiteName), 5000);
    }
}

void CsaGameWiring::onGameEnded(const QString& result, const QString& cause, int consumedTimeMs)
{
    qCDebug(lcUi) << "onGameEnded:" << result << "(" << cause << ")"
                  << "consumedTimeMs=" << consumedTimeMs;

    if (!m_coordinator) return;

    // 敗者の判定
    const bool iAmLoser = (result == tr("負け"));
    const bool isBlackSide = m_coordinator->isBlackSide();
    const bool loserIsBlack = (iAmLoser == isBlackSide);

    // デバッグ: 敗者判定の詳細を出力
    qCDebug(lcUi) << "onGameEnded judgment:"
                   << "iAmLoser=" << iAmLoser
                   << "isBlackSide=" << isBlackSide
                   << "loserIsBlack=" << loserIsBlack;

    // 終局行テキストを生成
    const QString endLine = buildEndLineText(cause, loserIsBlack);

    qCDebug(lcUi) << "onGameEnded endLine=" << endLine;

    // 消費時間をフォーマット
    const int totalMs = loserIsBlack ? m_coordinator->blackTotalTimeMs()
                                     : m_coordinator->whiteTotalTimeMs();

    // デバッグ: 消費時間の計算詳細を出力
    qCDebug(lcUi) << "Time calculation:"
                   << "blackTotalTimeMs=" << m_coordinator->blackTotalTimeMs()
                   << "whiteTotalTimeMs=" << m_coordinator->whiteTotalTimeMs()
                   << "totalMs(for loser)=" << totalMs
                   << "consumedTimeMs=" << consumedTimeMs
                   << "totalMs+consumedTimeMs=" << (totalMs + consumedTimeMs);

    const QString elapsedStr = formatElapsedTime(consumedTimeMs, totalMs + consumedTimeMs);
    qCDebug(lcUi) << "elapsedStr=" << elapsedStr;

    // 棋譜欄に追加
    Q_EMIT appendKifuLineRequested(endLine, elapsedStr);

    // m_sfenHistoryにも終局行用のダミーエントリを追加
    if (m_sfenHistory && !m_sfenHistory->isEmpty()) {
        qCDebug(lcUi) << "onGameEnded: appending last SFEN to sfenRecord";
        m_sfenHistory->append(m_sfenHistory->last());
    }

    // 分岐ツリー更新を要求
    Q_EMIT refreshBranchTreeRequested();

    // 手数を更新
    if (m_kifuRecordModel) {
        const int currentRow = m_kifuRecordModel->rowCount() - 1;
        m_activePly = currentRow;
        m_currentSelectedPly = currentRow;

        // 棋譜欄で終局行を選択状態にする
        if (m_recordPane && m_recordPane->kifuView()) {
            QModelIndex idx = m_kifuRecordModel->index(currentRow, 0);
            m_recordPane->kifuView()->setCurrentIndex(idx);
            m_recordPane->kifuView()->scrollTo(idx);
        }
    }

    // 対局終了ダイアログを表示
    const QString message = tr("対局が終了しました。\n\n結果: %1\n原因: %2").arg(result, cause);
    Q_EMIT showGameEndDialogRequested(tr("対局終了"), message);

    // プレイモードをリセット
    Q_EMIT playModeChanged(0);  // NotStarted

    // ナビゲーション有効化を要求
    Q_EMIT enableNavigationRequested();

    // ステータスバーに表示
    if (m_statusBar) {
        m_statusBar->showMessage(tr("対局終了: %1 (%2)").arg(result, cause), 5000);
    }
}

void CsaGameWiring::onMoveMade(const QString& csaMove, const QString& usiMove,
                               const QString& prettyMove, int consumedTimeMs)
{
    Q_UNUSED(usiMove)

    qCDebug(lcUi) << "onMoveMade:" << prettyMove;

    if (!m_coordinator) return;

    // CSA形式から手番を判定
    const bool isBlackMove = (csaMove.length() > 0 && csaMove[0] == QLatin1Char('+'));

    // 累計消費時間を取得
    const int totalMs = isBlackMove ? m_coordinator->blackTotalTimeMs()
                                    : m_coordinator->whiteTotalTimeMs();
    const QString elapsedStr = formatElapsedTime(consumedTimeMs, totalMs);

    // 棋譜欄に追記
    Q_EMIT appendKifuLineRequested(prettyMove, elapsedStr);

    // 分岐ツリー更新を要求
    Q_EMIT refreshBranchTreeRequested();

    // 手数を更新
    if (m_kifuRecordModel) {
        const int currentRow = m_kifuRecordModel->rowCount() - 1;
        m_activePly = currentRow;
        m_currentSelectedPly = currentRow;
    }

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

QString CsaGameWiring::formatElapsedTime(int consumedTimeMs, int totalTimeMs) const
{
    const int consumedSec = consumedTimeMs / 1000;
    const int consumedMin = consumedSec / 60;
    const int consumedSecRem = consumedSec % 60;

    const int totalSec = totalTimeMs / 1000;
    const int totalHour = totalSec / 3600;
    const int totalMin = (totalSec % 3600) / 60;
    const int totalSecRem = totalSec % 60;

    return QString("%1:%2/%3:%4:%5")
        .arg(consumedMin, 2, 10, QLatin1Char('0'))
        .arg(consumedSecRem, 2, 10, QLatin1Char('0'))
        .arg(totalHour, 2, 10, QLatin1Char('0'))
        .arg(totalMin, 2, 10, QLatin1Char('0'))
        .arg(totalSecRem, 2, 10, QLatin1Char('0'));
}

QString CsaGameWiring::buildEndLineText(const QString& cause, bool loserIsBlack) const
{
    const QString mark = loserIsBlack ? QStringLiteral("▲") : QStringLiteral("△");

    if (cause == tr("投了")) {
        return mark + tr("投了");
    }
    if (cause == tr("時間切れ")) {
        return mark + tr("時間切れ");
    }
    if (cause == tr("反則")) {
        return mark + tr("反則負け");
    }
    if (cause == tr("千日手")) {
        return tr("千日手");
    }
    if (cause == tr("連続王手の千日手")) {
        return mark + tr("反則負け（連続王手）");
    }
    if (cause == tr("入玉宣言")) {
        return tr("入玉宣言");
    }
    if (cause == tr("中断")) {
        return tr("中断");
    }
    return cause;
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

    qCDebug(lcUi) << "startCsaGame: starting CSA game setup";

    // CSA通信対局コーディネータが未作成の場合は作成する
    if (!m_coordinator) {
        m_coordinator = new CsaGameCoordinator(this);
        m_coordinatorOwnedByThis = true;

        // 依存オブジェクトを設定
        CsaGameCoordinator::Dependencies deps;
        deps.gameController = m_gameController;
        deps.view = m_shogiView;
        deps.clock = m_timeController ? m_timeController->clock() : nullptr;
        deps.boardController = m_boardController;
        deps.recordModel = m_kifuRecordModel;
        deps.sfenRecord = m_sfenHistory;
        deps.gameMoves = m_gameMoves;
        deps.usiCommLog = m_usiCommLog;
        deps.engineThinking = m_engineThinking;
        m_coordinator->setDependencies(deps);

        // シグナル配線
        wire();

        // CSA通信ログをEngineAnalysisTabに転送
        if (m_analysisTab) {
            connect(m_coordinator, &CsaGameCoordinator::csaCommLogAppended,
                    m_analysisTab, &EngineAnalysisTab::appendCsaLog);
            // EngineAnalysisTabからのCSAコマンド送信シグナルを接続
            connect(m_analysisTab, &EngineAnalysisTab::csaRawCommandRequested,
                    m_coordinator, &CsaGameCoordinator::sendRawCommand);
        }

        // BoardSetupControllerからの指し手をCsaGameCoordinatorに転送
        if (m_boardSetupController) {
            connect(m_boardSetupController, &BoardSetupController::csaMoveRequested,
                    m_coordinator, &CsaGameCoordinator::onHumanMove);
        }
    }

    // 対局開始オプションを設定
    CsaGameCoordinator::StartOptions options;
    options.host = dialog->host();
    options.port = dialog->port();
    options.username = dialog->loginId();
    options.password = dialog->password();
    options.csaVersion = dialog->csaVersion();

    if (dialog->isHuman()) {
        options.playerType = CsaGameCoordinator::PlayerType::Human;
    } else {
        options.playerType = CsaGameCoordinator::PlayerType::Engine;
        options.engineName = dialog->engineName();
        options.engineNumber = dialog->engineNumber();
        // エンジンパスは設定から取得
        const QList<CsaGameDialog::Engine>& engineList = dialog->engineList();
        if (!engineList.isEmpty()) {
            int idx = dialog->engineNumber();
            if (idx >= 0 && idx < engineList.size()) {
                options.enginePath = engineList.at(idx).path;
            }
        }
    }

    // プレイモード変更を通知
    Q_EMIT playModeChanged(static_cast<int>(PlayMode::CsaNetworkMode));

    qCDebug(lcUi) << "startCsaGame: About to start game and create waiting dialog";

    // 待機ダイアログを作成
    CsaWaitingDialog waitingDialog(m_coordinator, parent);

    // キャンセル要求時の処理を接続
    connect(&waitingDialog, &CsaWaitingDialog::cancelRequested,
            this, &CsaGameWiring::onWaitingCancelled);

    qCDebug(lcUi) << "startCsaGame: CsaWaitingDialog created, now starting game...";

    // 対局を開始（シグナルがCsaWaitingDialogに届くようになった後に開始）
    m_coordinator->startGame(options);

    qCDebug(lcUi) << "startCsaGame: Game started, showing waiting dialog...";

    // 待機ダイアログを表示（対局開始またはエラーまでブロック）
    waitingDialog.exec();

    qCDebug(lcUi) << "startCsaGame: Waiting dialog closed";

    return true;
}
