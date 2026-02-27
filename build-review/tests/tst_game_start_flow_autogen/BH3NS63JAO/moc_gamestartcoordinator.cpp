/****************************************************************************
** Meta object code from reading C++ file 'gamestartcoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/game/gamestartcoordinator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'gamestartcoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN20GameStartCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto GameStartCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN20GameStartCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "GameStartCoordinator",
        "requestPreStartCleanup",
        "",
        "requestApplyTimeControl",
        "GameStartCoordinator::TimeControl",
        "tc",
        "started",
        "MatchCoordinator::StartOptions",
        "opt",
        "playerNamesResolved",
        "human1",
        "human2",
        "engine1",
        "engine2",
        "playMode",
        "timeUpdated",
        "p1ms",
        "p2ms",
        "p1turn",
        "urgencyMs",
        "requestAppendGameOverMove",
        "MatchCoordinator::GameEndInfo",
        "info",
        "boardFlipped",
        "nowFlipped",
        "gameOverStateChanged",
        "MatchCoordinator::GameOverState",
        "st",
        "matchGameEnded",
        "consecutiveGamesConfigured",
        "totalGames",
        "switchTurn",
        "requestSelectKifuRow",
        "row",
        "requestBranchTreeResetForNewGame"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestPreStartCleanup'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestApplyTimeControl'
        QtMocHelpers::SignalData<void(const GameStartCoordinator::TimeControl &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 5 },
        }}),
        // Signal 'started'
        QtMocHelpers::SignalData<void(const MatchCoordinator::StartOptions &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Signal 'playerNamesResolved'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, const QString &, int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 }, { QMetaType::QString, 11 }, { QMetaType::QString, 12 }, { QMetaType::QString, 13 },
            { QMetaType::Int, 14 },
        }}),
        // Signal 'timeUpdated'
        QtMocHelpers::SignalData<void(qint64, qint64, bool, qint64)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 16 }, { QMetaType::LongLong, 17 }, { QMetaType::Bool, 18 }, { QMetaType::LongLong, 19 },
        }}),
        // Signal 'requestAppendGameOverMove'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
        // Signal 'boardFlipped'
        QtMocHelpers::SignalData<void(bool)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 24 },
        }}),
        // Signal 'gameOverStateChanged'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameOverState &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 26, 27 },
        }}),
        // Signal 'matchGameEnded'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
        // Signal 'consecutiveGamesConfigured'
        QtMocHelpers::SignalData<void(int, bool)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 30 }, { QMetaType::Bool, 31 },
        }}),
        // Signal 'requestSelectKifuRow'
        QtMocHelpers::SignalData<void(int)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 33 },
        }}),
        // Signal 'requestBranchTreeResetForNewGame'
        QtMocHelpers::SignalData<void()>(34, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<GameStartCoordinator, qt_meta_tag_ZN20GameStartCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject GameStartCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GameStartCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GameStartCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20GameStartCoordinatorE_t>.metaTypes,
    nullptr
} };

void GameStartCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GameStartCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestPreStartCleanup(); break;
        case 1: _t->requestApplyTimeControl((*reinterpret_cast<std::add_pointer_t<GameStartCoordinator::TimeControl>>(_a[1]))); break;
        case 2: _t->started((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::StartOptions>>(_a[1]))); break;
        case 3: _t->playerNamesResolved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5]))); break;
        case 4: _t->timeUpdated((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[4]))); break;
        case 5: _t->requestAppendGameOverMove((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 6: _t->boardFlipped((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->gameOverStateChanged((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameOverState>>(_a[1]))); break;
        case 8: _t->matchGameEnded((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 9: _t->consecutiveGamesConfigured((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 10: _t->requestSelectKifuRow((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->requestBranchTreeResetForNewGame(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< GameStartCoordinator::TimeControl >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)()>(_a, &GameStartCoordinator::requestPreStartCleanup, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const GameStartCoordinator::TimeControl & )>(_a, &GameStartCoordinator::requestApplyTimeControl, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const MatchCoordinator::StartOptions & )>(_a, &GameStartCoordinator::started, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const QString & , const QString & , const QString & , const QString & , int )>(_a, &GameStartCoordinator::playerNamesResolved, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(qint64 , qint64 , bool , qint64 )>(_a, &GameStartCoordinator::timeUpdated, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const MatchCoordinator::GameEndInfo & )>(_a, &GameStartCoordinator::requestAppendGameOverMove, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(bool )>(_a, &GameStartCoordinator::boardFlipped, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const MatchCoordinator::GameOverState & )>(_a, &GameStartCoordinator::gameOverStateChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(const MatchCoordinator::GameEndInfo & )>(_a, &GameStartCoordinator::matchGameEnded, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(int , bool )>(_a, &GameStartCoordinator::consecutiveGamesConfigured, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)(int )>(_a, &GameStartCoordinator::requestSelectKifuRow, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameStartCoordinator::*)()>(_a, &GameStartCoordinator::requestBranchTreeResetForNewGame, 11))
            return;
    }
}

const QMetaObject *GameStartCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameStartCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GameStartCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int GameStartCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void GameStartCoordinator::requestPreStartCleanup()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void GameStartCoordinator::requestApplyTimeControl(const GameStartCoordinator::TimeControl & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void GameStartCoordinator::started(const MatchCoordinator::StartOptions & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void GameStartCoordinator::playerNamesResolved(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, int _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3, _t4, _t5);
}

// SIGNAL 4
void GameStartCoordinator::timeUpdated(qint64 _t1, qint64 _t2, bool _t3, qint64 _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 5
void GameStartCoordinator::requestAppendGameOverMove(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void GameStartCoordinator::boardFlipped(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void GameStartCoordinator::gameOverStateChanged(const MatchCoordinator::GameOverState & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void GameStartCoordinator::matchGameEnded(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void GameStartCoordinator::consecutiveGamesConfigured(int _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}

// SIGNAL 10
void GameStartCoordinator::requestSelectKifuRow(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void GameStartCoordinator::requestBranchTreeResetForNewGame()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}
QT_WARNING_POP
