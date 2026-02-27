/****************************************************************************
** Meta object code from reading C++ file 'analysiscoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/analysis/analysiscoordinator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'analysiscoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19AnalysisCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto AnalysisCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN19AnalysisCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AnalysisCoordinator",
        "requestSendUsiCommand",
        "",
        "line",
        "analysisFinished",
        "AnalysisCoordinator::Mode",
        "mode",
        "analysisProgress",
        "ply",
        "depth",
        "seldepth",
        "scoreCp",
        "mate",
        "pv",
        "rawInfoLine",
        "positionPrepared",
        "positionCmd",
        "onStopTimerTimeout",
        "Mode",
        "Idle",
        "SinglePosition",
        "RangePositions"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'requestSendUsiCommand'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'analysisFinished'
        QtMocHelpers::SignalData<void(AnalysisCoordinator::Mode)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'analysisProgress'
        QtMocHelpers::SignalData<void(int, int, int, int, int, const QString &, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::Int, 9 }, { QMetaType::Int, 10 }, { QMetaType::Int, 11 },
            { QMetaType::Int, 12 }, { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'positionPrepared'
        QtMocHelpers::SignalData<void(int, const QString &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::QString, 16 },
        }}),
        // Slot 'onStopTimerTimeout'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Mode'
        QtMocHelpers::EnumData<enum Mode>(18, 18, QMC::EnumFlags{}).add({
            {   19, Mode::Idle },
            {   20, Mode::SinglePosition },
            {   21, Mode::RangePositions },
        }),
    };
    return QtMocHelpers::metaObjectData<AnalysisCoordinator, qt_meta_tag_ZN19AnalysisCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AnalysisCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalysisCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalysisCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19AnalysisCoordinatorE_t>.metaTypes,
    nullptr
} };

void AnalysisCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnalysisCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->requestSendUsiCommand((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->analysisFinished((*reinterpret_cast<std::add_pointer_t<AnalysisCoordinator::Mode>>(_a[1]))); break;
        case 2: _t->analysisProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7]))); break;
        case 3: _t->positionPrepared((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->onStopTimerTimeout(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AnalysisCoordinator::*)(const QString & )>(_a, &AnalysisCoordinator::requestSendUsiCommand, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnalysisCoordinator::*)(AnalysisCoordinator::Mode )>(_a, &AnalysisCoordinator::analysisFinished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnalysisCoordinator::*)(int , int , int , int , int , const QString & , const QString & )>(_a, &AnalysisCoordinator::analysisProgress, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnalysisCoordinator::*)(int , const QString & )>(_a, &AnalysisCoordinator::positionPrepared, 3))
            return;
    }
}

const QMetaObject *AnalysisCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AnalysisCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19AnalysisCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AnalysisCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    return _id;
}

// SIGNAL 0
void AnalysisCoordinator::requestSendUsiCommand(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AnalysisCoordinator::analysisFinished(AnalysisCoordinator::Mode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AnalysisCoordinator::analysisProgress(int _t1, int _t2, int _t3, int _t4, int _t5, const QString & _t6, const QString & _t7)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4, _t5, _t6, _t7);
}

// SIGNAL 3
void AnalysisCoordinator::positionPrepared(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
