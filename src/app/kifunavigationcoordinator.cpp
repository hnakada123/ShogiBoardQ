/// @file kifunavigationcoordinator.cpp
/// @brief 棋譜ナビゲーション同期コーディネータの実装

#include "kifunavigationcoordinator.h"
#include "josekiwindowwiring.h"

#include "shogiview.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "matchcoordinator.h"
#include "engineanalysistab.h"
#include "considerationtabmanager.h"
#include "uistatepolicymanager.h"
#include "playmode.h"
#include "logcategories.h"

KifuNavigationCoordinator::KifuNavigationCoordinator(QObject* parent)
    : QObject(parent)
{
}

void KifuNavigationCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void KifuNavigationCoordinator::navigateToRow(int ply)
{
    if (!m_deps.navController || !m_deps.navState) return;
    if (m_deps.shogiView && m_deps.shogiView->positionEditMode()) return;
    m_deps.navController->goToLine(m_deps.navState->currentLineIndex(), ply);
}

void KifuNavigationCoordinator::syncLoadedPositionAtRow(int ply)
{
    navigateToRow(ply);
}

// `handleBranchNodeHandled`: 分岐ノード処理完了後の盤面・ハイライト更新。
void KifuNavigationCoordinator::handleBranchNodeHandled(
    int ply, const QString& sfen,
    int previousFileTo, int previousRankTo,
    const QString& lastUsiMove)
{
    qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled ENTER ply=" << ply
                       << "sfen=" << sfen
                       << "fileTo=" << previousFileTo << "rankTo=" << previousRankTo
                       << "usiMove=" << lastUsiMove
                       << "playMode=" << (m_deps.playMode ? static_cast<int>(*m_deps.playMode) : -1)
                       << "match=" << (m_deps.match ? "valid" : "null");

    // currentSfenStr を更新
    if (!sfen.isEmpty() && m_deps.currentSfenStr) {
        *m_deps.currentSfenStr = sfen;
    }

    if (m_deps.uiStatePolicy) m_deps.uiStatePolicy->enableNavigationIfAllowed();

    // 定跡ウィンドウを更新
    if (m_deps.josekiWiring) m_deps.josekiWiring->updateJosekiWindow();

    // 検討モード時はエンジンに新しい局面を送信
    if (m_deps.playMode && *m_deps.playMode == PlayMode::ConsiderationMode
        && m_deps.match && !sfen.isEmpty()) {
        const QString newPosition = QStringLiteral("position sfen ") + sfen;
        qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: sending to engine:" << newPosition;
        if (m_deps.match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
            qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned true";
            if (m_deps.analysisTab) {
                m_deps.analysisTab->considerationTabManager()->startElapsedTimer();
            }
        } else {
            qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned false (same position or not in consideration)";
        }
    } else {
        qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: NOT in consideration mode or match/sfen missing";
    }
    qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled LEAVE";
}

void KifuNavigationCoordinator::selectKifuRow(int row)
{
    navigateToRow(row);
}

void KifuNavigationCoordinator::syncNavStateToPly(int ply)
{
    if (!m_deps.navState || m_deps.navState->currentPly() == ply) return;
    navigateToRow(ply);
}
