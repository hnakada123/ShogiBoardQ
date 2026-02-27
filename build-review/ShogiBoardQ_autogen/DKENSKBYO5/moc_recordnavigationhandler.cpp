/****************************************************************************
** Meta object code from reading C++ file 'recordnavigationhandler.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/navigation/recordnavigationhandler.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'recordnavigationhandler.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23RecordNavigationHandlerE_t {};
} // unnamed namespace

template <> constexpr inline auto RecordNavigationHandler::qt_create_metaobjectdata<qt_meta_tag_ZN23RecordNavigationHandlerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RecordNavigationHandler",
        "boardSyncRequired",
        "",
        "ply",
        "branchBoardSyncRequired",
        "currentSfen",
        "prevSfen",
        "enableArrowButtonsRequired",
        "turnUpdateRequired",
        "josekiUpdateRequired",
        "buildPositionRequired",
        "row",
        "onMainRowChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'boardSyncRequired'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'branchBoardSyncRequired'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'enableArrowButtonsRequired'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'turnUpdateRequired'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'josekiUpdateRequired'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'buildPositionRequired'
        QtMocHelpers::SignalData<void(int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Slot 'onMainRowChanged'
        QtMocHelpers::SlotData<void(int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RecordNavigationHandler, qt_meta_tag_ZN23RecordNavigationHandlerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RecordNavigationHandler::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23RecordNavigationHandlerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23RecordNavigationHandlerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23RecordNavigationHandlerE_t>.metaTypes,
    nullptr
} };

void RecordNavigationHandler::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RecordNavigationHandler *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->boardSyncRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->branchBoardSyncRequired((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->enableArrowButtonsRequired(); break;
        case 3: _t->turnUpdateRequired(); break;
        case 4: _t->josekiUpdateRequired(); break;
        case 5: _t->buildPositionRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onMainRowChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)(int )>(_a, &RecordNavigationHandler::boardSyncRequired, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)(const QString & , const QString & )>(_a, &RecordNavigationHandler::branchBoardSyncRequired, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)()>(_a, &RecordNavigationHandler::enableArrowButtonsRequired, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)()>(_a, &RecordNavigationHandler::turnUpdateRequired, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)()>(_a, &RecordNavigationHandler::josekiUpdateRequired, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecordNavigationHandler::*)(int )>(_a, &RecordNavigationHandler::buildPositionRequired, 5))
            return;
    }
}

const QMetaObject *RecordNavigationHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RecordNavigationHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23RecordNavigationHandlerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int RecordNavigationHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void RecordNavigationHandler::boardSyncRequired(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void RecordNavigationHandler::branchBoardSyncRequired(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void RecordNavigationHandler::enableArrowButtonsRequired()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void RecordNavigationHandler::turnUpdateRequired()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void RecordNavigationHandler::josekiUpdateRequired()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void RecordNavigationHandler::buildPositionRequired(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
