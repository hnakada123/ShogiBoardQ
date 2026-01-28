#include "navigationcontextadapter.h"
#include "kifurecordlistmodel.h"
#include "recordnavigationcontroller.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "kifutypes.h"  // ResolvedRow

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
    return m_deps.activeResolvedRow ? *m_deps.activeResolvedRow : 0;
}

int NavigationContextAdapter::maxPlyAtRow(int row) const
{
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

    RecordNavigationController* navCtrl = m_deps.recordNavController ? *m_deps.recordNavController : nullptr;
    if (navCtrl) {
        navCtrl->applySelect(row, ply);
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
}
