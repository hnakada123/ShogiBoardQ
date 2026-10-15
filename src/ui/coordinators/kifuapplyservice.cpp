/// @file kifuapplyservice.cpp
/// @brief 棋譜解析結果のモデル適用サービスの実装

#include "kifuapplyservice.h"
#include "kifuapplylogger.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "kiftosfenconverter.h"
#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifunavigationstate.h"
#include "branchtreemanager.h"
#include "shogiview.h"
#include "logcategories.h"

#include <QTableWidget>
#include <QAbstractItemView>
#include "kifuloadparser.h"
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QTableView>
#include <utility>

KifuApplyService::KifuApplyService(QObject* parent)
    : QObject(parent)
{
}

void KifuApplyService::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void KifuApplyService::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// SFEN/BOD局面読み込み
// ============================================================

bool KifuApplyService::loadPositionFromSfen(const QString& sfenStr)
{
    qCDebug(lcKifu).noquote() << "loadPositionFromSfen:" << sfenStr;

    QString sfen = sfenStr.trimmed();

    // "sfen " プレフィックスがあれば除去
    if (sfen.startsWith(QStringLiteral("sfen "), Qt::CaseInsensitive)) {
        sfen = sfen.mid(5).trimmed();
    }

    // SFEN文字列の検証
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() < 4) {
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(tr("無効なSFEN形式です。"));
        return false;
    }

    // 分岐ツリーは棋譜の正本で、起動時にレジストリが作って渡す
    KifuBranchTree* const branchTree = currentTree();
    KifuNavigationState* const navState = currentNavState();
    if (branchTree == nullptr) {
        qCWarning(lcKifu) << "loadPositionFromSfen: branch tree is not set";
        return false;
    }

    *m_refs.loadingKifu = true;

    // 表示用データを作成
    QList<KifDisplayItem> disp;
    KifDisplayItem startItem;
    startItem.ply = 0;
    startItem.prettyMove = tr("=== 開始局面 ===");
    startItem.comment = QString();
    disp.append(startItem);

    // ツリーをクリアする前にcurrentNodeをnullに設定
    if (navState != nullptr) {
        navState->setCurrentNode(nullptr);
        navState->resetPreferredLineIndex();
    }

    // KifuBranchTree をセットアップ
    branchTree->clear();
    branchTree->setRootSfen(sfen);

    // 表示中の手数（分岐ツリーのカーソル）を開始局面へ
    if (navState != nullptr) {
        navState->goToRoot();
    }

    // ゲーム情報をクリア
    populateGameInfo({});
    applyPlayersFromGameInfo({});

    // シグナルを発行
    if (m_hooks.displayGameRecord) m_hooks.displayGameRecord(disp);
    qCDebug(lcKifu) << "emitting syncBoardAndHighlightsAtRow(0) from loadPositionFromSfen";
    if (m_hooks.syncBoardAndHighlightsAtRow) m_hooks.syncBoardAndHighlightsAtRow(0);
    if (m_hooks.enableArrowButtons) m_hooks.enableArrowButtons();

    *m_refs.loadingKifu = false;

    qCDebug(lcKifu).noquote() << "loadPositionFromSfen: completed";
    return true;
}

bool KifuApplyService::loadPositionFromBod(const QString& bodStr)
{
    qCDebug(lcKifu).noquote() << "loadPositionFromBod: length =" << bodStr.size();

    QString sfen;
    QString detectedLabel;
    QString warn;

    const QStringList lines = bodStr.split(QLatin1Char('\n'));

    if (!KifToSfenConverter::buildInitialSfenFromBod(lines, sfen, &detectedLabel, &warn)) {
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(tr("BOD形式の解析に失敗しました。%1").arg(warn));
        return false;
    }

    if (sfen.isEmpty()) {
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(tr("BOD形式から局面を取得できませんでした。"));
        return false;
    }

    qCDebug(lcKifu).noquote() << "loadPositionFromBod: converted SFEN =" << sfen;

    // 手番情報を追加
    if (!sfen.contains(QLatin1Char(' '))) {
        QString turn = QStringLiteral("b");
        if (bodStr.contains(QStringLiteral("後手番")) || bodStr.contains(QStringLiteral("上手番"))) {
            turn = QStringLiteral("w");
        }
        sfen = QStringLiteral("%1 %2 - 1").arg(sfen, turn);
    }

    return loadPositionFromSfen(sfen);
}

// ============================================================
// 対局情報
// ============================================================

void KifuApplyService::populateGameInfo(const QList<KifGameInfoItem>& items)
{
    if (!m_refs.gameInfoTable) {
        qCWarning(lcKifu).noquote() << "populateGameInfo: gameInfoTable is null, skipping table update";
        if (m_hooks.gameInfoPopulated) m_hooks.gameInfoPopulated(items);
        return;
    }

    m_refs.gameInfoTable->blockSignals(true);

    m_refs.gameInfoTable->clearContents();
    m_refs.gameInfoTable->setRowCount(static_cast<int>(items.size()));

    for (int row = 0; row < static_cast<int>(items.size()); ++row) {
        const auto& it = items.at(row);
        auto *keyItem   = new QTableWidgetItem(it.key);
        auto *valueItem = new QTableWidgetItem(it.value);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_refs.gameInfoTable->setItem(row, 0, keyItem);
        m_refs.gameInfoTable->setItem(row, 1, valueItem);
    }

    m_refs.gameInfoTable->resizeColumnToContents(0);
    m_refs.gameInfoTable->blockSignals(false);

    addGameInfoTabIfMissing();

    if (m_hooks.gameInfoPopulated) m_hooks.gameInfoPopulated(items);
}

void KifuApplyService::addGameInfoTabIfMissing()
{
    // 対局情報は GameInfoPaneController を介したドック表示に一本化した。
    // 棋譜読み込み時のデータ反映はテーブル更新のみ行い、タブへの再親子付けは行わない。
    if (m_refs.gameInfoTable == nullptr) {
        qCDebug(lcKifu).noquote() << "addGameInfoTabIfMissing: gameInfoTable is null, skipping";
        return;
    }
}

QString KifuApplyService::findGameInfoValue(const QList<KifGameInfoItem>& items,
                                            const QStringList& keys) const
{
    for (const auto& it : items) {
        if (keys.contains(it.key)) {
            const QString v = it.value.trimmed();
            if (!v.isEmpty()) return v;
        }
    }
    return QString();
}

void KifuApplyService::applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items)
{
    ShogiView* const shogiView = currentView();

    QString black = findGameInfoValue(items, { QStringLiteral("先手") });
    QString white = findGameInfoValue(items, { QStringLiteral("後手") });

    const QString shitate = findGameInfoValue(items, { QStringLiteral("下手") });
    const QString uwate   = findGameInfoValue(items, { QStringLiteral("上手") });

    if (black.isEmpty() && !shitate.isEmpty())
        black = shitate;
    if (white.isEmpty() && !uwate.isEmpty())
        white = uwate;

    if (black.isEmpty())
        black = findGameInfoValue(items, { QStringLiteral("先手省略名") });
    if (white.isEmpty())
        white = findGameInfoValue(items, { QStringLiteral("後手省略名") });

    if (shogiView)
        shogiView->setBlackPlayerName(black);
    if (shogiView)
        shogiView->setWhitePlayerName(white);
}

// ============================================================
// データ再構築
// ============================================================

// ============================================================
// 分岐マーカー
// ============================================================

void KifuApplyService::applyBranchMarksForCurrentLine()
{
    if (!m_refs.kifuRecordModel) return;

    QSet<int> marks;

    KifuBranchTree* const branchTree = currentTree();
    KifuNavigationState* const navState = currentNavState();

    const int currentLineIdx = (navState != nullptr) ? navState->currentLineIndex() : 0;
    qCDebug(lcKifu).noquote() << "applyBranchMarksForCurrentLine: currentLineIndex=" << currentLineIdx;

    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchLine> lines = branchTree->allLines();
        const int nLines = static_cast<int>(lines.size());
        const int active = (nLines == 0) ? 0 : qBound(0, currentLineIdx, nLines - 1);
        if (active >= 0 && active < nLines) {
            const BranchLine& line = lines.at(active);
            marks = branchTree->branchablePlysOnLine(line);
        }
        qCDebug(lcKifu).noquote() << "applyBranchMarksForCurrentLine: marks=" << marks;
    } else {
        qCDebug(lcKifu).noquote() << "applyBranchMarksForCurrentLine: no BranchTree available";
    }

    m_refs.kifuRecordModel->setBranchPlyMarks(marks);
}

// ============================================================
// 棋譜解析結果の一括適用フェーズ実装
// ============================================================

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
    // 表示中の手順から作り直す（対局を始めるときは MatchCoordinator::start が手順を受け取る）
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
