/// @file kifuapplyservice.cpp
/// @brief 棋譜解析結果のモデル適用サービスの実装

#include "kifuapplyservice.h"
#include "kifuapplylogger.h"
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

bool KifuApplyService::ensureSfenHistoryStorage(const char* callerTag) const
{
    if (m_refs.sfenHistory && *m_refs.sfenHistory) {
        return true;
    }

    qCCritical(lcKifu).noquote() << callerTag << ": sfenHistory storage is not initialized";
    if (m_hooks.errorOccurred) {
        m_hooks.errorOccurred(tr("内部エラー: SFEN履歴の格納先が初期化されていません。"));
    }
    return false;
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

    if (!ensureSfenHistoryStorage("loadPositionFromSfen")) {
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

bool KifuApplyService::rebuildSfenRecord(const QString& initialSfen,
                                         const QStringList& usiMoves,
                                         bool hasTerminal)
{
    if (!ensureSfenHistoryStorage("rebuildSfenRecord")) {
        return false;
    }

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

    *sfenHistory = list;

    qCDebug(lcKifu).noquote() << "rebuildSfenRecord LEAVE"
                             << "sfenHistory*=" << static_cast<const void*>(sfenHistory)
                             << "sfenHistory->size=" << sfenHistory->size();
    return true;
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
