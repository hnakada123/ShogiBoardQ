/****************************************************************************
** Meta object code from reading C++ file 'matchcoordinatorwiring.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/wiring/matchcoordinatorwiring.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'matchcoordinatorwiring.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN22MatchCoordinatorWiringE_t {};
} // unnamed namespace

template <> constexpr inline auto MatchCoordinatorWiring::qt_create_metaobjectdata<qt_meta_tag_ZN22MatchCoordinatorWiringE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MatchCoordinatorWiring",
        "requestAppendGameOverMove",
        "",
        "MatchCoordinator::GameEndInfo",
        "info",
        "boardFlipped",
        "nowFlipped",
        "gameOverStateChanged",
        "MatchCoordinator::GameOverState",
        "st",
        "matchGameEnded",
        "resignationTriggered",
        "requestPreStartCleanup",
        "requestApplyTimeControl",
        "GameStartCoordinator::TimeControl",
        "tc",
        "menuPlayerNamesResolved",
        "human1",
        "human2",
        "engine1",
        "engine2",
        "playMode",
        "consecutiveGamesConfigured",
        "totalGames",
        "switchTurn",
        "gameStarted",
        "MatchCoordinator::StartOptions",
        "opt",
        "requestSelectKifuRow",
        "row",
        "requestBranchTreeResetForNewGame"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestAppendGameOverMove'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'boardFlipped'
        QtMocHelpers::SignalData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Signal 'gameOverStateChanged'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameOverState &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Signal 'matchGameEnded'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'resignationTriggered'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestPreStartCleanup'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestApplyTimeControl'
        QtMocHelpers::SignalData<void(const GameStartCoordinator::TimeControl &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 14, 15 },
        }}),
        // Signal 'menuPlayerNamesResolved'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, const QString &, int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 }, { QMetaType::QString, 18 }, { QMetaType::QString, 19 }, { QMetaType::QString, 20 },
            { QMetaType::Int, 21 },
        }}),
        // Signal 'consecutiveGamesConfigured'
        QtMocHelpers::SignalData<void(int, bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 23 }, { QMetaType::Bool, 24 },
        }}),
        // Signal 'gameStarted'
        QtMocHelpers::SignalData<void(const MatchCoordinator::StartOptions &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 26, 27 },
        }}),
        // Signal 'requestSelectKifuRow'
        QtMocHelpers::SignalData<void(int)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 },
        }}),
        // Signal 'requestBranchTreeResetForNewGame'
        QtMocHelpers::SignalData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MatchCoordinatorWiring, qt_meta_tag_ZN22MatchCoordinatorWiringE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MatchCoordinatorWiring::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22MatchCoordinatorWiringE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22MatchCoordinatorWiringE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22MatchCoordinatorWiringE_t>.metaTypes,
    nullptr
} };

void MatchCoordinatorWiring::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MatchCoordinatorWiring *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestAppendGameOverMove((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 1: _t->boardFlipped((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->gameOverStateChanged((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameOverState>>(_a[1]))); break;
        case 3: _t->matchGameEnded((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 4: _t->resignationTriggered(); break;
        case 5: _t->requestPreStartCleanup(); break;
        case 6: _t->requestApplyTimeControl((*reinterpret_cast<std::add_pointer_t<GameStartCoordinator::TimeControl>>(_a[1]))); break;
        case 7: _t->menuPlayerNamesResolved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5]))); break;
        case 8: _t->consecutiveGamesConfigured((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 9: _t->gameStarted((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::StartOptions>>(_a[1]))); break;
        case 10: _t->requestSelectKifuRow((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->requestBranchTreeResetForNewGame(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< GameStartCoordinator::TimeControl >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const MatchCoordinator::GameEndInfo & )>(_a, &MatchCoordinatorWiring::requestAppendGameOverMove, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(bool )>(_a, &MatchCoordinatorWiring::boardFlipped, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const MatchCoordinator::GameOverState & )>(_a, &MatchCoordinatorWiring::gameOverStateChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const MatchCoordinator::GameEndInfo & )>(_a, &MatchCoordinatorWiring::matchGameEnded, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)()>(_a, &MatchCoordinatorWiring::resignationTriggered, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)()>(_a, &MatchCoordinatorWiring::requestPreStartCleanup, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const GameStartCoordinator::TimeControl & )>(_a, &MatchCoordinatorWiring::requestApplyTimeControl, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const QString & , const QString & , const QString & , const QString & , int )>(_a, &MatchCoordinatorWiring::menuPlayerNamesResolved, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(int , bool )>(_a, &MatchCoordinatorWiring::consecutiveGamesConfigured, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(const MatchCoordinator::StartOptions & )>(_a, &MatchCoordinatorWiring::gameStarted, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)(int )>(_a, &MatchCoordinatorWiring::requestSelectKifuRow, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinatorWiring::*)()>(_a, &MatchCoordinatorWiring::requestBranchTreeResetForNewGame, 11))
            return;
    }
}

const QMetaObject *MatchCoordinatorWiring::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MatchCoordinatorWiring::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22MatchCoordinatorWiringE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MatchCoordinatorWiring::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void MatchCoordinatorWiring::requestAppendGameOverMove(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void MatchCoordinatorWiring::boardFlipped(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void MatchCoordinatorWiring::gameOverStateChanged(const MatchCoordinator::GameOverState & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void MatchCoordinatorWiring::matchGameEnded(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void MatchCoordinatorWiring::resignationTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void MatchCoordinatorWiring::requestPreStartCleanup()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void MatchCoordinatorWiring::requestApplyTimeControl(const GameStartCoordinator::TimeControl & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void MatchCoordinatorWiring::menuPlayerNamesResolved(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, int _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 8
void MatchCoordinatorWiring::consecutiveGamesConfigured(int _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}

// SIGNAL 9
void MatchCoordinatorWiring::gameStarted(const MatchCoordinator::StartOptions & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void MatchCoordinatorWiring::requestSelectKifuRow(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void MatchCoordinatorWiring::requestBranchTreeResetForNewGame()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}
QT_WARNING_POP
