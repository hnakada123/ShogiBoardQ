/// @file considerationpositionservice.cpp
/// @brief 検討モードでの局面解決サービスの実装

#include "considerationpositionservice.h"

#include "considerationpositionresolver.h"
#include "engineanalysistab.h"
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

    ConsiderationPositionResolver::Inputs inputs;
    inputs.positionStrList = m_deps.positionStrList;
    inputs.gameUsiMoves = m_deps.gameUsiMoves;
    inputs.gameMoves = m_deps.gameMoves;
    inputs.startSfenStr = m_deps.startSfenStr;
    inputs.sfenRecord = m_deps.match->sfenRecordPtr();
    inputs.kifuLoadCoordinator = m_deps.kifuLoadCoordinator;
    inputs.kifuRecordModel = m_deps.kifuRecordModel;
    inputs.branchTree = m_deps.branchTree;
    inputs.navState = m_deps.navState;

    const ConsiderationPositionResolver resolver(inputs);
    const ConsiderationPositionResolver::UpdateParams params = resolver.resolveForRow(row);
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
            m_deps.analysisTab->startElapsedTimer();
        }
    }
}
