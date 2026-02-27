/****************************************************************************
** Meta object code from reading C++ file 'shogigamecontroller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/game/shogigamecontroller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'shogigamecontroller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19ShogiGameControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto ShogiGameController::qt_create_metaobjectdata<qt_meta_tag_ZN19ShogiGameControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ShogiGameController",
        "showPromotionDialog",
        "",
        "currentPlayerChanged",
        "ShogiGameController::Player",
        "endDragSignal",
        "moveCommitted",
        "mover",
        "ply",
        "newGame",
        "QString&",
        "startsfenstr",
        "currentPlayer",
        "Player",
        "Result",
        "NoResult",
        "Player1Wins",
        "Draw",
        "Player2Wins",
        "NoPlayer",
        "Player1",
        "Player2"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'showPromotionDialog'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentPlayerChanged'
        QtMocHelpers::SignalData<void(ShogiGameController::Player)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 2 },
        }}),
        // Signal 'endDragSignal'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'moveCommitted'
        QtMocHelpers::SignalData<void(ShogiGameController::Player, int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 4, 7 }, { QMetaType::Int, 8 },
        }}),
        // Slot 'newGame'
        QtMocHelpers::SlotData<void(QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'currentPlayer'
        QtMocHelpers::PropertyData<enum Player>(12, 0x80000000 | 13, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Result'
        QtMocHelpers::EnumData<enum Result>(14, 14, QMC::EnumFlags{}).add({
            {   15, Result::NoResult },
            {   16, Result::Player1Wins },
            {   17, Result::Draw },
            {   18, Result::Player2Wins },
        }),
        // enum 'Player'
        QtMocHelpers::EnumData<enum Player>(13, 13, QMC::EnumFlags{}).add({
            {   19, Player::NoPlayer },
            {   20, Player::Player1 },
            {   21, Player::Player2 },
        }),
    };
    return QtMocHelpers::metaObjectData<ShogiGameController, qt_meta_tag_ZN19ShogiGameControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ShogiGameController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ShogiGameControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ShogiGameControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19ShogiGameControllerE_t>.metaTypes,
    nullptr
} };

void ShogiGameController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ShogiGameController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->showPromotionDialog(); break;
        case 1: _t->currentPlayerChanged((*reinterpret_cast<std::add_pointer_t<ShogiGameController::Player>>(_a[1]))); break;
        case 2: _t->endDragSignal(); break;
        case 3: _t->moveCommitted((*reinterpret_cast<std::add_pointer_t<ShogiGameController::Player>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->newGame((*reinterpret_cast<std::add_pointer_t<QString&>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ShogiGameController::*)()>(_a, &ShogiGameController::showPromotionDialog, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ShogiGameController::*)(ShogiGameController::Player )>(_a, &ShogiGameController::currentPlayerChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ShogiGameController::*)()>(_a, &ShogiGameController::endDragSignal, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ShogiGameController::*)(ShogiGameController::Player , int )>(_a, &ShogiGameController::moveCommitted, 3))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<enum Player*>(_v) = _t->currentPlayer(); break;
        default: break;
        }
    }
}

const QMetaObject *ShogiGameController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ShogiGameController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ShogiGameControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ShogiGameController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void ShogiGameController::showPromotionDialog()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ShogiGameController::currentPlayerChanged(ShogiGameController::Player _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ShogiGameController::endDragSignal()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ShogiGameController::moveCommitted(ShogiGameController::Player _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
