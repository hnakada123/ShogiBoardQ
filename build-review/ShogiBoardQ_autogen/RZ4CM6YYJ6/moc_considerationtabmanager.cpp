/****************************************************************************
** Meta object code from reading C++ file 'considerationtabmanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/widgets/considerationtabmanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'considerationtabmanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23ConsiderationTabManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsiderationTabManager::qt_create_metaobjectdata<qt_meta_tag_ZN23ConsiderationTabManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsiderationTabManager",
        "considerationMultiPVChanged",
        "",
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
        "pvRowClicked",
        "engineIndex",
        "row",
        "onConsiderationViewClicked",
        "QModelIndex",
        "onConsiderationFontIncrease",
        "onConsiderationFontDecrease",
        "onMultiPVComboBoxChanged",
        "onEngineComboBoxChanged",
        "onEngineSettingsClicked",
        "onTimeSettingChanged",
        "onElapsedTimerTick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'considerationMultiPVChanged'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'stopConsiderationRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'startConsiderationRequested'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'engineSettingsRequested'
        QtMocHelpers::SignalData<void(int, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 8 },
        }}),
        // Signal 'considerationTimeSettingsChanged'
        QtMocHelpers::SignalData<void(bool, int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 }, { QMetaType::Int, 11 },
        }}),
        // Signal 'showArrowsChanged'
        QtMocHelpers::SignalData<void(bool)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 13 },
        }}),
        // Signal 'considerationEngineChanged'
        QtMocHelpers::SignalData<void(int, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 }, { QMetaType::QString, 16 },
        }}),
        // Signal 'pvRowClicked'
        QtMocHelpers::SignalData<void(int, int)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 18 }, { QMetaType::Int, 19 },
        }}),
        // Slot 'onConsiderationViewClicked'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 15 },
        }}),
        // Slot 'onConsiderationFontIncrease'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onConsiderationFontDecrease'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMultiPVComboBoxChanged'
        QtMocHelpers::SlotData<void(int)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'onEngineComboBoxChanged'
        QtMocHelpers::SlotData<void(int)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'onEngineSettingsClicked'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTimeSettingChanged'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onElapsedTimerTick'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsiderationTabManager, qt_meta_tag_ZN23ConsiderationTabManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsiderationTabManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConsiderationTabManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConsiderationTabManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23ConsiderationTabManagerE_t>.metaTypes,
    nullptr
} };

void ConsiderationTabManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsiderationTabManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->considerationMultiPVChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->stopConsiderationRequested(); break;
        case 2: _t->startConsiderationRequested(); break;
        case 3: _t->engineSettingsRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->considerationTimeSettingsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->showArrowsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->considerationEngineChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 7: _t->pvRowClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->onConsiderationViewClicked((*reinterpret_cast<std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 9: _t->onConsiderationFontIncrease(); break;
        case 10: _t->onConsiderationFontDecrease(); break;
        case 11: _t->onMultiPVComboBoxChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->onEngineComboBoxChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->onEngineSettingsClicked(); break;
        case 14: _t->onTimeSettingChanged(); break;
        case 15: _t->onElapsedTimerTick(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(int )>(_a, &ConsiderationTabManager::considerationMultiPVChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)()>(_a, &ConsiderationTabManager::stopConsiderationRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)()>(_a, &ConsiderationTabManager::startConsiderationRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(int , const QString & )>(_a, &ConsiderationTabManager::engineSettingsRequested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(bool , int )>(_a, &ConsiderationTabManager::considerationTimeSettingsChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(bool )>(_a, &ConsiderationTabManager::showArrowsChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(int , const QString & )>(_a, &ConsiderationTabManager::considerationEngineChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConsiderationTabManager::*)(int , int )>(_a, &ConsiderationTabManager::pvRowClicked, 7))
            return;
    }
}

const QMetaObject *ConsiderationTabManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsiderationTabManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConsiderationTabManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ConsiderationTabManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void ConsiderationTabManager::considerationMultiPVChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ConsiderationTabManager::stopConsiderationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ConsiderationTabManager::startConsiderationRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void ConsiderationTabManager::engineSettingsRequested(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void ConsiderationTabManager::considerationTimeSettingsChanged(bool _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void ConsiderationTabManager::showArrowsChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void ConsiderationTabManager::considerationEngineChanged(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void ConsiderationTabManager::pvRowClicked(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}
QT_WARNING_POP
