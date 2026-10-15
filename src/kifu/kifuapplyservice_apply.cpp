/// @file kifuapplyservice_apply.cpp
/// @brief 棋譜解析結果の一括適用フェーズ実装

#include "kifuapplyservice.h"
#include "kifuloadparser.h"

#include "kifuapplylogger.h"
#include "branchtreemanager.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifunavigationstate.h"
#include "recordpane.h"
#include "logcategories.h"

#include <QAbstractItemView>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QTableView>

#include <utility>

bool KifuApplyService::applyParsedResult(
    const QString& filePath,
    const QString& initialSfen,
    const QString& teaiLabel,
    const KifParseResult& res,
    const QString& parseWarn,
    const char* callerTag)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer stepTimer;
    stepTimer.start();
    auto logStep = [&](const char* stepName) {
        qCDebug(lcKifu).noquote()
            << QStringLiteral("%1: %2 ms").arg(stepName).arg(stepTimer.elapsed());
        stepTimer.restart();
    };

    const QList<KifDisplayItem>& disp = res.mainline.displayItems();
    const QStringList& usiMoves = res.mainline.usiMoves();
    const bool hasTerminal = res.mainline.endsWithTerminal();

    // 局面編集から保存した棋譜は指し手が0手でも、有効な開始局面図を持つ。
    const bool hasPositionDiagram = teaiLabel == QStringLiteral("BOD") || teaiLabel == QStringLiteral("配置");
    if (!hasPositionDiagram && !validateParsedResult(filePath, usiMoves, disp, hasTerminal, callerTag)) {
        return false;
    }

    if (!KifuBranchTreeBuilder::validateVariations(res)) {
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(tr("棋譜の分岐元または開始手数が不正です。"));
        return false;
    }

    // 棋譜の正本は分岐ツリー。対局の局面列・指し手列はここでは触らず、対局開始時に
    // 表示中の手順から作り直す（MatchCoordinator::configureAndStart）
    applyToRecordView(res, filePath, teaiLabel, parseWarn);
    logStep("applyToRecordView");

    if (!applyBranchTree(res, initialSfen)) {
        *m_refs.loadingKifu = false;
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(tr("棋譜の分岐元または開始手数が不正です。"));
        return false;
    }
    logStep("applyBranchTree");

    *m_refs.loadingKifu = false;

    // 0手の棋譜でも、選択行が0のままでselectionChangedが発火しない場合がある。
    // ツリーと履歴を構築し終えてから、開始局面を明示的に表示する。
    if (m_hooks.syncBoardAndHighlightsAtRow) {
        m_hooks.syncBoardAndHighlightsAtRow(0);
    }

    qCDebug(lcKifu).noquote()
        << QStringLiteral("applyParsedResult TOTAL: %1 ms").arg(totalTimer.elapsed());
    qCDebug(lcKifu).noquote() << callerTag << "OUT";
    return true;
}

bool KifuApplyService::validateParsedResult(const QString& filePath, const QStringList& usiMoves,
                                            const QList<KifDisplayItem>& disp,
                                            bool hasTerminal, const char* callerTag)
{
    if (usiMoves.isEmpty() && !hasTerminal && disp.size() <= 1) {
        const QString errorMessage =
            tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(filePath);
        if (m_hooks.errorOccurred) {
            m_hooks.errorOccurred(errorMessage);
        }
        qCDebug(lcKifu).noquote() << callerTag << "OUT (no moves)";
        *m_refs.loadingKifu = false;
        return false;
    }
    return true;
}

void KifuApplyService::applyToRecordView(const KifParseResult& res, const QString& filePath,
                                         const QString& teaiLabel, const QString& parseWarn)
{
    const QList<KifDisplayItem>& disp = res.mainline.displayItems();
    KifuNavigationState* const navState = currentNavState();

    if (m_hooks.displayGameRecord) {
        m_hooks.displayGameRecord(disp);
    }

    if (m_refs.recordPane && m_refs.recordPane->kifuView()) {
        QTableView* view = m_refs.recordPane->kifuView();
        if (view->model() && view->model()->rowCount() > 0) {
            const QModelIndex idx0 = view->model()->index(0, 0);
            if (view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollTo(idx0, QAbstractItemView::PositionAtTop);
        }
    }

    if (navState != nullptr) {
        navState->goToRoot();
    }

    if (m_hooks.enableArrowButtons) {
        m_hooks.enableArrowButtons();
    }

    KifuApplyLogger::logImportSummary(filePath, res.mainline.usiMoves(), disp, teaiLabel, parseWarn, QString());
}

bool KifuApplyService::applyBranchTree(const KifParseResult& res, const QString& initialSfen)
{
    KifuBranchTree* const branchTree = currentTree();
    KifuNavigationState* const navState = currentNavState();
    BranchTreeManager* const branchTreeManager = currentTreeManager();

    if (branchTree != nullptr) {
        if (navState != nullptr) {
            navState->setCurrentNode(nullptr);
        }
        // 構築中の treeChanged を抑止し、ナビ状態をルートへ戻してから1回だけ発火させる
        branchTree->beginBatchUpdate();
        const bool built = KifuBranchTreeBuilder::buildFromKifParseResult(branchTree, res, initialSfen);
        if (navState != nullptr) {
            navState->goToRoot();
        }
        branchTree->endBatchUpdate();
        if (!built) return false;
        qCDebug(lcKifu).noquote() << "KifuBranchTree built: nodeCount=" << branchTree->nodeCount()
                                  << "lineCount=" << branchTree->lineCount();
        if (m_hooks.branchTreeBuilt) {
            m_hooks.branchTreeBuilt();
        }
    }

    applyBranchMarksForCurrentLine();

    if (branchTreeManager && branchTree != nullptr && !branchTree->isEmpty()) {
        // 分岐ツリーは treeChanged を受けた KifuDisplayCoordinator が
        // BranchTreeManager::setBranchTree() で描画済み。ここでは開始局面の
        // ハイライトと中央表示のみ行う。
        branchTreeManager->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }

    if (m_refs.kifuBranchModel) {
        m_refs.kifuBranchModel->clearBranchCandidates();
        m_refs.kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view = m_refs.recordPane ? m_refs.recordPane->branchView() : nullptr) {
            view->setVisible(true);
            view->setEnabled(false);
        }
    }
    return true;
}
