/****************************************************************************
** Meta object code from reading C++ file 'engineanalysistab.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/widgets/engineanalysistab.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'engineanalysistab.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17EngineAnalysisTabE_t {};
} // unnamed namespace

template <> constexpr inline auto EngineAnalysisTab::qt_create_metaobjectdata<qt_meta_tag_ZN17EngineAnalysisTabE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EngineAnalysisTab",
        "branchNodeActivated",
        "",
        "row",
        "ply",
        "requestApplyStart",
        "requestApplyMainAtPly",
        "commentUpdated",
        "moveIndex",
        "newComment",
        "pvRowClicked",
        "engineIndex",
        "csaRawCommandRequested",
        "command",
        "usiCommandRequested",
        "target",
        "considerationMultiPVChanged",
        "value",
        "stopConsiderationRequested",
        "startConsiderationRequested",
        "engineSettingsRequested",
        "engineNumber",
        "engineName",
        "considerationTimeSettingsChanged",
        "unlimited",
        "byoyomiSec",
        "showArrowsChanged",
        "checked",
        "considerationEngineChanged",
        "index",
        "name",
        "setAnalysisVisible",
        "on",
        "setCommentHtml",
        "html",
        "setCurrentMoveIndex",
        "currentMoveIndex",
        "onView1Clicked",
        "QModelIndex",
        "onView2Clicked",
        "onModel1Reset",
        "onModel2Reset",
        "onLog1Changed",
        "onLog2Changed",
        "onView1SectionResized",
        "logicalIndex",
        "oldSize",
        "newSize",
        "onView2SectionResized"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'branchNodeActivated'
        QtMocHelpers::SignalData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
        // Signal 'requestApplyStart'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestApplyMainAtPly'
        QtMocHelpers::SignalData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 },
        }}),
        // Signal 'commentUpdated'
        QtMocHelpers::SignalData<void(int, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::QString, 9 },
        }}),
        // Signal 'pvRowClicked'
        QtMocHelpers::SignalData<void(int, int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 }, { QMetaType::Int, 3 },
        }}),
        // Signal 'csaRawCommandRequested'
        QtMocHelpers::SignalData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Signal 'usiCommandRequested'
        QtMocHelpers::SignalData<void(int, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { QMetaType::QString, 13 },
        }}),
        // Signal 'considerationMultiPVChanged'
        QtMocHelpers::SignalData<void(int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 },
        }}),
        // Signal 'stopConsiderationRequested'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'startConsiderationRequested'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'engineSettingsRequested'
        QtMocHelpers::SignalData<void(int, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 21 }, { QMetaType::QString, 22 },
        }}),
        // Signal 'considerationTimeSettingsChanged'
        QtMocHelpers::SignalData<void(bool, int)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 24 }, { QMetaType::Int, 25 },
        }}),
        // Signal 'showArrowsChanged'
        QtMocHelpers::SignalData<void(bool)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 27 },
        }}),
        // Signal 'considerationEngineChanged'
        QtMocHelpers::SignalData<void(int, const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 }, { QMetaType::QString, 30 },
        }}),
        // Slot 'setAnalysisVisible'
        QtMocHelpers::SlotData<void(bool)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 32 },
        }}),
        // Slot 'setCommentHtml'
        QtMocHelpers::SlotData<void(const QString &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 },
        }}),
        // Slot 'setCurrentMoveIndex'
        QtMocHelpers::SlotData<void(int)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 },
        }}),
        // Slot 'currentMoveIndex'
        QtMocHelpers::SlotData<int() const>(36, 2, QMC::AccessPublic, QMetaType::Int),
        // Slot 'onView1Clicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 38, 29 },
        }}),
        // Slot 'onView2Clicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 38, 29 },
        }}),
        // Slot 'onModel1Reset'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onModel2Reset'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLog1Changed'
        QtMocHelpers::SlotData<void()>(42, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLog2Changed'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onView1SectionResized'
        QtMocHelpers::SlotData<void(int, int, int)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 45 }, { QMetaType::Int, 46 }, { QMetaType::Int, 47 },
        }}),
        // Slot 'onView2SectionResized'
        QtMocHelpers::SlotData<void(int, int, int)>(48, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 45 }, { QMetaType::Int, 46 }, { QMetaType::Int, 47 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<EngineAnalysisTab, qt_meta_tag_ZN17EngineAnalysisTabE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject EngineAnalysisTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EngineAnalysisTabE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EngineAnalysisTabE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17EngineAnalysisTabE_t>.metaTypes,
    nullptr
} };

void EngineAnalysisTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EngineAnalysisTab *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->branchNodeActivated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->requestApplyStart(); break;
        case 2: _t->requestApplyMainAtPly((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->commentUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->pvRowClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->csaRawCommandRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->usiCommandRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 7: _t->considerationMultiPVChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->stopConsiderationRequested(); break;
        case 9: _t->startConsiderationRequested(); break;
        case 10: _t->engineSettingsRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->considerationTimeSettingsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 12: _t->showArrowsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->considerationEngineChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->setAnalysisVisible((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->setCommentHtml((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->setCurrentMoveIndex((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 17: { int _r = _t->currentMoveIndex();
            if (_a[0]) *reinterpret_cast<int*>(_a[0]) = std::move(_r); }  break;
        case 18: _t->onView1Clicked((*reinterpret_cast<std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 19: _t->onView2Clicked((*reinterpret_cast<std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 20: _t->onModel1Reset(); break;
        case 21: _t->onModel2Reset(); break;
        case 22: _t->onLog1Changed(); break;
        case 23: _t->onLog2Changed(); break;
        case 24: _t->onView1SectionResized((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 25: _t->onView2SectionResized((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , int )>(_a, &EngineAnalysisTab::branchNodeActivated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)()>(_a, &EngineAnalysisTab::requestApplyStart, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int )>(_a, &EngineAnalysisTab::requestApplyMainAtPly, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , const QString & )>(_a, &EngineAnalysisTab::commentUpdated, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , int )>(_a, &EngineAnalysisTab::pvRowClicked, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(const QString & )>(_a, &EngineAnalysisTab::csaRawCommandRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , const QString & )>(_a, &EngineAnalysisTab::usiCommandRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int )>(_a, &EngineAnalysisTab::considerationMultiPVChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)()>(_a, &EngineAnalysisTab::stopConsiderationRequested, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)()>(_a, &EngineAnalysisTab::startConsiderationRequested, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , const QString & )>(_a, &EngineAnalysisTab::engineSettingsRequested, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(bool , int )>(_a, &EngineAnalysisTab::considerationTimeSettingsChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(bool )>(_a, &EngineAnalysisTab::showArrowsChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineAnalysisTab::*)(int , const QString & )>(_a, &EngineAnalysisTab::considerationEngineChanged, 13))
            return;
    }
}

const QMetaObject *EngineAnalysisTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EngineAnalysisTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EngineAnalysisTabE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int EngineAnalysisTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 26)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 26)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 26;
    }
    return _id;
}

// SIGNAL 0
void EngineAnalysisTab::branchNodeActivated(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void EngineAnalysisTab::requestApplyStart()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void EngineAnalysisTab::requestApplyMainAtPly(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void EngineAnalysisTab::commentUpdated(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void EngineAnalysisTab::pvRowClicked(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void EngineAnalysisTab::csaRawCommandRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void EngineAnalysisTab::usiCommandRequested(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void EngineAnalysisTab::considerationMultiPVChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void EngineAnalysisTab::stopConsiderationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void EngineAnalysisTab::startConsiderationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void EngineAnalysisTab::engineSettingsRequested(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2);
}

// SIGNAL 11
void EngineAnalysisTab::considerationTimeSettingsChanged(bool _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}

// SIGNAL 12
void EngineAnalysisTab::showArrowsChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void EngineAnalysisTab::considerationEngineChanged(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1, _t2);
}
QT_WARNING_POP
