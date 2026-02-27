/****************************************************************************
** Meta object code from reading C++ file 'gamesessionorchestrator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/app/gamesessionorchestrator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'gamesessionorchestrator.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN23GameSessionOrchestratorE_t {};
} // unnamed namespace

template <> constexpr inline auto GameSessionOrchestrator::qt_create_metaobjectdata<qt_meta_tag_ZN23GameSessionOrchestratorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "GameSessionOrchestrator",
        "initializeGame",
        "",
        "handleResignation",
        "handleBreakOffGame",
        "movePieceImmediately",
        "stopTsumeSearch",
        "openWebsiteInExternalBrowser",
        "onMatchGameEnded",
        "MatchCoordinator::GameEndInfo",
        "info",
        "onGameOverStateChanged",
        "MatchCoordinator::GameOverState",
        "st",
        "onRequestAppendGameOverMove",
        "onResignationTriggered",
        "onPreStartCleanupRequested",
        "onApplyTimeControlRequested",
        "GameStartCoordinator::TimeControl",
        "tc",
        "onGameStarted",
        "MatchCoordinator::StartOptions",
        "opt",
        "onConsecutiveStartRequested",
        "GameStartCoordinator::StartParams",
        "params",
        "onConsecutiveGamesConfigured",
        "totalGames",
        "switchTurn",
        "startNextConsecutiveGame",
        "startNewShogiGame",
        "invokeStartGame"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'initializeGame'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleResignation'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleBreakOffGame'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'movePieceImmediately'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'stopTsumeSearch'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'openWebsiteInExternalBrowser'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onMatchGameEnded'
        QtMocHelpers::SlotData<void(const MatchCoordinator::GameEndInfo &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'onGameOverStateChanged'
        QtMocHelpers::SlotData<void(const MatchCoordinator::GameOverState &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onRequestAppendGameOverMove'
        QtMocHelpers::SlotData<void(const MatchCoordinator::GameEndInfo &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'onResignationTriggered'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onPreStartCleanupRequested'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onApplyTimeControlRequested'
        QtMocHelpers::SlotData<void(const GameStartCoordinator::TimeControl &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 },
        }}),
        // Slot 'onGameStarted'
        QtMocHelpers::SlotData<void(const MatchCoordinator::StartOptions &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
        // Slot 'onConsecutiveStartRequested'
        QtMocHelpers::SlotData<void(const GameStartCoordinator::StartParams &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 24, 25 },
        }}),
        // Slot 'onConsecutiveGamesConfigured'
        QtMocHelpers::SlotData<void(int, bool)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 27 }, { QMetaType::Bool, 28 },
        }}),
        // Slot 'startNextConsecutiveGame'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startNewShogiGame'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'invokeStartGame'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<GameSessionOrchestrator, qt_meta_tag_ZN23GameSessionOrchestratorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject GameSessionOrchestrator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23GameSessionOrchestratorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23GameSessionOrchestratorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23GameSessionOrchestratorE_t>.metaTypes,
    nullptr
} };

void GameSessionOrchestrator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GameSessionOrchestrator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->initializeGame(); break;
        case 1: _t->handleResignation(); break;
        case 2: _t->handleBreakOffGame(); break;
        case 3: _t->movePieceImmediately(); break;
        case 4: _t->stopTsumeSearch(); break;
        case 5: _t->openWebsiteInExternalBrowser(); break;
        case 6: _t->onMatchGameEnded((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 7: _t->onGameOverStateChanged((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameOverState>>(_a[1]))); break;
        case 8: _t->onRequestAppendGameOverMove((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 9: _t->onResignationTriggered(); break;
        case 10: _t->onPreStartCleanupRequested(); break;
        case 11: _t->onApplyTimeControlRequested((*reinterpret_cast<std::add_pointer_t<GameStartCoordinator::TimeControl>>(_a[1]))); break;
        case 12: _t->onGameStarted((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::StartOptions>>(_a[1]))); break;
        case 13: _t->onConsecutiveStartRequested((*reinterpret_cast<std::add_pointer_t<GameStartCoordinator::StartParams>>(_a[1]))); break;
        case 14: _t->onConsecutiveGamesConfigured((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 15: _t->startNextConsecutiveGame(); break;
        case 16: _t->startNewShogiGame(); break;
        case 17: _t->invokeStartGame(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< GameStartCoordinator::TimeControl >(); break;
            }
            break;
        }
    }
}

const QMetaObject *GameSessionOrchestrator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameSessionOrchestrator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23GameSessionOrchestratorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int GameSessionOrchestrator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    return _id;
}
QT_WARNING_POP
