/// @file boardloadservice.cpp
/// @brief SFEN適用/分岐同期の盤面読み込みサービスの実装

#include "boardloadservice.h"

#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "boardsyncpresenter.h"
#include "logcategories.h"

BoardLoadService::BoardLoadService(QObject* parent)
    : QObject(parent)
{
}

void BoardLoadService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void BoardLoadService::loadFromSfen(const QString& sfen)
{
    qCDebug(lcApp).noquote() << "BoardLoadService::loadFromSfen: sfen=" << sfen.left(60);

    if (sfen.isEmpty()) {
        qCWarning(lcApp) << "BoardLoadService::loadFromSfen: empty SFEN, skipping";
        return;
    }

    if (m_deps.gc && m_deps.gc->board() && m_deps.view) {
        // 盤面を設定
        m_deps.gc->board()->setSfen(sfen);
        // ビューに反映
        m_deps.view->applyBoardAndRender(m_deps.gc->board());
        // 手番表示を更新
        if (m_deps.setCurrentTurn) m_deps.setCurrentTurn();
        // 現在のSFEN文字列を更新
        if (m_deps.currentSfenStr) *m_deps.currentSfenStr = sfen;

        qCDebug(lcApp) << "BoardLoadService::loadFromSfen: board updated successfully";
    } else {
        qCWarning(lcApp) << "BoardLoadService::loadFromSfen: missing dependencies"
                         << "gc=" << m_deps.gc
                         << "board=" << (m_deps.gc ? m_deps.gc->board() : nullptr)
                         << "view=" << m_deps.view;
    }
}

void BoardLoadService::loadWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    // 分岐由来の局面切替では通常同期との競合を避けるため、ガードを立てる。
    if (m_deps.beginBranchNavGuard) m_deps.beginBranchNavGuard();

    // 盤面更新とハイライト計算は BoardSyncPresenter に委譲する。
    if (m_deps.ensureBoardSyncPresenter) m_deps.ensureBoardSyncPresenter();
    if (m_deps.boardSync) {
        m_deps.boardSync->loadBoardWithHighlights(currentSfen, prevSfen);
    }

    // 手番表示を更新
    if (!currentSfen.isEmpty()) {
        if (m_deps.setCurrentTurn) m_deps.setCurrentTurn();
        if (m_deps.currentSfenStr) *m_deps.currentSfenStr = currentSfen;
    }
}
