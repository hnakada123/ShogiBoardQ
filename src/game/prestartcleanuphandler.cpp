#include "prestartcleanuphandler.h"

#include <QDebug>
#include <QLabel>
#include <QTableView>
#include <QItemSelectionModel>

#include "boardinteractioncontroller.h"
#include "shogiview.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "usicommlogmodel.h"
#include "timecontrolcontroller.h"
#include "kifudisplay.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "recordpane.h"

PreStartCleanupHandler::PreStartCleanupHandler(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_boardController(deps.boardController)
    , m_shogiView(deps.shogiView)
    , m_kifuRecordModel(deps.kifuRecordModel)
    , m_kifuBranchModel(deps.kifuBranchModel)
    , m_lineEditModel1(deps.lineEditModel1)
    , m_lineEditModel2(deps.lineEditModel2)
    , m_timeController(deps.timeController)
    , m_evalChart(deps.evalChart)
    , m_evalGraphController(deps.evalGraphController)
    , m_recordPane(deps.recordPane)
    , m_startSfenStr(deps.startSfenStr)
    , m_currentSfenStr(deps.currentSfenStr)
    , m_activePly(deps.activePly)
    , m_currentSelectedPly(deps.currentSelectedPly)
    , m_currentMoveIndex(deps.currentMoveIndex)
{
}

void PreStartCleanupHandler::setBranchDisplayPlan(QHash<int, QMap<int, BranchCandidateDisplay>>* plan)
{
    m_branchDisplayPlan = plan;
}

void PreStartCleanupHandler::performCleanup()
{
    qDebug().noquote() << "[PreStartCleanupHandler] performCleanup: start";

    // 盤/ハイライト等のビジュアル初期化
    clearBoardAndHighlights();
    clearClockDisplay();

    // 「現在の局面」から開始かどうかを判定
    const bool startFromCurrentPos = isStartFromCurrentPosition();

    // 安全な keepRow（保持したい最終行＝選択中の行）を算出
    int keepRow = m_currentSelectedPly ? qMax(0, *m_currentSelectedPly) : 0;

    // 棋譜モデルのクリーンアップ
    keepRow = cleanupKifuModel(startFromCurrentPos, keepRow);

    // 手数トラッキングの更新
    updatePlyTracking(startFromCurrentPos, keepRow);

    // 分岐モデルをクリア
    clearBranchModel();

    // コメント欄をクリア
    Q_EMIT broadcastCommentRequested(QString(), true);

    // USIログの初期化
    resetUsiLogModels();

    // 時間制御のリセット
    resetTimeControl();

    // 評価値グラフの初期化/トリム
    resetEvaluationGraph();

    // 棋譜欄の開始行を選択
    selectStartRow();

    // デバッグログ
    qDebug().noquote()
        << "[PreStartCleanupHandler] performCleanup: startFromCurrentPos=" << startFromCurrentPos
        << " keepRow=" << keepRow
        << " rows(after)=" << (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
}

bool PreStartCleanupHandler::isStartFromCurrentPosition() const
{
    if (!m_startSfenStr || !m_currentSfenStr) return false;

    // m_startSfenStr が空で m_currentSfenStr に値がある場合は「現在の局面から開始」
    return m_startSfenStr->trimmed().isEmpty() && !m_currentSfenStr->trimmed().isEmpty();
}

void PreStartCleanupHandler::clearBoardAndHighlights()
{
    if (m_boardController) {
        m_boardController->clearAllHighlights();
    }
}

void PreStartCleanupHandler::clearClockDisplay()
{
    if (m_shogiView) {
        m_shogiView->blackClockLabel()->setText(QStringLiteral("00:00:00"));
        m_shogiView->whiteClockLabel()->setText(QStringLiteral("00:00:00"));
    }
}

int PreStartCleanupHandler::cleanupKifuModel(bool startFromCurrentPos, int keepRow)
{
    if (!m_kifuRecordModel) return 0;

    if (startFromCurrentPos) {
        // 1) 1局目の途中までを残して、末尾だけを削除
        const int rows = m_kifuRecordModel->rowCount();
        if (rows <= 0) {
            // 空なら見出しだけ用意
            m_kifuRecordModel->appendItem(
                new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                QStringLiteral("（１手 / 合計）")));
            return 0;
        }
        // keepRow をモデル範囲にクランプし、末尾の余剰行を一括削除
        if (keepRow > rows - 1) keepRow = rows - 1;

        // ★ 終局手（投了、詰み、千日手など）から再対局する場合、終局手を除外
        // 終局手は盤面を変更しないため、その直前の局面から再対局する
        static const QStringList kTerminalKeywords = {
            QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
            QStringLiteral("千日手"), QStringLiteral("切れ負け"),
            QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
            QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
            QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
        };
        if (keepRow > 0 && keepRow < rows) {
            if (KifuDisplay* item = m_kifuRecordModel->item(keepRow)) {
                const QString moveText = item->currentMove();
                for (const auto& kw : kTerminalKeywords) {
                    if (moveText.contains(kw)) {
                        qDebug().noquote() << "[PreStartCleanupHandler] cleanupKifuModel: detected terminal move at row"
                                           << keepRow << "(" << moveText << "), adjusting to" << (keepRow - 1);
                        keepRow = keepRow - 1;
                        break;
                    }
                }
            }
        }

        const int toRemove = rows - (keepRow + 1);
        if (toRemove > 0) {
            m_kifuRecordModel->removeLastItems(toRemove);
        }
        return keepRow;
    } else {
        // 2) 平手/手合割など「新規初期局面から開始」のときは従来通り全消去
        m_kifuRecordModel->clearAllItems();
        // 見出し行を先頭へ
        m_kifuRecordModel->appendItem(
            new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                            QStringLiteral("（１手 / 合計）")));
        return 0;
    }
}

void PreStartCleanupHandler::updatePlyTracking(bool startFromCurrentPos, int keepRow)
{
    if (startFromCurrentPos) {
        if (m_activePly) *m_activePly = keepRow;
        if (m_currentSelectedPly) *m_currentSelectedPly = keepRow;
        if (m_currentMoveIndex) *m_currentMoveIndex = keepRow;
    } else {
        if (m_activePly) *m_activePly = 0;
        if (m_currentSelectedPly) *m_currentSelectedPly = 0;
        if (m_currentMoveIndex) *m_currentMoveIndex = 0;
    }
}

void PreStartCleanupHandler::clearBranchModel()
{
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clear();
    }
    if (m_branchDisplayPlan) {
        m_branchDisplayPlan->clear();
    }
}

void PreStartCleanupHandler::resetUsiLogModels()
{
    qDebug().noquote() << "[PreStartCleanupHandler] resetUsiLogModels: RESETTING ENGINE NAMES";

    auto resetInfo = [](UsiCommLogModel* m) {
        if (!m) return;
        m->clear();
        m->setEngineName(QString());
        m->setPredictiveMove(QString());
        m->setSearchedMove(QString());
        m->setSearchDepth(QString());
        m->setNodeCount(QString());
        m->setNodesPerSecond(QString());
        m->setHashUsage(QString());
    };

    resetInfo(m_lineEditModel1);
    resetInfo(m_lineEditModel2);

    qDebug().noquote() << "[PreStartCleanupHandler] resetUsiLogModels: ENGINE NAMES RESET DONE";
}

void PreStartCleanupHandler::resetTimeControl()
{
    if (m_timeController) {
        m_timeController->clearGameStartTime();
    }
}

void PreStartCleanupHandler::resetEvaluationGraph()
{
    const bool startFromCurrentPos = isStartFromCurrentPosition();
    if (!startFromCurrentPos) {
        if (m_evalChart) {
            m_evalChart->clearAll();
        }
        if (m_evalGraphController) {
            m_evalGraphController->clearScores();
        }
    } else {
        if (m_evalGraphController && m_currentMoveIndex) {
            const int trimPly = qMax(0, *m_currentMoveIndex);
            m_evalGraphController->trimToPly(trimPly);
        }
    }
}

void PreStartCleanupHandler::selectStartRow()
{
    if (!m_recordPane || !m_kifuRecordModel || m_kifuRecordModel->rowCount() <= 0) return;

    if (auto* view = m_recordPane->kifuView()) {
        if (auto* sel = view->selectionModel()) {
            const QModelIndex idx = m_kifuRecordModel->index(0, 0);
            sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
    m_kifuRecordModel->setCurrentHighlightRow(0);
}
