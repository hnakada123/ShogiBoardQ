/// @file prestartcleanuphandler.cpp
/// @brief 対局開始前クリーンアップハンドラクラスの実装

#include "prestartcleanuphandler.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "matchcoordinator.h"

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
#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "sfenutils.h"

// ============================================================
// 初期化
// ============================================================

PreStartCleanupHandler::PreStartCleanupHandler(const Dependencies& deps, QObject* parent)
    : QObject(parent)
{
    updateDependencies(deps);
}

void PreStartCleanupHandler::updateDependencies(const Dependencies& deps)
{
    m_boardController = deps.boardController;
    m_shogiView = deps.shogiView;
    m_kifuRecordModel = deps.kifuRecordModel;
    m_kifuBranchModel = deps.kifuBranchModel;
    m_lineEditModel1 = deps.lineEditModel1;
    m_lineEditModel2 = deps.lineEditModel2;
    m_timeController = deps.timeController;
    m_evalChart = deps.evalChart;
    m_evalGraphController = deps.evalGraphController;
    m_recordPane = deps.recordPane;
    m_liveGameSession = deps.liveGameSession;
    m_branchTree = deps.branchTree;
    m_navState = deps.navState;
}

// ============================================================
// メインクリーンアップ
// ============================================================

KifuBranchNode* PreStartCleanupHandler::performCleanup(const GameStartPreparation& preparation)
{
    const bool preserve = preparation.mode == GameStartPreparation::Mode::CurrentPosition;
    KifuBranchNode* node = preserve && m_branchTree
        ? m_branchTree->nodeAt(preparation.startNodeId) : nullptr;
    // 存在しないノードから別の分岐や同じSFENを探し直さない。
    if (preserve && preparation.startNodeId >= 0 && !node) return nullptr;
    if (m_liveGameSession && m_liveGameSession->isActive()) m_liveGameSession->discard();

    if (node && node->isTerminal() && node->parent()) {
        KifuBranchNode* terminal = node;
        node = node->parent();
        if (m_navState) m_navState->setCurrentNode(node);
        if (terminal->terminalType() == TerminalType::Interrupt && terminal->childCount() == 0) {
            m_branchTree->removeLeaf(terminal);
        }
    }
    if (m_branchTree && (!preserve || !node)) {
        if (m_navState) {
            m_navState->setCurrentNode(nullptr);
            m_navState->clearLineSelectionMemory();
        }
        m_branchTree->setRootSfen(SfenUtils::normalizeStart(preparation.rootSfen));
        node = m_branchTree->root();
    }
    const int ply = node ? node->ply() : 0;
    if (m_navState) m_navState->setCurrentNode(node);
    clearBoardAndHighlights();
    clearClockDisplay();
    cleanupKifuModel(node);
    clearBranchModel();
    resetUsiLogModels();
    resetTimeControl();
    resetEvaluationGraph(preserve, ply);
    selectStartRow(ply);
    return node;
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

void PreStartCleanupHandler::cleanupKifuModel(KifuBranchNode* startNode)
{
    if (!m_kifuRecordModel) return;
    m_kifuRecordModel->clearAllItems();
    if (startNode && m_branchTree) {
        const auto path = m_branchTree->pathToNode(startNode);
        QList<KifuDisplay*> items;
        items.reserve(path.size());
        for (KifuBranchNode* node : path) {
            auto* item = new KifuDisplay(node->displayText(), node->timeText(), node->comment());
            if (node->parent() && !node->isTerminal()) {
                item->beforeSfen = node->parent()->sfen();
                item->usiMove = KifuPresentation::usiMove(node->move());
            }
            items.append(item);
        }
        m_kifuRecordModel->appendItems(items);
        return;
    }
    m_kifuRecordModel->appendItem(
        new KifuDisplay(tr("=== 開始局面 ==="), tr("（１手 / 合計）")));
}

void PreStartCleanupHandler::clearBranchModel()
{
    if (m_kifuBranchModel) {
        m_kifuBranchModel->clear();
    }
}

void PreStartCleanupHandler::resetUsiLogModels()
{
    qCDebug(lcGame).noquote() << "resetUsiLogModels: RESETTING ENGINE NAMES";

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

    qCDebug(lcGame).noquote() << "resetUsiLogModels: ENGINE NAMES RESET DONE";
}

void PreStartCleanupHandler::resetTimeControl()
{
    if (m_timeController) {
        m_timeController->clearGameStartTime();
    }
}

void PreStartCleanupHandler::resetEvaluationGraph(bool preserve, int ply)
{
    if (!preserve) {
        if (m_evalChart) {
            m_evalChart->clearAll();
        }
        if (m_evalGraphController) {
            m_evalGraphController->clearScores();
        }
    } else {
        if (m_evalGraphController) {
            m_evalGraphController->trimToPly(ply);
        }
    }
}

void PreStartCleanupHandler::selectStartRow(int row)
{
    if (!m_kifuRecordModel || m_kifuRecordModel->rowCount() <= 0) return;
    m_kifuRecordModel->setCurrentHighlightRow(row);
    if (!m_recordPane) return;

    if (auto* view = m_recordPane->kifuView()) {
        if (auto* sel = view->selectionModel()) {
            const QModelIndex idx = m_kifuRecordModel->index(row, 0);
            sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
}
