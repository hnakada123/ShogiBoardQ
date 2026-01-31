#include "navigationcontextadapter.h"
#include "kifurecordlistmodel.h"
#include "kifuloadcoordinator.h"
#include "recordnavigationcontroller.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "kifutypes.h"  // ResolvedRow
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"

#include <QTableView>
#include <QAbstractItemModel>
#include <QDebug>

NavigationContextAdapter::NavigationContextAdapter(QObject* parent)
    : QObject(parent)
{
}

void NavigationContextAdapter::setDeps(const Deps& deps)
{
    m_deps = deps;
}

void NavigationContextAdapter::setCallbacks(const Callbacks& callbacks)
{
    m_callbacks = callbacks;
}

bool NavigationContextAdapter::hasResolvedRows() const
{
    return m_deps.resolvedRows && !m_deps.resolvedRows->isEmpty();
}

int NavigationContextAdapter::resolvedRowCount() const
{
    return m_deps.resolvedRows ? static_cast<int>(m_deps.resolvedRows->size()) : 0;
}

int NavigationContextAdapter::activeResolvedRow() const
{
    // ★ 新分岐システムが有効で、分岐ラインにいる場合はそのラインインデックスを返す
    // 注意: isOnMainLine()はノードごとの判定なので、currentLineIndex() > 0 で分岐判定する
    KifuNavigationState* navState = m_deps.navState ? *m_deps.navState : nullptr;
    if (navState != nullptr && navState->currentLineIndex() > 0) {
        const int lineIndex = navState->currentLineIndex();
        qDebug().noquote() << "[NavCtxAdapter] activeResolvedRow: using navState lineIndex=" << lineIndex;
        return lineIndex;
    }

    return m_deps.activeResolvedRow ? *m_deps.activeResolvedRow : 0;
}

int NavigationContextAdapter::maxPlyAtRow(int row) const
{
    // ★ 新分岐システムが有効で、分岐ラインにいる場合はそのラインのノード数を使用
    // 注意: isOnMainLine()はノードごとの判定なので、currentLineIndex() > 0 で分岐判定する
    KifuNavigationState* navState = m_deps.navState ? *m_deps.navState : nullptr;
    KifuBranchTree* branchTree = m_deps.branchTree ? *m_deps.branchTree : nullptr;
    if (navState != nullptr && branchTree != nullptr && navState->currentLineIndex() > 0) {
        QVector<BranchLine> lines = branchTree->allLines();
        const int lineIndex = navState->currentLineIndex();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            // ノード数 - 1 = 最大ply (開始局面が ply=0)
            const int maxPly = static_cast<int>(lines.at(lineIndex).nodes.size()) - 1;
            qDebug().noquote() << "[NavCtxAdapter] maxPlyAtRow (branch): row=" << row
                               << "lineIndex=" << lineIndex << "maxPly=" << maxPly;
            return maxPly;
        }
    }

    // ★ ライブ対局中の場合は、棋譜モデルの行数を使用
    KifuLoadCoordinator* kifuCoord = m_deps.kifuLoadCoordinator ? *m_deps.kifuLoadCoordinator : nullptr;
    if (kifuCoord && kifuCoord->isLiveGameActive()) {
        KifuRecordListModel* kifuModel = m_deps.kifuRecordModel ? *m_deps.kifuRecordModel : nullptr;
        const int kifuMax = (kifuModel && kifuModel->rowCount() > 0)
                                ? (kifuModel->rowCount() - 1)
                                : 0;
        qDebug().noquote() << "[NavCtxAdapter] maxPlyAtRow (liveGameActive): row=" << row
                           << "kifuMax=" << kifuMax;
        return kifuMax;
    }

    if (!m_deps.resolvedRows || m_deps.resolvedRows->isEmpty()) {
        // ライブ（解決済み行なし）のとき：
        // - SFEN: 「開始局面 + 実手数」なので終局行（投了/時間切れ）は含まれない → size()-1
        // - 棋譜欄: 「実手 + 終局行（あれば）」が入る → rowCount()-1
        // 末尾へ進める上限は「どちらか大きい方」を採用する。
        QStringList* sfenRecord = m_deps.sfenRecord ? *m_deps.sfenRecord : nullptr;
        KifuRecordListModel* kifuModel = m_deps.kifuRecordModel ? *m_deps.kifuRecordModel : nullptr;

        const int sfenMax = (sfenRecord && !sfenRecord->isEmpty())
                                ? static_cast<int>(sfenRecord->size() - 1)
                                : 0;
        const int kifuMax = (kifuModel && kifuModel->rowCount() > 0)
                                ? (kifuModel->rowCount() - 1)
                                : 0;
        const int result = qMax(sfenMax, kifuMax);
        qDebug().noquote() << "[NavCtxAdapter] maxPlyAtRow (live): row=" << row
                           << "sfenMax=" << sfenMax << "kifuMax=" << kifuMax
                           << "result=" << result;
        return result;
    }

    // 既に解決済み行がある（棋譜ファイル読み込み後など）のとき：
    // その行に表示するエントリ数（disp.size()）が末尾。
    const int clamped = static_cast<int>(qBound(qsizetype(0), qsizetype(row), m_deps.resolvedRows->size() - 1));
    const int result = static_cast<int>((*m_deps.resolvedRows)[clamped].disp.size());
    qDebug().noquote() << "[NavCtxAdapter] maxPlyAtRow (resolved): row=" << row
                       << "clamped=" << clamped << "result=" << result;
    return result;
}

int NavigationContextAdapter::currentPly() const
{
    // ★ 新分岐システムが有効で、分岐ラインにいる場合はそのplyを使用
    // 注意: isOnMainLine()はノードごとの判定なので、currentLineIndex() > 0 で分岐判定する
    KifuNavigationState* navState = m_deps.navState ? *m_deps.navState : nullptr;
    if (navState != nullptr && navState->currentLineIndex() > 0) {
        const int ply = navState->currentPly();
        qDebug().noquote() << "[NavCtxAdapter] currentPly (branch): ply=" << ply;
        return ply;
    }

    // ★ リプレイ／再開（ライブ追記）中は UI 側のトラッキング値を優先
    ReplayController* replayCtrl = m_deps.replayController ? *m_deps.replayController : nullptr;
    const bool liveAppend = replayCtrl ? replayCtrl->isLiveAppendMode() : false;

    const int currentSelectedPly = m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : -1;
    const int activePly = m_deps.activePly ? *m_deps.activePly : -1;

    qDebug().noquote() << "[NavCtxAdapter] currentPly(): liveAppend=" << liveAppend
                       << "currentSelectedPly=" << currentSelectedPly
                       << "activePly=" << activePly;

    RecordPane* recordPane = m_deps.recordPane ? *m_deps.recordPane : nullptr;

    if (liveAppend) {
        if (currentSelectedPly >= 0) {
            qDebug().noquote() << "[NavCtxAdapter] currentPly() returning currentSelectedPly=" << currentSelectedPly;
            return currentSelectedPly;
        }

        // 念のためビューの currentIndex もフォールバックに
        const QTableView* view = recordPane ? recordPane->kifuView() : nullptr;
        if (view) {
            const QModelIndex cur = view->currentIndex();
            if (cur.isValid()) {
                int result = qMax(0, cur.row());
                qDebug().noquote() << "[NavCtxAdapter] currentPly() (liveAppend) returning view.currentIndex.row=" << result;
                return result;
            }
        }
        qDebug().noquote() << "[NavCtxAdapter] currentPly() (liveAppend) returning 0 (fallback)";
        return 0;
    }

    // 通常時は従来通り activePly を優先
    if (activePly >= 0) {
        qDebug().noquote() << "[NavCtxAdapter] currentPly() returning activePly=" << activePly;
        return activePly;
    }

    const QTableView* view = recordPane ? recordPane->kifuView() : nullptr;
    if (view) {
        const QModelIndex cur = view->currentIndex();
        if (cur.isValid()) {
            int result = qMax(0, cur.row());
            qDebug().noquote() << "[NavCtxAdapter] currentPly() returning view.currentIndex.row=" << result;
            return result;
        }
    }
    qDebug().noquote() << "[NavCtxAdapter] currentPly() returning 0 (final fallback)";
    return 0;
}

void NavigationContextAdapter::applySelect(int row, int ply)
{
    if (m_callbacks.ensureRecordNavController) {
        m_callbacks.ensureRecordNavController();
    }

    // ★ 新分岐システムが有効で、分岐ラインにいる場合は新システムでナビゲート
    // 注意: isOnMainLine()はノードごとの判定なので、currentLineIndex() > 0 で分岐判定する
    KifuNavigationState* navState = m_deps.navState ? *m_deps.navState : nullptr;
    KifuNavigationController* kifuNavCtrl = m_deps.kifuNavController ? *m_deps.kifuNavController : nullptr;
    KifuBranchTree* branchTree = m_deps.branchTree ? *m_deps.branchTree : nullptr;

    if (navState != nullptr && kifuNavCtrl != nullptr && branchTree != nullptr && navState->currentLineIndex() > 0) {
        qDebug().noquote() << "[NavCtxAdapter] applySelect (branch): row=" << row
                           << "ply=" << ply << "lineIndex=" << navState->currentLineIndex();

        // 現在のライン上でplyに対応するノードを探す
        QVector<BranchLine> lines = branchTree->allLines();
        const int lineIndex = navState->currentLineIndex();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                if (node->ply() == ply) {
                    kifuNavCtrl->goToNode(node);
                    qDebug().noquote() << "[NavCtxAdapter] applySelect (branch): goToNode ply=" << ply;
                    return;
                }
            }
        }
        qDebug().noquote() << "[NavCtxAdapter] applySelect (branch): node not found for ply=" << ply;
        return;
    }

    // ★ ライブ対局中は行（row）を無視して、単純にplyナビゲーションを行う
    KifuLoadCoordinator* kifuCoord = m_deps.kifuLoadCoordinator ? *m_deps.kifuLoadCoordinator : nullptr;
    const bool liveGameActive = kifuCoord && kifuCoord->isLiveGameActive();

    if (liveGameActive) {
        qDebug().noquote() << "[NavCtxAdapter] applySelect (liveGameActive): ignoring row=" << row
                           << "using ply=" << ply;
        // ライブ対局中はresolvedRowsを経由せず、直接棋譜ビューをナビゲート
        RecordNavigationController* navCtrl = m_deps.recordNavController ? *m_deps.recordNavController : nullptr;
        if (navCtrl) {
            navCtrl->navigateKifuViewToRow(ply);
        }
    } else {
        RecordNavigationController* navCtrl = m_deps.recordNavController ? *m_deps.recordNavController : nullptr;
        if (navCtrl) {
            navCtrl->applySelect(row, ply);
        }
    }

    // currentSfenStrを選択した局面に更新
    QStringList* sfenRecord = m_deps.sfenRecord ? *m_deps.sfenRecord : nullptr;
    if (sfenRecord && m_deps.currentSfenStr && ply >= 0 && ply < sfenRecord->size()) {
        *m_deps.currentSfenStr = sfenRecord->at(ply);
        qDebug() << "[NavCtxAdapter] applySelect: row=" << row << "ply=" << ply << "sfen=" << *m_deps.currentSfenStr;
    }

    // 定跡ウィンドウを更新
    if (m_callbacks.updateJosekiWindow) {
        m_callbacks.updateJosekiWindow();
    }

    // 新システムに位置変更を通知（SFENも含める）
    QString sfen;
    if (sfenRecord && ply >= 0 && ply < sfenRecord->size()) {
        sfen = sfenRecord->at(ply);
    }
    qDebug().noquote() << "[NCA] applySelect: emitting positionChanged row=" << row << "ply=" << ply
                       << "sfen=" << sfen.left(50);
    emit positionChanged(row, ply, sfen);
    qDebug().noquote() << "[NCA] applySelect: positionChanged emitted, returning";
}
