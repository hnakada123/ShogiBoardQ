/// @file considerationpositionservice.cpp
/// @brief 検討モードでの局面解決サービスの実装

#include "considerationpositionservice.h"

#include "considerationpositionresolver.h"
#include "kifunavigationstate.h"
#include "engineanalysistab.h"
#include "considerationtabmanager.h"
#include "logcategories.h"
#include "matchcoordinator.h"
#include "playmode.h"

ConsiderationPositionService::ConsiderationPositionService(QObject* parent)
    : QObject(parent)
{
}

void ConsiderationPositionService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void ConsiderationPositionService::handleBuildPositionRequired(int row)
{
    qCDebug(lcApp).noquote() << "ConsiderationPositionService::handleBuildPositionRequired ENTER row=" << row
                             << "playMode=" << (m_deps.playMode ? static_cast<int>(*m_deps.playMode) : -1)
                             << "match=" << (m_deps.match ? "valid" : "null");

    if (!m_deps.playMode || *m_deps.playMode != PlayMode::ConsiderationMode || !m_deps.match) {
        qCDebug(lcApp).noquote()
            << "ConsiderationPositionService::handleBuildPositionRequired:"
               " not in consideration mode or no match, returning";
        return;
    }

    // 表示中の手順（分岐ツリー）から局面と直前の手を決める
    const RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
    const ConsiderationPositionResolver::UpdateParams params = ConsiderationPositionResolver::resolve(
        line, row, m_deps.startSfenStr ? *m_deps.startSfenStr : QString());
    qCDebug(lcApp).noquote()
        << "ConsiderationPositionService::handleBuildPositionRequired: newPosition=" << params.position;

    if (params.position.isEmpty()) {
        return;
    }

    if (m_deps.match->updateConsiderationPosition(
            params.position,
            params.previousFileTo,
            params.previousRankTo,
            params.lastUsiMove)) {
        // ポジションが変更された場合、経過時間タイマーをリセットして再開
        if (m_deps.analysisTab) {
            m_deps.analysisTab->considerationTabManager()->startElapsedTimer();
        }
    }
}
