/****************************************************************************
** Meta object code from reading C++ file 'analysisflowcontroller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/analysis/analysisflowcontroller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'analysisflowcontroller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN22AnalysisFlowControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto AnalysisFlowController::qt_create_metaobjectdata<qt_meta_tag_ZN22AnalysisFlowControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AnalysisFlowController",
        "analysisStopped",
        "",
        "analysisProgressReported",
        "ply",
        "scoreCp",
        "analysisResultRowSelected",
        "row",
        "onUsiCommLogChanged",
        "onBestMoveReceived",
        "onInfoLineReceived",
        "line",
        "onThinkingInfoUpdated",
        "time",
        "depth",
        "nodes",
        "score",
        "pvKanjiStr",
        "usiPv",
        "baseSfen",
        "multipv",
        "onPositionPrepared",
        "sfen",
        "onAnalysisProgress",
        "seldepth",
        "mate",
        "pv",
        "raw",
        "onAnalysisFinished",
        "AnalysisCoordinator::Mode",
        "mode",
        "onResultRowDoubleClicked",
        "onEngineError",
        "msg"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'analysisStopped'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'analysisProgressReported'
        QtMocHelpers::SignalData<void(int, int)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 }, { QMetaType::Int, 5 },
        }}),
        // Signal 'analysisResultRowSelected'
        QtMocHelpers::SignalData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Slot 'onUsiCommLogChanged'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onBestMoveReceived'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onInfoLineReceived'
        QtMocHelpers::SlotData<void(const QString &)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Slot 'onThinkingInfoUpdated'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, int, int)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 13 }, { QMetaType::QString, 14 }, { QMetaType::QString, 15 }, { QMetaType::QString, 16 },
            { QMetaType::QString, 17 }, { QMetaType::QString, 18 }, { QMetaType::QString, 19 }, { QMetaType::Int, 20 },
            { QMetaType::Int, 5 },
        }}),
        // Slot 'onPositionPrepared'
        QtMocHelpers::SlotData<void(int, const QString &)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 4 }, { QMetaType::QString, 22 },
        }}),
        // Slot 'onAnalysisProgress'
        QtMocHelpers::SlotData<void(int, int, int, int, int, const QString &, const QString &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 4 }, { QMetaType::Int, 14 }, { QMetaType::Int, 24 }, { QMetaType::Int, 5 },
            { QMetaType::Int, 25 }, { QMetaType::QString, 26 }, { QMetaType::QString, 27 },
        }}),
        // Slot 'onAnalysisFinished'
        QtMocHelpers::SlotData<void(AnalysisCoordinator::Mode)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 29, 30 },
        }}),
        // Slot 'onResultRowDoubleClicked'
        QtMocHelpers::SlotData<void(int)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Slot 'onEngineError'
        QtMocHelpers::SlotData<void(const QString &)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 33 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AnalysisFlowController, qt_meta_tag_ZN22AnalysisFlowControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AnalysisFlowController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22AnalysisFlowControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22AnalysisFlowControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22AnalysisFlowControllerE_t>.metaTypes,
    nullptr
} };

void AnalysisFlowController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnalysisFlowController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->analysisStopped(); break;
        case 1: _t->analysisProgressReported((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->analysisResultRowSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onUsiCommLogChanged(); break;
        case 4: _t->onBestMoveReceived(); break;
        case 5: _t->onInfoLineReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->onThinkingInfoUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[8])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[9]))); break;
        case 7: _t->onPositionPrepared((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->onAnalysisProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7]))); break;
        case 9: _t->onAnalysisFinished((*reinterpret_cast<std::add_pointer_t<AnalysisCoordinator::Mode>>(_a[1]))); break;
        case 10: _t->onResultRowDoubleClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->onEngineError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AnalysisFlowController::*)()>(_a, &AnalysisFlowController::analysisStopped, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnalysisFlowController::*)(int , int )>(_a, &AnalysisFlowController::analysisProgressReported, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AnalysisFlowController::*)(int )>(_a, &AnalysisFlowController::analysisResultRowSelected, 2))
            return;
    }
}

const QMetaObject *AnalysisFlowController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AnalysisFlowController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22AnalysisFlowControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AnalysisFlowController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void AnalysisFlowController::analysisStopped()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AnalysisFlowController::analysisProgressReported(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void AnalysisFlowController::analysisResultRowSelected(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
