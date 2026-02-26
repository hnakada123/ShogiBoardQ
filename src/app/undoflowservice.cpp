/// @file undoflowservice.cpp
/// @brief 「待った」巻き戻し後処理の実装

#include "undoflowservice.h"

#include "matchcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "playmode.h"

void UndoFlowService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void UndoFlowService::undoLastTwoMoves()
{
    if (!m_deps.match) {
        return;
    }

    // 2手戻しを実行し、成功した場合は評価値グラフを更新する
    if (!m_deps.match->undoTwoPlies()) {
        return;
    }

    if (!m_deps.evalGraphController || !m_deps.playMode) {
        return;
    }

    // 評価値グラフのプロットを1つ削除（2手で1プロット）
    // ※EvEモードでは「待った」は使用しない想定
    switch (*m_deps.playMode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        // 後手がエンジン → P2を削除
        m_deps.evalGraphController->removeLastP2Score();
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        // 先手がエンジン → P1を削除
        m_deps.evalGraphController->removeLastP1Score();
        break;
    default:
        // その他のモード（EvE、HvH、解析モード等）では何もしない
        break;
    }

    // 縦線（カーソルライン）を現在の手数位置に更新
    // sfenRecordのサイズ - 1 が現在の手数（ply）
    // sfenRecord: [開局SFEN, 1手目後SFEN, 2手目後SFEN, ...]
    // size=1 → ply=0（開局）, size=2 → ply=1, size=3 → ply=2, ...
    const int currentPly = m_deps.sfenRecord
        ? static_cast<int>(qMax(static_cast<qsizetype>(0), m_deps.sfenRecord->size() - 1))
        : 0;
    m_deps.evalGraphController->setCurrentPly(currentPly);
}
