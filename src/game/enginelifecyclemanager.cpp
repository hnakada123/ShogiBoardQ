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

void EngineLifecycleManager::initializeAndStartEngineFor(Turn side,
                                                         const QString& enginePathIn,
                                                         const QString& engineNameIn)
{
    // 人間対エンジンではエンジンが後手でも m_usi1 だけを使い、m_usi2 は空のままにする
    // （呼び出し側は「後手側が空なら先手側」と扱う）。参照で受けると m_usi2 に同じエンジンが入り、
    // E2 宛てのコマンドや後始末が同じエンジンへ二重に届く。
    Usi* eng = (side == Turn::Black ? m_usi1 : m_usi2);

    if (!eng) {
        if (m_usi1 && !m_usi2) {
            qCDebug(lcGame) << "[Match] fallback to m_usi1 for HvE";
            eng = m_usi1;
        } else {
            qCWarning(lcGame) << "[Match] engine ptr is null (side=" << side << ")";
            return;
        }
    }

    QString path = enginePathIn;
    QString name = engineNameIn;

    // 起動の失敗や対局中の異常終了を MatchCoordinator に知らせる。
    // 起動の失敗はこの呼び出しの中で通知されることがある。
    QObject::connect(eng, &Usi::errorOccurred,
                     this, &EngineLifecycleManager::engineError,
                     Qt::UniqueConnection);
    eng->initializeAndStartEngineCommunication(path, name);

    wireResignToArbiter(eng, side);
    wireWinToArbiter(eng, side);
}

void EngineLifecycleManager::wireResignToArbiter(Usi* engine, Turn side)
{
    if (!engine) return;

    QObject::disconnect(engine, &Usi::bestMoveResignReceived, this, nullptr);

    if (side == Turn::Black) {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &EngineLifecycleManager::onBlackEngineResign,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveResignReceived,
                         this,   &EngineLifecycleManager::onWhiteEngineResign,
                         Qt::UniqueConnection);
    }
}

void EngineLifecycleManager::wireWinToArbiter(Usi* engine, Turn side)
{
    if (!engine) return;

    QObject::disconnect(engine, &Usi::bestMoveWinReceived, this, nullptr);

    if (side == Turn::Black) {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &EngineLifecycleManager::onBlackEngineWin,
                         Qt::UniqueConnection);
    } else {
        QObject::connect(engine, &Usi::bestMoveWinReceived,
                         this,   &EngineLifecycleManager::onWhiteEngineWin,
                         Qt::UniqueConnection);
    }
}

void EngineLifecycleManager::onBlackEngineResign()
{
    if (m_hooks.onEngineResign) m_hooks.onEngineResign(Turn::Black);
}

void EngineLifecycleManager::onWhiteEngineResign()
{
    if (m_hooks.onEngineResign) m_hooks.onEngineResign(Turn::White);
}

void EngineLifecycleManager::onBlackEngineWin()
{
    if (m_hooks.onEngineWin) m_hooks.onEngineWin(Turn::Black);
}

void EngineLifecycleManager::onWhiteEngineWin()
{
    if (m_hooks.onEngineWin) m_hooks.onEngineWin(Turn::White);
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
        QObject::disconnect(ref, &Usi::errorOccurred, this, nullptr);
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

    wireResignToArbiter(m_usi1, Turn::Black);
    wireResignToArbiter(m_usi2, Turn::White);
    wireWinToArbiter(m_usi1, Turn::Black);
    wireWinToArbiter(m_usi2, Turn::White);

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
    // 対局中の「すぐ指させる」（MatchTurnHandler::forceImmediateMove）から使う
    if (!which) return;
    which->requestImmediateMove();
}

void EngineLifecycleManager::sendRawTo(Usi* which, const QString& cmd)
{
    if (!which) return;
    which->sendRaw(cmd);
}
