/****************************************************************************
** Meta object code from reading C++ file 'consecutivegamescontroller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/game/consecutivegamescontroller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'consecutivegamescontroller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN26ConsecutiveGamesControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsecutiveGamesController::qt_create_metaobjectdata<qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsecutiveGamesController",
        "requestStartNextGame",
        "",
        "GameStartCoordinator::StartParams",
        "params",
        "requestPreStartCleanup",
        "startNextGame"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestStartNextGame'
        QtMocHelpers::SignalData<void(const GameStartCoordinator::StartParams &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'requestPreStartCleanup'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startNextGame'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsecutiveGamesController, qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsecutiveGamesController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>.metaTypes,
    nullptr
} };

void ConsecutiveGamesController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsecutiveGamesController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestStartNextGame((*reinterpret_cast<std::add_pointer_t<GameStartCoordinator::StartParams>>(_a[1]))); break;
        case 1: _t->requestPreStartCleanup(); break;
        case 2: _t->startNextGame(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsecutiveGamesController::*)(const GameStartCoordinator::StartParams & )>(_a, &ConsecutiveGamesController::requestStartNextGame, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsecutiveGamesController::*)()>(_a, &ConsecutiveGamesController::requestPreStartCleanup, 1))
            return;
    }
}

const QMetaObject *ConsecutiveGamesController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsecutiveGamesController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN26ConsecutiveGamesControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ConsecutiveGamesController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void ConsecutiveGamesController::requestStartNextGame(const GameStartCoordinator::StartParams & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ConsecutiveGamesController::requestPreStartCleanup()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
