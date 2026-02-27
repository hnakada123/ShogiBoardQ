/****************************************************************************
** Meta object code from reading C++ file 'uistatepolicymanager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/coordinators/uistatepolicymanager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uistatepolicymanager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN20UiStatePolicyManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto UiStatePolicyManager::qt_create_metaobjectdata<qt_meta_tag_ZN20UiStatePolicyManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UiStatePolicyManager",
        "stateChanged",
        "",
        "AppState",
        "newState",
        "transitionToIdle",
        "transitionToDuringGame",
        "transitionToDuringAnalysis",
        "transitionToDuringCsaGame",
        "transitionToDuringTsumeSearch",
        "transitionToDuringConsideration",
        "transitionToDuringPositionEdit",
        "enableNavigationIfAllowed",
        "Idle",
        "DuringGame",
        "DuringAnalysis",
        "DuringCsaGame",
        "DuringTsumeSearch",
        "DuringConsideration",
        "DuringPositionEdit"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stateChanged'
        QtMocHelpers::SignalData<void(enum AppState)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'transitionToIdle'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringGame'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringAnalysis'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringCsaGame'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringTsumeSearch'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringConsideration'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'transitionToDuringPositionEdit'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'enableNavigationIfAllowed'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'AppState'
        QtMocHelpers::EnumData<enum AppState>(3, 3, QMC::EnumIsScoped).add({
            {   13, AppState::Idle },
            {   14, AppState::DuringGame },
            {   15, AppState::DuringAnalysis },
            {   16, AppState::DuringCsaGame },
            {   17, AppState::DuringTsumeSearch },
            {   18, AppState::DuringConsideration },
            {   19, AppState::DuringPositionEdit },
        }),
    };
    return QtMocHelpers::metaObjectData<UiStatePolicyManager, qt_meta_tag_ZN20UiStatePolicyManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UiStatePolicyManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20UiStatePolicyManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20UiStatePolicyManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20UiStatePolicyManagerE_t>.metaTypes,
    nullptr
} };

void UiStatePolicyManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UiStatePolicyManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->stateChanged((*reinterpret_cast<std::add_pointer_t<enum AppState>>(_a[1]))); break;
        case 1: _t->transitionToIdle(); break;
        case 2: _t->transitionToDuringGame(); break;
        case 3: _t->transitionToDuringAnalysis(); break;
        case 4: _t->transitionToDuringCsaGame(); break;
        case 5: _t->transitionToDuringTsumeSearch(); break;
        case 6: _t->transitionToDuringConsideration(); break;
        case 7: _t->transitionToDuringPositionEdit(); break;
        case 8: _t->enableNavigationIfAllowed(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UiStatePolicyManager::*)(AppState )>(_a, &UiStatePolicyManager::stateChanged, 0))
            return;
    }
}

const QMetaObject *UiStatePolicyManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UiStatePolicyManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20UiStatePolicyManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int UiStatePolicyManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void UiStatePolicyManager::stateChanged(AppState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
