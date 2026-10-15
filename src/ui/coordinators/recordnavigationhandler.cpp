/// @file recordnavigationhandler.cpp
/// @brief 棋譜欄の行選択をナビゲーション要求へ変換する。
#include "recordnavigationhandler.h"
#include "livegamesession.h"
#include "kifunavigationstate.h"
#include "kifudisplaycoordinator.h"

RecordNavigationHandler::RecordNavigationHandler(QObject* parent) : QObject(parent) {}

void RecordNavigationHandler::updateDeps(const Deps& deps) { m_deps = deps; }

void RecordNavigationHandler::onMainRowChanged(int row)
{
    if (row < 0 || !m_deps.navState || !m_deps.displayCoordinator) return;
    // 対局準備・再開も含め、棋譜の更新に伴う行選択を閲覧操作として扱わない。
    if (m_deps.liveSession && m_deps.liveSession->canAddMove()) return;
    m_deps.displayCoordinator->onPositionChanged(m_deps.navState->currentLineIndex(), row, {});
}
