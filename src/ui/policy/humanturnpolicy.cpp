/// @file humanturnpolicy.cpp
/// @brief 盤を人間が操作できるか・棋譜を移って・変えてよいか・対局が進行中かの判定の実装

#include "humanturnpolicy.h"

#include "appactivitystate.h"
#include "gamechannels.h"
#include "livegamesession.h"
#include "uistatepolicymanager.h"

HumanTurnPolicy::BoardInput HumanTurnPolicy::boardInput() const
{
    if (m_disabled || !m_deps.activity) return BoardInput::None;
    switch (m_deps.activity->activity()) {
    case AppActivityState::Activity::DuringPositionEdit:
        return BoardInput::Edit;
    case AppActivityState::Activity::Idle:
        // 対局していない待機中は、盤上の着手を表示中の手の次の手（変化）として記録する。
        // 終局後も同じ扱いで、指し継いだ手は分岐ツリーに変化として加わる。
        return canEditRecord() ? BoardInput::Variation : BoardInput::None;
    case AppActivityState::Activity::DuringGame:
    case AppActivityState::Activity::DuringCsaGame: {
        const GameChannel* channel = m_deps.channels ? m_deps.channels->active() : nullptr;
        return channel && channel->isHumanToMove() ? BoardInput::Game : BoardInput::None;
    }
    default:
        return BoardInput::None;
    }
}

bool HumanTurnPolicy::canBrowseRecord() const
{
    if (m_disabled || !m_deps.activity) return false;
    // 移れる状態は UI の表（局面送りのボタン・棋譜欄・評価値グラフを押せる状態）と同じ1か所で決める
    return UiStatePolicyManager::tablePolicy(m_deps.activity->activity(), UiStatePolicyManager::UiElement::WidgetNavigation)
        == UiStatePolicyManager::Policy::Enabled;
}

bool HumanTurnPolicy::canSaveRecord() const
{
    if (m_disabled || !m_deps.activity) return false;
    return UiStatePolicyManager::tablePolicy(m_deps.activity->activity(), UiStatePolicyManager::UiElement::FileSaveAs)
        == UiStatePolicyManager::Policy::Enabled;
}

bool HumanTurnPolicy::canEditRecord() const
{
    // 対局の棋譜を書いている間は、ツリーに手を足している最中なので変えない。通信対局の局と局の間は見るだけ
    // （棋譜は次の対局で置き換わる）
    return canBrowseRecord() && !(m_deps.liveSession && m_deps.liveSession->isActive())
        && !m_deps.activity->is(AppActivityState::Activity::DuringCsaConnection);
}

bool HumanTurnPolicy::isHumanTurnNow() const
{
    const BoardInput input = boardInput();
    return input == BoardInput::Variation || input == BoardInput::Game;
}

bool HumanTurnPolicy::isGameActivelyInProgress() const
{
    const GameChannel* channel = m_deps.channels ? m_deps.channels->active() : nullptr;
    return channel && channel->isInProgress();
}
