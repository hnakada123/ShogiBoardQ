#include "csagamewiring.h"

#include <QDebug>
#include <QStatusBar>
#include <QModelIndex>
#include <QTableView>

#include "csagamecoordinator.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "kifurecordlistmodel.h"
#include "recordpane.h"
#include "boardinteractioncontroller.h"
#include "kifudisplay.h"

CsaGameWiring::CsaGameWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_coordinator(deps.coordinator)
    , m_gameController(deps.gameController)
    , m_shogiView(deps.shogiView)
    , m_kifuRecordModel(deps.kifuRecordModel)
    , m_recordPane(deps.recordPane)
    , m_boardController(deps.boardController)
    , m_statusBar(deps.statusBar)
    , m_sfenRecord(deps.sfenRecord)
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
        qWarning().noquote() << "[CsaGameWiring] wire: coordinator is null";
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

    qDebug().noquote() << "[CsaGameWiring] wire: connected all signals";
}

void CsaGameWiring::unwire()
{
    if (!m_coordinator) return;

    disconnect(m_coordinator, nullptr, this, nullptr);
    qDebug().noquote() << "[CsaGameWiring] unwire: disconnected all signals";
}

void CsaGameWiring::onGameStarted(const QString& blackName, const QString& whiteName)
{
    qInfo().noquote() << "[CsaGameWiring] onGameStarted:" << blackName << "vs" << whiteName;

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
            new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                            QStringLiteral("（１手 / 合計）")));
    }

    // m_sfenRecordをクリアし、開始局面を追加
    const QString hiratePosition = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    if (m_sfenRecord) {
        m_sfenRecord->clear();
        m_sfenRecord->append(hiratePosition);
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
    qInfo().noquote() << "[CsaGameWiring] onGameEnded:" << result << "(" << cause << ")"
                      << "consumedTimeMs=" << consumedTimeMs;

    if (!m_coordinator) return;

    // 敗者の判定
    const bool iAmLoser = (result == tr("負け"));
    const bool isBlackSide = m_coordinator->isBlackSide();
    const bool loserIsBlack = (iAmLoser == isBlackSide);

    // 終局行テキストを生成
    const QString endLine = buildEndLineText(cause, loserIsBlack);

    qDebug().noquote() << "[CsaGameWiring] onGameEnded endLine=" << endLine;

    // 消費時間をフォーマット
    const int totalMs = loserIsBlack ? m_coordinator->blackTotalTimeMs()
                                     : m_coordinator->whiteTotalTimeMs();
    const QString elapsedStr = formatElapsedTime(consumedTimeMs, totalMs + consumedTimeMs);

    // 棋譜欄に追加
    Q_EMIT appendKifuLineRequested(endLine, elapsedStr);

    // m_sfenRecordにも終局行用のダミーエントリを追加
    if (m_sfenRecord && !m_sfenRecord->isEmpty()) {
        qDebug().noquote() << "[CsaGameWiring] onGameEnded: appending last SFEN to sfenRecord";
        m_sfenRecord->append(m_sfenRecord->last());
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

    qDebug().noquote() << "[CsaGameWiring] onMoveMade:" << prettyMove;

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
    qDebug().noquote() << "[CsaGameWiring] onTurnChanged: myTurn =" << isMyTurn;

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
        qWarning().noquote() << "[CSA]" << message;
    } else {
        qInfo().noquote() << "[CSA]" << message;
    }

    if (m_statusBar) {
        m_statusBar->showMessage(message, 3000);
    }
}

void CsaGameWiring::onMoveHighlightRequested(const QPoint& from, const QPoint& to)
{
    qDebug().noquote() << "[CsaGameWiring] onMoveHighlightRequested: from=" << from << "to=" << to;

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
