/// @file kifuapplyservice_apply.cpp
/// @brief 棋譜解析結果の一括適用フェーズ実装

#include "kifuapplyservice.h"

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

void KifuApplyService::applyParsedResult(
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
    *m_refs.kifuUsiMoves = res.mainline.usiMoves;
    const bool hasTerminal = !disp.isEmpty() && isTerminalPretty(disp.back().prettyMove);

    if (!validateParsedResult(filePath, disp, hasTerminal, callerTag)) {
        return;
    }

    rebuildMainlineState(initialSfen, hasTerminal);
    logStep("rebuildMainlineState");

    rebuildPositionCommands(initialSfen);
    logStep("rebuildPositionCommands");

    applyToRecordView(res, filePath, teaiLabel, parseWarn);
    logStep("applyToRecordView");

    applyBranchTree(res, initialSfen);
    logStep("applyBranchTree");

    *m_refs.loadingKifu = false;

    qCDebug(lcKifu).noquote()
        << QStringLiteral("applyParsedResult TOTAL: %1 ms").arg(totalTimer.elapsed());
    qCDebug(lcKifu).noquote() << callerTag << "OUT";
}

bool KifuApplyService::validateParsedResult(const QString& filePath, const QList<KifDisplayItem>& disp,
                                            bool hasTerminal, const char* callerTag)
{
    if (m_refs.kifuUsiMoves->isEmpty() && !hasTerminal && disp.size() <= 1) {
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

void KifuApplyService::rebuildMainlineState(const QString& initialSfen, bool hasTerminal)
{
    qCDebug(lcKifu).noquote() << "applyParsedResult: calling rebuildSfenRecord"
                              << "initialSfen=" << initialSfen.left(60)
                              << "usiMoves.size=" << m_refs.kifuUsiMoves->size()
                              << "hasTerminal=" << hasTerminal;

    rebuildSfenRecord(initialSfen, *m_refs.kifuUsiMoves, hasTerminal);
    rebuildGameMoves(initialSfen, *m_refs.kifuUsiMoves);

    QStringList* sfenHistory = *m_refs.sfenHistory;
    qCDebug(lcKifu).noquote() << "applyParsedResult: after rebuildSfenRecord"
                              << "sfenHistory*=" << static_cast<const void*>(sfenHistory)
                              << "sfenHistory.size=" << (sfenHistory ? sfenHistory->size() : -1);
    if (sfenHistory && !sfenHistory->isEmpty()) {
        qCDebug(lcKifu).noquote() << "sfenHistory[0]=" << sfenHistory->first().left(60);
        if (sfenHistory->size() > 1) {
            qCDebug(lcKifu).noquote() << "sfenHistory[1]=" << sfenHistory->at(1).left(60);
        }
        if (sfenHistory->size() > 2) {
            qCDebug(lcKifu).noquote() << "sfenHistory[last]=" << sfenHistory->last().left(60);
        }
    }
}

void KifuApplyService::rebuildPositionCommands(const QString& initialSfen)
{
    m_refs.positionStrList->clear();
    m_refs.positionStrList->reserve(m_refs.kifuUsiMoves->size() + 1);

    const QString base = QStringLiteral("position sfen %1").arg(initialSfen);
    m_refs.positionStrList->push_back(base);

    QStringList acc;
    acc.reserve(m_refs.kifuUsiMoves->size());
    for (qsizetype i = 0; i < m_refs.kifuUsiMoves->size(); ++i) {
        acc.push_back(m_refs.kifuUsiMoves->at(i));
        m_refs.positionStrList->push_back(base + QStringLiteral(" moves ") + acc.join(' '));
    }

    qCDebug(lcKifu).noquote() << "position list built. count=" << m_refs.positionStrList->size();
    if (!m_refs.positionStrList->isEmpty()) {
        qCDebug(lcKifu).noquote() << "pos[0]=" << m_refs.positionStrList->first();
        if (m_refs.positionStrList->size() > 1) {
            qCDebug(lcKifu).noquote() << "pos[1]=" << m_refs.positionStrList->at(1);
        }
    }
}

void KifuApplyService::applyToRecordView(const KifParseResult& res, const QString& filePath,
                                         const QString& teaiLabel, const QString& parseWarn)
{
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    QStringList* sfenHistory = *m_refs.sfenHistory;
    KifuNavigationState* navState = *m_refs.navState;

    if (m_hooks.displayGameRecord) {
        m_hooks.displayGameRecord(disp);
    }

    *m_refs.dispMain = disp;
    if (sfenHistory) {
        *m_refs.sfenMain = *sfenHistory;
    } else {
        m_refs.sfenMain->clear();
    }
    *m_refs.gmMain = *m_refs.gameMoves;

    m_refs.variationsByPly->clear();
    m_refs.variationsSeq->clear();
    for (const KifVariation& kv : std::as_const(res.variations)) {
        KifLine line = kv.line;
        line.startPly = kv.startPly;
        if (line.disp.isEmpty()) {
            continue;
        }
        (*m_refs.variationsByPly)[line.startPly].push_back(line);
        m_refs.variationsSeq->push_back(line);
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
    *m_refs.activePly = 0;

    if (m_hooks.enableArrowButtons) {
        m_hooks.enableArrowButtons();
    }

    KifuApplyLogger::logImportSummary(filePath, *m_refs.kifuUsiMoves, disp, teaiLabel, parseWarn,
                                      QString(), sfenHistory, m_refs.gameMoves);
}

void KifuApplyService::applyBranchTree(const KifParseResult& res, const QString& initialSfen)
{
    KifuBranchTree*& branchTree = *m_refs.branchTree;
    KifuNavigationState* navState = *m_refs.navState;
    BranchTreeManager* branchTreeManager = *m_refs.branchTreeManager;

    if (branchTree != nullptr) {
        if (navState != nullptr) {
            navState->setCurrentNode(nullptr);
        }
        KifuBranchTreeBuilder::buildFromKifParseResult(branchTree, res, initialSfen);
        if (navState != nullptr) {
            navState->goToRoot();
        }
        qCDebug(lcKifu).noquote() << "KifuBranchTree built: nodeCount=" << branchTree->nodeCount()
                                  << "lineCount=" << branchTree->lineCount();
        if (m_hooks.branchTreeBuilt) {
            m_hooks.branchTreeBuilt();
        }
    }

    applyBranchMarksForCurrentLine();

    if (branchTreeManager && branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchTreeManager::ResolvedRowLite> rows;
        const QList<BranchLine> lines = branchTree->allLines();
        rows.reserve(lines.size());

        for (int i = 0; i < lines.size(); ++i) {
            const BranchLine& line = lines.at(i);
            BranchTreeManager::ResolvedRowLite resolved;
            resolved.startPly = line.branchPly;
            resolved.parent = (line.branchPoint != nullptr)
                                  ? branchTree->findLineIndexForNode(line.branchPoint)
                                  : -1;

            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                item.prettyMove = node->displayText();
                item.comment = node->comment();
                item.timeText = node->timeText();
                item.ply = node->ply();
                resolved.disp.append(item);
                resolved.sfen.append(node->sfen());
            }

            rows.push_back(std::move(resolved));
        }

        branchTreeManager->setBranchTreeRows(rows);
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
