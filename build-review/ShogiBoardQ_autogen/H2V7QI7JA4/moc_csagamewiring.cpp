/****************************************************************************
** Meta object code from reading C++ file 'csagamewiring.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/wiring/csagamewiring.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'csagamewiring.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13CsaGameWiringE_t {};
} // unnamed namespace

template <> constexpr inline auto CsaGameWiring::qt_create_metaobjectdata<qt_meta_tag_ZN13CsaGameWiringE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CsaGameWiring",
        "disableNavigationRequested",
        "",
        "enableNavigationRequested",
        "appendKifuLineRequested",
        "text",
        "elapsedTime",
        "refreshBranchTreeRequested",
        "playModeChanged",
        "mode",
        "showGameEndDialogRequested",
        "title",
        "message",
        "errorMessageRequested",
        "onWaitingCancelled",
        "onPlayModeChangedInternal",
        "showGameEndDialogInternal",
        "onGameStarted",
        "blackName",
        "whiteName",
        "onGameEnded",
        "result",
        "cause",
        "consumedTimeMs",
        "onMoveMade",
        "csaMove",
        "usiMove",
        "prettyMove",
        "onTurnChanged",
        "isMyTurn",
        "onLogMessage",
        "isError",
        "onMoveHighlightRequested",
        "QPoint",
        "from",
        "to"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'disableNavigationRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enableNavigationRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'appendKifuLineRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'refreshBranchTreeRequested'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playModeChanged'
        QtMocHelpers::SignalData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Signal 'showGameEndDialogRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 },
        }}),
        // Signal 'errorMessageRequested'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'onWaitingCancelled'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onPlayModeChangedInternal'
        QtMocHelpers::SlotData<void(int)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'showGameEndDialogInternal'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 },
        }}),
        // Slot 'onGameStarted'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 18 }, { QMetaType::QString, 19 },
        }}),
        // Slot 'onGameEnded'
        QtMocHelpers::SlotData<void(const QString &, const QString &, int)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 }, { QMetaType::Int, 23 },
        }}),
        // Slot 'onMoveMade'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const QString &, int)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 25 }, { QMetaType::QString, 26 }, { QMetaType::QString, 27 }, { QMetaType::Int, 23 },
        }}),
        // Slot 'onTurnChanged'
        QtMocHelpers::SlotData<void(bool)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 29 },
        }}),
        // Slot 'onLogMessage'
        QtMocHelpers::SlotData<void(const QString &, bool)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 12 }, { QMetaType::Bool, 31 },
        }}),
        // Slot 'onMoveHighlightRequested'
        QtMocHelpers::SlotData<void(const QPoint &, const QPoint &)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 33, 34 }, { 0x80000000 | 33, 35 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<CsaGameWiring, qt_meta_tag_ZN13CsaGameWiringE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CsaGameWiring::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CsaGameWiringE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CsaGameWiringE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13CsaGameWiringE_t>.metaTypes,
    nullptr
} };

void CsaGameWiring::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CsaGameWiring *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->disableNavigationRequested(); break;
        case 1: _t->enableNavigationRequested(); break;
        case 2: _t->appendKifuLineRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->refreshBranchTreeRequested(); break;
        case 4: _t->playModeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->showGameEndDialogRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->errorMessageRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onWaitingCancelled(); break;
        case 8: _t->onPlayModeChangedInternal((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->showGameEndDialogInternal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->onGameStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->onGameEnded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 12: _t->onMoveMade((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 13: _t->onTurnChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->onLogMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 15: _t->onMoveHighlightRequested((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)()>(_a, &CsaGameWiring::disableNavigationRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)()>(_a, &CsaGameWiring::enableNavigationRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)(const QString & , const QString & )>(_a, &CsaGameWiring::appendKifuLineRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)()>(_a, &CsaGameWiring::refreshBranchTreeRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)(int )>(_a, &CsaGameWiring::playModeChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)(const QString & , const QString & )>(_a, &CsaGameWiring::showGameEndDialogRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameWiring::*)(const QString & )>(_a, &CsaGameWiring::errorMessageRequested, 6))
            return;
    }
}

const QMetaObject *CsaGameWiring::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CsaGameWiring::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13CsaGameWiringE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CsaGameWiring::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void CsaGameWiring::disableNavigationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void CsaGameWiring::enableNavigationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void CsaGameWiring::appendKifuLineRequested(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void CsaGameWiring::refreshBranchTreeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void CsaGameWiring::playModeChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void CsaGameWiring::showGameEndDialogRequested(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void CsaGameWiring::errorMessageRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}
QT_WARNING_POP
