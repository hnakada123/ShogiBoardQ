/// @file engineslots.cpp
/// @brief エンジンの枠のリースと取り上げの知らせの実装

#include "engineslots.h"

#include "logcategories.h"
#include "usi.h"

#include <algorithm>
#include <utility>

EngineSlots::EngineSlots(QObject* parent)
    : QObject(parent)
{
}

EngineSlots::~EngineSlots() = default;

// ============================================================
// リース
// ============================================================

EngineLease::EngineLease(EngineSlots* engineSlots, Usi* engine)
    : m_engineSlots(engineSlots)
    , m_engine(engine)
{
}

EngineLease::~EngineLease()
{
    reset();
}

EngineLease::EngineLease(EngineLease&& other) noexcept
    : m_engineSlots(std::exchange(other.m_engineSlots, nullptr))
    , m_engine(std::exchange(other.m_engine, nullptr))
{
}

EngineLease& EngineLease::operator=(EngineLease&& other) noexcept
{
    if (this != &other) {
        reset();
        m_engineSlots = std::exchange(other.m_engineSlots, nullptr);
        m_engine = std::exchange(other.m_engine, nullptr);
    }
    return *this;
}

Usi* EngineLease::engine() const
{
    // 取り上げられたエンジンは枠から外れている（破棄は後で行われるので、ポインタだけでは分からない）
    return m_engineSlots && m_engine && m_engineSlots->holds(m_engine) ? m_engine.data() : nullptr;
}

void EngineLease::reset()
{
    if (Usi* engine = this->engine()) m_engineSlots->release(engine);
    m_engineSlots.clear();
    m_engine.clear();
}

EngineLease EngineSlots::acquire(QObject* owner, int slot, const QString& engineName)
{
    Slot& target = slotAt(slot);
    destroySlot(target, false, target.owner != owner);

    const QString displayName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    auto* engine = new Usi(this);
    engine->setLogIdentity(slot == 2 ? QStringLiteral("E2") : QStringLiteral("E1"), QString(), displayName);
    target.engine = engine;
    target.owner = owner;
    emit enginesChanged();
    return EngineLease(this, engine);
}

Usi* EngineSlots::engineInSlot(int slot) const
{
    return slotAt(slot).engine;
}

// ============================================================
// 破棄
// ============================================================

void EngineSlots::destroySlot(Slot& target, bool clearThinking, bool notifyOwner)
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

void EngineSlots::release(Usi* engine)
{
    for (Slot& s : m_slots) {
        if (engine && s.engine == engine) destroySlot(s, false, false);
    }
}

bool EngineSlots::holds(const Usi* engine) const
{
    return engine && std::any_of(m_slots.cbegin(), m_slots.cend(), [engine](const Slot& s) { return s.engine == engine; });
}

void EngineSlots::destroyEngines(bool clearDisplay)
{
    qCDebug(lcEngine).noquote() << "destroyEngines called, clearDisplay=" << clearDisplay;
    for (Slot& s : m_slots) destroySlot(s, clearDisplay, true);
    if (clearDisplay) emit displayClearRequested();
}
