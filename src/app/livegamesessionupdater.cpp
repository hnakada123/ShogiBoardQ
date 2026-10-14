/// @file livegamesessionupdater.cpp
/// @brief LiveGameSession の更新ロジック実装

#include "livegamesessionupdater.h"

#include "livegamesession.h"
#include "kifubranchtree.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "logcategories.h"
#include "sfenutils.h"

void LiveGameSessionUpdater::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void LiveGameSessionUpdater::startSessionIfNeeded()
{
    if (m_deps.liveSession == nullptr || m_deps.branchTree == nullptr) {
        return;
    }

    if (m_deps.liveSession->isActive()) {
        return;  // 既に開始済み
    }

    // ルートノードが必要（performCleanup で作成済みのはず）
    if (m_deps.branchTree->root() == nullptr) {
        qCWarning(lcApp).noquote() << "startSessionIfNeeded: root is null, cannot start session";
        return;
    }

    // 現在の局面が途中局面かどうかを判定
    const QString currentSfen = m_deps.currentSfenStr ? m_deps.currentSfenStr->trimmed() : QString();
    bool isFromMidPosition = !currentSfen.isEmpty() &&
                              currentSfen != QStringLiteral("startpos") &&
                              !SfenUtils::isHirateStart(currentSfen);

    if (isFromMidPosition) {
        // 途中局面から開始: SFENからノードを探す
        KifuBranchNode* branchPoint = m_deps.branchTree->findBySfen(currentSfen);
        if (branchPoint != nullptr) {
            m_deps.liveSession->startFromNode(branchPoint);
            qCDebug(lcApp).noquote() << "startSessionIfNeeded: started from node, ply=" << branchPoint->ply();
        } else {
            // ノードが見つからない場合はルートから開始
            qCWarning(lcApp).noquote() << "startSessionIfNeeded: branchPoint not found for sfen, starting from root";
            m_deps.liveSession->startFromRoot();
        }
    } else {
        // 新規対局: ルートから開始
        m_deps.liveSession->startFromRoot();
        qCDebug(lcApp).noquote() << "startSessionIfNeeded: session started from root";
    }
}

void LiveGameSessionUpdater::appendRecord(const RecordEntry& entry)
{
    startSessionIfNeeded();
    if (m_deps.liveSession && m_deps.liveSession->canAddMove()) m_deps.liveSession->appendRecord(entry);
}
