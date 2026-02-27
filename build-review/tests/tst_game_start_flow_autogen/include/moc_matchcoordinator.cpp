/****************************************************************************
** Meta object code from reading C++ file 'matchcoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/game/matchcoordinator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'matchcoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16MatchCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto MatchCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN16MatchCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MatchCoordinator",
        "boardFlipped",
        "",
        "nowFlipped",
        "gameEnded",
        "MatchCoordinator::GameEndInfo",
        "info",
        "timeUpdated",
        "p1ms",
        "p2ms",
        "p1turn",
        "urgencyMs",
        "gameOverStateChanged",
        "MatchCoordinator::GameOverState",
        "st",
        "requestAppendGameOverMove",
        "considerationModeEnded",
        "tsumeSearchModeEnded",
        "considerationWaitingStarted",
        "pokeTimeUpdateNow",
        "onEngine1Resign",
        "onEngine2Resign",
        "onEngine1Win",
        "onEngine2Win",
        "onClockTick",
        "onUsiError",
        "msg"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'boardFlipped'
        QtMocHelpers::SignalData<void(bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Signal 'gameEnded'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'timeUpdated'
        QtMocHelpers::SignalData<void(qint64, qint64, bool, qint64)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 8 }, { QMetaType::LongLong, 9 }, { QMetaType::Bool, 10 }, { QMetaType::LongLong, 11 },
        }}),
        // Signal 'gameOverStateChanged'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameOverState &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'requestAppendGameOverMove'
        QtMocHelpers::SignalData<void(const MatchCoordinator::GameEndInfo &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'considerationModeEnded'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'tsumeSearchModeEnded'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'considerationWaitingStarted'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'pokeTimeUpdateNow'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onEngine1Resign'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEngine2Resign'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEngine1Win'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEngine2Win'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClockTick'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onUsiError'
        QtMocHelpers::SlotData<void(const QString &)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 26 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MatchCoordinator, qt_meta_tag_ZN16MatchCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MatchCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16MatchCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16MatchCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16MatchCoordinatorE_t>.metaTypes,
    nullptr
} };

void MatchCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MatchCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->boardFlipped((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->gameEnded((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 2: _t->timeUpdated((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[4]))); break;
        case 3: _t->gameOverStateChanged((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameOverState>>(_a[1]))); break;
        case 4: _t->requestAppendGameOverMove((*reinterpret_cast<std::add_pointer_t<MatchCoordinator::GameEndInfo>>(_a[1]))); break;
        case 5: _t->considerationModeEnded(); break;
        case 6: _t->tsumeSearchModeEnded(); break;
        case 7: _t->considerationWaitingStarted(); break;
        case 8: _t->pokeTimeUpdateNow(); break;
        case 9: _t->onEngine1Resign(); break;
        case 10: _t->onEngine2Resign(); break;
        case 11: _t->onEngine1Win(); break;
        case 12: _t->onEngine2Win(); break;
        case 13: _t->onClockTick(); break;
        case 14: _t->onUsiError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)(bool )>(_a, &MatchCoordinator::boardFlipped, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)(const MatchCoordinator::GameEndInfo & )>(_a, &MatchCoordinator::gameEnded, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)(qint64 , qint64 , bool , qint64 )>(_a, &MatchCoordinator::timeUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)(const MatchCoordinator::GameOverState & )>(_a, &MatchCoordinator::gameOverStateChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)(const MatchCoordinator::GameEndInfo & )>(_a, &MatchCoordinator::requestAppendGameOverMove, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)()>(_a, &MatchCoordinator::considerationModeEnded, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)()>(_a, &MatchCoordinator::tsumeSearchModeEnded, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (MatchCoordinator::*)()>(_a, &MatchCoordinator::considerationWaitingStarted, 7))
            return;
    }
}

const QMetaObject *MatchCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MatchCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16MatchCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MatchCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void MatchCoordinator::boardFlipped(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void MatchCoordinator::gameEnded(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void MatchCoordinator::timeUpdated(qint64 _t1, qint64 _t2, bool _t3, qint64 _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 3
void MatchCoordinator::gameOverStateChanged(const MatchCoordinator::GameOverState & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void MatchCoordinator::requestAppendGameOverMove(const MatchCoordinator::GameEndInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void MatchCoordinator::considerationModeEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void MatchCoordinator::tsumeSearchModeEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void MatchCoordinator::considerationWaitingStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}
QT_WARNING_POP
