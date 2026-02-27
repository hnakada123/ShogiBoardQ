/// @file kifuapplyservice.cpp
/// @brief 棋譜解析結果のモデル適用サービスの実装

#include "kifuapplyservice.h"
#include "sfenpositiontracer.h"
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
#include <QDockWidget>
#include <QTabWidget>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QElapsedTimer>

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
// 解析結果の一括適用
// ============================================================

void KifuApplyService::applyParsedResult(
    const QString& filePath,
    const QString& initialSfen,
    const QString& teaiLabel,
    const KifParseResult& res,
    const QString& parseWarn,
    const char* callerTag)
{
    // パフォーマンス計測用
    QElapsedTimer totalTimer;
    totalTimer.start();
    QElapsedTimer stepTimer;
    auto logStep = [&](const char* stepName) {
        qCDebug(lcKifu).noquote() << QStringLiteral("%1: %2 ms").arg(stepName).arg(stepTimer.elapsed());
        stepTimer.restart();
    };
    stepTimer.start();

    // ローカルエイリアス（二重ポインタの展開）
    QStringList* sfenHistory = *m_refs.sfenHistory;
    KifuBranchTree*& branchTree = *m_refs.branchTree;
    KifuNavigationState* navState = *m_refs.navState;
    BranchTreeManager* branchTreeManager = *m_refs.branchTreeManager;

    // 本譜（表示／USI）
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    *m_refs.kifuUsiMoves = res.mainline.usiMoves;

    // 終局/中断判定（見た目文字列で簡易判定）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) {
            if (s.contains(kw)) return true;
        }
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    // disp[0]は開始局面エントリなので、指し手が1つもない場合は disp.size() <= 1
    if (m_refs.kifuUsiMoves->isEmpty() && !hasTerminal && disp.size() <= 1) {
        const QString errorMessage =
            tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(filePath);
        if (m_hooks.errorOccurred) m_hooks.errorOccurred(errorMessage);
        qCDebug(lcKifu).noquote() << callerTag << "OUT (no moves)";
        *m_refs.loadingKifu = false;
        return;
    }

    // 3) 本譜の SFEN 列と gameMoves を再構築
    qCDebug(lcKifu).noquote() << "applyParsedResult: calling rebuildSfenRecord"
                             << "initialSfen=" << initialSfen.left(60)
                             << "usiMoves.size=" << m_refs.kifuUsiMoves->size()
                             << "hasTerminal=" << hasTerminal;
    rebuildSfenRecord(initialSfen, *m_refs.kifuUsiMoves, hasTerminal);
    sfenHistory = *m_refs.sfenHistory; // rebuildSfenRecord may reallocate
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
    logStep("rebuildSfenRecord");
    rebuildGameMoves(initialSfen, *m_refs.kifuUsiMoves);
    logStep("rebuildGameMoves");

    // 3.5) USI position コマンド列を構築（0..N）
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

    logStep("buildPositionStrList");

    // 4) 棋譜表示へ反映（本譜）
    if (m_hooks.displayGameRecord) m_hooks.displayGameRecord(disp);
    logStep("displayGameRecord");

    // 5) 本譜スナップショットを保持
    *m_refs.dispMain = disp;
    *m_refs.sfenMain = *sfenHistory;
    *m_refs.gmMain = *m_refs.gameMoves;

    // 6) 変化を取りまとめ
    m_refs.variationsByPly->clear();
    m_refs.variationsSeq->clear();
    for (const KifVariation& kv : std::as_const(res.variations)) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;
        if (L.disp.isEmpty()) continue;
        (*m_refs.variationsByPly)[L.startPly].push_back(L);
        m_refs.variationsSeq->push_back(L);
    }

    // 7) 棋譜テーブルの初期選択
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

    // 8) 初期状態を適用
    if (navState != nullptr) {
        navState->goToRoot();
    }
    *m_refs.activePly = 0;

    // 9) UIの整合
    if (m_hooks.enableArrowButtons) m_hooks.enableArrowButtons();
    logImportSummary(filePath, *m_refs.kifuUsiMoves, disp, teaiLabel, parseWarn, QString());

    // 10) KifuBranchTree を構築
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
        if (m_hooks.branchTreeBuilt) m_hooks.branchTreeBuilt();
    }
    logStep("buildKifuBranchTree");

    // 分岐マーカー反映
    applyBranchMarksForCurrentLine();
    logStep("applyBranchMarksForCurrentLine");

    // 12) 分岐ツリーへ供給
    if (branchTreeManager && branchTree != nullptr && !branchTree->isEmpty()) {
        QVector<BranchTreeManager::ResolvedRowLite> rows;
        QVector<BranchLine> lines = branchTree->allLines();
        rows.reserve(lines.size());

        for (int i = 0; i < lines.size(); ++i) {
            const BranchLine& line = lines.at(i);
            BranchTreeManager::ResolvedRowLite x;
            x.startPly = line.branchPly;
            x.parent = (line.branchPoint != nullptr)
                           ? branchTree->findLineIndexForNode(line.branchPoint)
                           : -1;

            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                item.prettyMove = node->displayText();
                item.comment = node->comment();
                item.timeText = node->timeText();
                item.ply = node->ply();
                x.disp.append(item);
                x.sfen.append(node->sfen());
            }

            rows.push_back(std::move(x));
        }

        branchTreeManager->setBranchTreeRows(rows);
        branchTreeManager->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }
    logStep("setBranchTreeRows");

    if (m_refs.kifuBranchModel) {
        m_refs.kifuBranchModel->clearBranchCandidates();
        m_refs.kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view =
            m_refs.recordPane ? m_refs.recordPane->branchView() : nullptr) {
            view->setVisible(true);
            view->setEnabled(false);
        }
    }

    // ロード完了 → 抑止解除
    *m_refs.loadingKifu = false;

    qCDebug(lcKifu).noquote() << QStringLiteral("applyParsedResult TOTAL: %1 ms").arg(totalTimer.elapsed());
    qCDebug(lcKifu).noquote() << callerTag << "OUT";
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

    // ローカルエイリアス
    QStringList* sfenHistory = *m_refs.sfenHistory;
    KifuBranchTree*& branchTree = *m_refs.branchTree;
    KifuNavigationState* navState = *m_refs.navState;

    *m_refs.loadingKifu = true;

    // sfenRecordをクリアして初期局面をセット
    if (sfenHistory) {
        sfenHistory->clear();
        sfenHistory->append(sfen);
    }

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
    if (branchTree == nullptr) {
        branchTree = new KifuBranchTree(m_refs.treeParent);
    } else {
        branchTree->clear();
    }
    branchTree->setRootSfen(sfen);

    // 各種インデックスをリセット
    if (navState != nullptr) {
        navState->goToRoot();
    }
    *m_refs.activePly = 0;
    *m_refs.currentSelectedPly = 0;
    *m_refs.currentMoveIndex = 0;

    // ゲーム情報をクリア
    if (m_refs.gameInfoTable) {
        m_refs.gameInfoTable->clearContents();
        m_refs.gameInfoTable->setRowCount(0);
    }

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
        if (bodStr.contains(QStringLiteral("後手番"))) {
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
    if (!m_refs.tab) return;
    if (!m_refs.gameInfoTable) {
        qCDebug(lcKifu).noquote() << "addGameInfoTabIfMissing: gameInfoTable is null, skipping";
        return;
    }

    QDockWidget* gameInfoDock = *m_refs.gameInfoDock;

    // Dock で表示していたら解除
    if (gameInfoDock && gameInfoDock->widget() == m_refs.gameInfoTable) {
        gameInfoDock->setWidget(nullptr);
        gameInfoDock->deleteLater();
        *m_refs.gameInfoDock = nullptr;
    }

    // テーブルの親ウィジェット（コンテナ）を取得
    QWidget* widgetToAdd = m_refs.gameInfoTable;
    if (m_refs.gameInfoTable && m_refs.gameInfoTable->parentWidget()) {
        QWidget* parent = m_refs.gameInfoTable->parentWidget();
        if (!qobject_cast<QTabWidget*>(parent)) {
            widgetToAdd = parent;
        }
    }

    // 既にタブに含まれているか確認
    if (m_refs.tab->indexOf(widgetToAdd) != -1) {
        return;
    }

    // タブタイトルで検索
    for (int i = 0; i < m_refs.tab->count(); ++i) {
        if (m_refs.tab->tabText(i) == tr("対局情報")) {
            m_refs.tab->removeTab(i);
            m_refs.tab->insertTab(i, widgetToAdd, tr("対局情報"));
            return;
        }
    }

    // タブがない場合のみ追加
    int anchorIdx = -1;

    for (int i = 0; i < m_refs.tab->count(); ++i) {
        const QString t = m_refs.tab->tabText(i);
        if (t.contains(tr("思考")) || t.contains("Thinking", Qt::CaseInsensitive)) {
            anchorIdx = i;
            break;
        }
    }

    if (anchorIdx < 0) {
        for (int i = 0; i < m_refs.tab->count(); ++i) {
            const QString t = m_refs.tab->tabText(i);
            if (t.contains(tr("コメント")) || t.contains("Comments", Qt::CaseInsensitive)) {
                anchorIdx = i;  // NOLINT(clang-analyzer-deadcode.DeadStores)
                break;
            }
        }
    }

    m_refs.tab->insertTab(0, widgetToAdd, tr("対局情報"));
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
    ShogiView* shogiView = *m_refs.shogiView;

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

    if (!black.isEmpty() && shogiView)
        shogiView->setBlackPlayerName(black);
    if (!white.isEmpty() && shogiView)
        shogiView->setWhitePlayerName(white);
}

// ============================================================
// データ再構築
// ============================================================

void KifuApplyService::rebuildSfenRecord(const QString& initialSfen,
                                          const QStringList& usiMoves,
                                          bool hasTerminal)
{
    QStringList*& sfenHistory = *m_refs.sfenHistory;

    qCDebug(lcKifu).noquote() << "rebuildSfenRecord ENTER"
                             << "initialSfen=" << initialSfen.left(60)
                             << "usiMoves.size=" << usiMoves.size()
                             << "hasTerminal=" << hasTerminal
                             << "sfenHistory*=" << static_cast<const void*>(sfenHistory);

    const QStringList list = SfenPositionTracer::buildSfenRecord(initialSfen, usiMoves, hasTerminal);

    qCDebug(lcKifu).noquote() << "rebuildSfenRecord: built list.size=" << list.size();
    if (!list.isEmpty()) {
        qCDebug(lcKifu).noquote() << "rebuildSfenRecord: head[0]=" << list.first().left(60);
        if (list.size() > 1) {
            qCDebug(lcKifu).noquote() << "rebuildSfenRecord: tail[last]=" << list.last().left(60);
        }
    }

    if (!sfenHistory) {
        qCWarning(lcKifu) << "rebuildSfenRecord: sfenHistory was NULL! Creating new QStringList.";
        sfenHistory = new QStringList;
    }
    *sfenHistory = list;

    qCDebug(lcKifu).noquote() << "rebuildSfenRecord LEAVE"
                             << "sfenHistory*=" << static_cast<const void*>(sfenHistory)
                             << "sfenHistory->size=" << sfenHistory->size();
}

void KifuApplyService::rebuildGameMoves(const QString& initialSfen,
                                         const QStringList& usiMoves)
{
    *m_refs.gameMoves = SfenPositionTracer::buildGameMoves(initialSfen, usiMoves);
}

// ============================================================
// 分岐マーカー
// ============================================================

void KifuApplyService::applyBranchMarksForCurrentLine()
{
    if (!m_refs.kifuRecordModel) return;

    QSet<int> marks;

    KifuBranchTree* branchTree = *m_refs.branchTree;
    KifuNavigationState* navState = *m_refs.navState;

    const int currentLineIdx = (navState != nullptr) ? navState->currentLineIndex() : 0;
    qCDebug(lcKifu).noquote() << "applyBranchMarksForCurrentLine: currentLineIndex=" << currentLineIdx;

    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QVector<BranchLine> lines = branchTree->allLines();
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
// ログ出力
// ============================================================

void KifuApplyService::logImportSummary(const QString& filePath,
                                         const QStringList& usiMoves,
                                         const QList<KifDisplayItem>& disp,
                                         const QString& teaiLabel,
                                         const QString& warnParse,
                                         const QString& warnConvert) const
{
    if (!warnParse.isEmpty())
        qCWarning(lcKifu).noquote() << "parse warnings:\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qCWarning(lcKifu).noquote() << "convert warnings:\n" << warnConvert.trimmed();

    qCDebug(lcKifu).noquote() << QStringLiteral("KIF読込: %1手（%2）")
                                     .arg(usiMoves.size())
                                     .arg(QFileInfo(filePath).fileName());
    for (qsizetype i = 0; i < qMin(qsizetype(5), usiMoves.size()); ++i) {
        qCDebug(lcKifu).noquote() << QStringLiteral("USI[%1]: %2")
        .arg(i + 1)
            .arg(usiMoves.at(i));
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("手合割: %1")
                                     .arg(teaiLabel.isEmpty()
                                              ? QStringLiteral("平手(既定)")
                                              : teaiLabel);

    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
        ? QStringLiteral("00:00/00:00:00")
        : it.timeText;
        qCDebug(lcKifu).noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qCDebug(lcKifu).noquote() << QStringLiteral("  └ コメント: %1")
                                             .arg(it.comment.trimmed());
        }
    }

    QStringList* sfenHistory = *m_refs.sfenHistory;
    if (sfenHistory) {
        for (int i = 0; i < qMin(12, sfenHistory->size()); ++i) {
            qCDebug(lcKifu).noquote() << QStringLiteral("%1) %2")
            .arg(i)
                .arg(sfenHistory->at(i));
        }
    }

    qCDebug(lcKifu) << "gameMoves size:" << m_refs.gameMoves->size();
    for (qsizetype i = 0; i < m_refs.gameMoves->size(); ++i) {
        qCDebug(lcKifu).noquote() << QString("%1) ").arg(i + 1) << (*m_refs.gameMoves)[i];
    }
}
