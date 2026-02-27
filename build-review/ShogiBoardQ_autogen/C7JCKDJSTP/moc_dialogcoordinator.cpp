/****************************************************************************
** Meta object code from reading C++ file 'dialogcoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/coordinators/dialogcoordinator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dialogcoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17DialogCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto DialogCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN17DialogCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DialogCoordinator",
        "considerationModeStarted",
        "",
        "considerationTimeSettingsReady",
        "unlimited",
        "byoyomiSec",
        "considerationMultiPVReady",
        "multiPV",
        "tsumeSearchModeStarted",
        "tsumeSearchModeEnded",
        "analysisModeStarted",
        "analysisModeEnded",
        "analysisProgressReported",
        "ply",
        "scoreCp",
        "analysisResultRowSelected",
        "row"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'considerationModeStarted'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'considerationTimeSettingsReady'
        QtMocHelpers::SignalData<void(bool, int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 4 }, { QMetaType::Int, 5 },
        }}),
        // Signal 'considerationMultiPVReady'
        QtMocHelpers::SignalData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Signal 'tsumeSearchModeStarted'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'tsumeSearchModeEnded'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'analysisModeStarted'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'analysisModeEnded'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'analysisProgressReported'
        QtMocHelpers::SignalData<void(int, int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
        // Signal 'analysisResultRowSelected'
        QtMocHelpers::SignalData<void(int)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DialogCoordinator, qt_meta_tag_ZN17DialogCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DialogCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DialogCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DialogCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17DialogCoordinatorE_t>.metaTypes,
    nullptr
} };

void DialogCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DialogCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->considerationModeStarted(); break;
        case 1: _t->considerationTimeSettingsReady((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->considerationMultiPVReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->tsumeSearchModeStarted(); break;
        case 4: _t->tsumeSearchModeEnded(); break;
        case 5: _t->analysisModeStarted(); break;
        case 6: _t->analysisModeEnded(); break;
        case 7: _t->analysisProgressReported((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->analysisResultRowSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)()>(_a, &DialogCoordinator::considerationModeStarted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)(bool , int )>(_a, &DialogCoordinator::considerationTimeSettingsReady, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)(int )>(_a, &DialogCoordinator::considerationMultiPVReady, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)()>(_a, &DialogCoordinator::tsumeSearchModeStarted, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)()>(_a, &DialogCoordinator::tsumeSearchModeEnded, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)()>(_a, &DialogCoordinator::analysisModeStarted, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)()>(_a, &DialogCoordinator::analysisModeEnded, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)(int , int )>(_a, &DialogCoordinator::analysisProgressReported, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (DialogCoordinator::*)(int )>(_a, &DialogCoordinator::analysisResultRowSelected, 8))
            return;
    }
}

const QMetaObject *DialogCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DialogCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DialogCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DialogCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void DialogCoordinator::considerationModeStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DialogCoordinator::considerationTimeSettingsReady(bool _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void DialogCoordinator::considerationMultiPVReady(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void DialogCoordinator::tsumeSearchModeStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void DialogCoordinator::tsumeSearchModeEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void DialogCoordinator::analysisModeStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void DialogCoordinator::analysisModeEnded()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void DialogCoordinator::analysisProgressReported(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void DialogCoordinator::analysisResultRowSelected(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}
QT_WARNING_POP
