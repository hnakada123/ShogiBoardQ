/// @file enginelifecyclemanager.cpp
/// @brief エンジンの枠のリースと取り上げの知らせの実装

#include "enginelifecyclemanager.h"

#include "logcategories.h"
#include "usi.h"

#include <algorithm>
#include <utility>

EngineLifecycleManager::EngineLifecycleManager(QObject* parent)
    : QObject(parent)
{
}

EngineLifecycleManager::~EngineLifecycleManager() = default;

// ============================================================
// リース
// ============================================================

EngineLease::EngineLease(EngineLifecycleManager* manager, Usi* engine)
    : m_manager(manager)
    , m_engine(engine)
{
}

EngineLease::~EngineLease()
{
    reset();
}

EngineLease::EngineLease(EngineLease&& other) noexcept
    : m_manager(std::exchange(other.m_manager, nullptr))
    , m_engine(std::exchange(other.m_engine, nullptr))
{
}

EngineLease& EngineLease::operator=(EngineLease&& other) noexcept
{
    if (this != &other) {
        reset();
        m_manager = std::exchange(other.m_manager, nullptr);
        m_engine = std::exchange(other.m_engine, nullptr);
    }
    return *this;
}

Usi* EngineLease::engine() const
{
    // 取り上げられたエンジンは枠から外れている（破棄は後で行われるので、ポインタだけでは分からない）
    return m_manager && m_engine && m_manager->holds(m_engine) ? m_engine.data() : nullptr;
}

void EngineLease::reset()
{
    if (Usi* engine = this->engine()) m_manager->release(engine);
    m_manager.clear();
    m_engine.clear();
}

EngineLease EngineLifecycleManager::acquire(QObject* owner, int slot, const QString& engineName)
{
    Seat& target = seat(slot);
    destroySeat(target, false, target.owner != owner);

    const QString displayName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    auto* engine = new Usi(this);
    engine->setLogIdentity(slot == 2 ? QStringLiteral("E2") : QStringLiteral("E1"), QString(), displayName);
    target.engine = engine;
    target.owner = owner;
    emit enginesChanged();
    return EngineLease(this, engine);
}

Usi* EngineLifecycleManager::engineInSlot(int slot) const
{
    return seat(slot).engine;
}

// ============================================================
// 破棄
// ============================================================

void EngineLifecycleManager::destroySeat(Seat& target, bool clearThinking, bool notifyOwner)
{
    Usi* engine = std::exchange(target.engine, nullptr);
    const QPointer<QObject> owner = std::exchange(target.owner, nullptr);
    if (!engine) return;
    disconnect(engine, nullptr, this, nullptr);
    engine->quit(clearThinking);
    engine->deleteLater();
    emit enginesChanged();
    if (notifyOwner && owner) emit engineRevoked(owner);
}

void EngineLifecycleManager::release(Usi* engine)
{
    for (Seat& s : m_seats) {
        if (engine && s.engine == engine) destroySeat(s, false, false);
    }
}

bool EngineLifecycleManager::holds(const Usi* engine) const
{
    return engine && std::any_of(m_seats.cbegin(), m_seats.cend(), [engine](const Seat& s) { return s.engine == engine; });
}

void EngineLifecycleManager::destroyEngines(bool clearDisplay)
{
    qCDebug(lcGame).noquote() << "destroyEngines called, clearDisplay=" << clearDisplay;
    for (Seat& s : m_seats) destroySeat(s, clearDisplay, true);
    if (clearDisplay) emit displayClearRequested();
}
