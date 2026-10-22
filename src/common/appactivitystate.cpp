/// @file appactivitystate.cpp
/// @brief 動作状態とその持ち主の実装

#include "appactivitystate.h"

#include <utility>

// ============================================================
// ActivityLease
// ============================================================

ActivityLease::ActivityLease(ActivityLease&& other) noexcept
    : m_state(std::exchange(other.m_state, nullptr))
    , m_token(std::exchange(other.m_token, 0))
{
}

ActivityLease& ActivityLease::operator=(ActivityLease&& other) noexcept
{
    if (this != &other) {
        reset();
        m_state = std::exchange(other.m_state, nullptr);
        m_token = std::exchange(other.m_token, 0);
    }
    return *this;
}

bool ActivityLease::isCurrent() const
{
    return m_state && m_token != 0 && m_state->m_token == m_token;
}

void ActivityLease::reset()
{
    if (AppActivityState* state = m_state.data()) state->release(m_token);
    m_state.clear();
    m_token = 0;
}

// ============================================================
// AppActivityState
// ============================================================

ActivityLease AppActivityState::begin(Activity activity, QObject* owner)
{
    const QPointer<QObject> previous = std::exchange(m_owner, owner);
    m_token = m_nextToken++;
    const quint64 token = m_token;
    setActivity(activity);
    // 始めた部品の準備が済んでから知らせる（前の持ち主が自分のリースを手放しても、新しい状態は変わらない）
    if (previous && previous != owner) {
        m_revokedOwners.append(previous);
        QMetaObject::invokeMethod(this, &AppActivityState::notifyRevoked, Qt::QueuedConnection);
    }
    return ActivityLease(this, token);
}

void AppActivityState::notifyRevoked()
{
    const QList<QPointer<QObject>> owners = std::exchange(m_revokedOwners, {});
    for (const QPointer<QObject>& owner : owners) {
        if (owner) emit revoked(owner);
    }
}

void AppActivityState::release(quint64 token)
{
    if (token == 0 || token != m_token) return;
    m_token = 0;
    m_owner.clear();
    setActivity(Activity::Idle);
}

void AppActivityState::setActivity(Activity next)
{
    if (m_activity == next) return;
    m_activity = next;
    emit changed();
}
