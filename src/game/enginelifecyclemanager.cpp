/// @file enginelifecyclemanager.cpp
/// @brief エンジンライフサイクル管理クラスの実装

#include "enginelifecyclemanager.h"

#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <QDebug>

// ============================================================
// 初期化
// ============================================================

EngineLifecycleManager::EngineLifecycleManager(QObject* parent)
    : QObject(parent)
{
}

void EngineLifecycleManager::setRefs(const Refs& refs) { m_refs = refs; }
void EngineLifecycleManager::setHooks(const Hooks& hooks) { m_hooks = hooks; }

void EngineLifecycleManager::setModelPtrs(UsiCommLogModel* comm1,
                                          ShogiEngineThinkingModel* think1,
                                          UsiCommLogModel* comm2,
                                          ShogiEngineThinkingModel* think2)
{
    m_comm1 = comm1;
    m_think1 = think1;
    m_comm2 = comm2;
    m_think2 = think2;
}

// ============================================================
// エンジンポインタ
// ============================================================

Usi* EngineLifecycleManager::usi1() const { return m_usi1; }
Usi* EngineLifecycleManager::usi2() const { return m_usi2; }
void EngineLifecycleManager::setUsi1(Usi* u) { m_usi1 = u; emit enginesChanged(); }
void EngineLifecycleManager::setUsi2(Usi* u) { m_usi2 = u; emit enginesChanged(); }

void EngineLifecycleManager::updateUsiPtrs(Usi* e1, Usi* e2)
{
    m_usi1 = e1;
    m_usi2 = e2;
    emit enginesChanged();
}

bool EngineLifecycleManager::isShutdownInProgress() const { return m_engineShutdownInProgress; }
void EngineLifecycleManager::setShutdownInProgress(bool v) { m_engineShutdownInProgress = v; }

// ============================================================
// エンジン初期化・シグナル配線
// ============================================================

void EngineLifecycleManager::initializeAndStartEngineFor(Player side,
                                                         const QString& enginePathIn,
                                                         const QString& engineNameIn)
{
    Usi*& eng = (side == EngineLifecycleManager::P1 ? m_usi1 : m_usi2);

    if (!eng) {
        if (m_usi1 && !m_usi2) {
            qCDebug(lcGame) << "[Match] fallback to m_usi1 for HvE";
            eng = m_usi1;
        } else {
            qCWarning(lcGame) << "[Match] engine ptr is null (side=" << (side == EngineLifecycleManager::P1 ? "P1" : "P2") << ")";
            return;
        }
    }

    QString path = enginePathIn;
    QString name = engineNameIn;

    eng->initializeAndStartEngineCommunication(path, name);

    wireResignToArbiter(eng, (side == EngineLifecycleManager::P1));
    wireWinToArbiter(eng, (side == EngineLifecycleManager::P1));
}

void EngineLifecycleManager::wireResignToArbiter(Usi* engine, bool asP1)
{
    if (!engine) return;

    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, nullptr);

    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &EngineLifecycleManager::onEngine1Resign,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &EngineLifecycleManager::onEngine2Resign,
                         Qt::UniqueConnection);
    }
}

void EngineLifecycleManager::wireWinToArbiter(Usi* engine, bool asP1)
{
    if (!engine) return;

    QObject::disconnect(engine, &Usi::bestMoveWinReceived, this, nullptr);

    if (asP1) {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &EngineLifecycleManager::onEngine1Win,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &EngineLifecycleManager::onEngine2Win,
                         Qt::UniqueConnection);
    }
}

void EngineLifecycleManager::onEngine1Resign()
{
    if (m_hooks.onEngineResign) m_hooks.onEngineResign(1);
}

void EngineLifecycleManager::onEngine2Resign()
{
    if (m_hooks.onEngineResign) m_hooks.onEngineResign(2);
}

void EngineLifecycleManager::onEngine1Win()
{
    if (m_hooks.onEngineWin) m_hooks.onEngineWin(1);
}

void EngineLifecycleManager::onEngine2Win()
{
    if (m_hooks.onEngineWin) m_hooks.onEngineWin(2);
}

void EngineLifecycleManager::disconnectArbiterSignals()
{
    disconnectArbiterSignals(m_usi1);
    disconnectArbiterSignals(m_usi2);
}

void EngineLifecycleManager::disconnectArbiterSignals(Usi* engine)
{
    if (!engine) return;
    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, nullptr);
    QObject::disconnect(engine, &Usi::bestMoveWinReceived, this, nullptr);
}

// ============================================================
// エンジン破棄
// ============================================================

void EngineLifecycleManager::destroyEngine(int idx, bool clearThinking)
{
    Usi*& ref = (idx == 1 ? m_usi1 : m_usi2);
    if (ref) {
        disconnectArbiterSignals(ref);
        ref->cleanupEngineProcessAndThread(clearThinking);
        ref->deleteLater();
        ref = nullptr;
        emit enginesChanged();
    }
}

void EngineLifecycleManager::destroyEngines(bool clearModels)
{
    qCDebug(lcGame).noquote() << "destroyEngines called, clearModels=" << clearModels;
    destroyEngine(1, clearModels);
    destroyEngine(2, clearModels);

    if (clearModels) {
        qCDebug(lcGame).noquote() << "destroyEngines clearing models";
        if (m_comm1)  m_comm1->clear();
        if (m_think1) m_think1->clearAllItems();
        if (m_comm2)  m_comm2->clear();
        if (m_think2) m_think2->clearAllItems();
    } else {
        qCDebug(lcGame).noquote() << "destroyEngines preserving models (clearModels=false)";
    }
}

// ============================================================
// モデル管理
// ============================================================

EngineLifecycleManager::EngineModelPair EngineLifecycleManager::ensureEngineModels(int engineIndex)
{
    UsiCommLogModel*&          commRef  = (engineIndex == 1) ? m_comm1  : m_comm2;
    ShogiEngineThinkingModel*& thinkRef = (engineIndex == 1) ? m_think1 : m_think2;

    if (!commRef) {
        commRef = new UsiCommLogModel(this);
        qCWarning(lcGame) << "comm" << engineIndex << "fallback created";
    }
    if (!thinkRef) {
        thinkRef = new ShogiEngineThinkingModel(this);
        qCWarning(lcGame) << "think" << engineIndex << "fallback created";
    }

    return { commRef, thinkRef };
}

// ============================================================
// EvE 初期化
// ============================================================

void EngineLifecycleManager::initEnginesForEvE(const QString& engineName1,
                                                const QString& engineName2)
{
    destroyEngines();

    auto [comm1, think1] = ensureEngineModels(1);
    auto [comm2, think2] = ensureEngineModels(2);

    const QString dispName1 = engineName1.isEmpty() ? QStringLiteral("Engine") : engineName1;
    const QString dispName2 = engineName2.isEmpty() ? QStringLiteral("Engine") : engineName2;
    comm1->setEngineName(dispName1);
    comm2->setEngineName(dispName2);

    m_usi1 = new Usi(comm1, think1, m_refs.gc, this);
    m_usi2 = new Usi(comm2, think2, m_refs.gc, this);

    m_usi1->resetResignNotified(); m_usi1->clearHardTimeout(); m_usi1->resetWinNotified();
    m_usi2->resetResignNotified(); m_usi2->clearHardTimeout(); m_usi2->resetWinNotified();

    wireResignToArbiter(m_usi1, /*asP1=*/true);
    wireResignToArbiter(m_usi2, /*asP1=*/false);
    wireWinToArbiter(m_usi1, /*asP1=*/true);
    wireWinToArbiter(m_usi2, /*asP1=*/false);

    m_usi1->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), engineName1);
    m_usi2->setLogIdentity(QStringLiteral("[E2]"), QStringLiteral("P2"), engineName2);
    m_usi1->setSquelchResignLogging(false);
    m_usi2->setSquelchResignLogging(false);

    updateUsiPtrs(m_usi1, m_usi2);
}

// ============================================================
// USI 送信
// ============================================================

void EngineLifecycleManager::sendStopToEngine(Usi* which)
{
    if (!which) return;
    which->sendStopCommand();
}

void EngineLifecycleManager::sendRawTo(Usi* which, const QString& cmd)
{
    if (!which) return;
    which->sendRaw(cmd);
}
