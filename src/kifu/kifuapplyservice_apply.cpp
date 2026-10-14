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

namespace {
const QStringList& terminalKeywords()
{
    static const QStringList keywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    return keywords;
}

bool isTerminalPretty(const QString& text)
{
    for (const auto& keyword : terminalKeywords()) {
        if (text.contains(keyword)) {
            return true;
        }
    }
    return false;
}
} // namespace

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

    const QList<KifDisplayItem>& disp = res.mainline.disp;
    const QStringList& usiMoves = res.mainline.usiMoves;
    const bool hasTerminal = !disp.isEmpty() && isTerminalPretty(disp.back().prettyMove);

    // 局面編集から保存した棋譜は指し手が0手でも、有効な開始局面図を持つ。
    const bool hasPositionDiagram = teaiLabel == QStringLiteral("BOD") || teaiLabel == QStringLiteral("配置");
    if (!hasPositionDiagram && !validateParsedResult(filePath, usiMoves, disp, hasTerminal, callerTag)) {
        return false;
    }

    // 棋譜の正本は分岐ツリー。対局の局面列・指し手列はここでは触らず、対局開始時に
    // 表示中の手順から作り直す（MainWindowGameStartService::prepareForInitializeGame）
    applyToRecordView(res, filePath, teaiLabel, parseWarn);
    logStep("applyToRecordView");

    applyBranchTree(res, initialSfen);
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
    const QList<KifDisplayItem>& disp = res.mainline.disp;
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

    KifuApplyLogger::logImportSummary(filePath, res.mainline.usiMoves, disp, teaiLabel, parseWarn, QString());
}

void KifuApplyService::applyBranchTree(const KifParseResult& res, const QString& initialSfen)
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
        KifuBranchTreeBuilder::buildFromKifParseResult(branchTree, res, initialSfen);
        if (navState != nullptr) {
            navState->goToRoot();
        }
        branchTree->endBatchUpdate();
        qCDebug(lcKifu).noquote() << "KifuBranchTree built: nodeCount=" << branchTree->nodeCount()
                                  << "lineCount=" << branchTree->lineCount();
        if (m_hooks.branchTreeBuilt) {
            m_hooks.branchTreeBuilt();
        }
    }

    applyBranchMarksForCurrentLine();

    if (branchTreeManager && branchTree != nullptr && !branchTree->isEmpty()) {
        // 分岐ツリーの行データは treeChanged を受けた KifuDisplayCoordinator が
        // KifuDisplayPresenter::buildBranchTreeRows() で構築済み。ここでは開始局面の
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
}
