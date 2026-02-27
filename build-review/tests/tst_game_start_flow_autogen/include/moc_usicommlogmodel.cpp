/****************************************************************************
** Meta object code from reading C++ file 'usicommlogmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/engine/usicommlogmodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'usicommlogmodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15UsiCommLogModelE_t {};
} // unnamed namespace

template <> constexpr inline auto UsiCommLogModel::qt_create_metaobjectdata<qt_meta_tag_ZN15UsiCommLogModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UsiCommLogModel",
        "engineNameChanged",
        "",
        "predictiveMoveChanged",
        "searchedMoveChanged",
        "searchDepthChanged",
        "nodeCountChanged",
        "nodesPerSecondChanged",
        "hashUsageChanged",
        "usiCommLogChanged",
        "setEngineName",
        "engineName",
        "setPredictiveMove",
        "predictiveMove",
        "setSearchedMove",
        "searchedMove",
        "setSearchDepth",
        "searchDepth",
        "setNodeCount",
        "nodeCount",
        "setNodesPerSecond",
        "nodesPerSecond",
        "setHashUsage",
        "hashUsage",
        "usiCommLog"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'engineNameChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'predictiveMoveChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchedMoveChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchDepthChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nodeCountChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'nodesPerSecondChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'hashUsageChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'usiCommLogChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setEngineName'
        QtMocHelpers::SlotData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Slot 'setPredictiveMove'
        QtMocHelpers::SlotData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 13 },
        }}),
        // Slot 'setSearchedMove'
        QtMocHelpers::SlotData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'setSearchDepth'
        QtMocHelpers::SlotData<void(const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Slot 'setNodeCount'
        QtMocHelpers::SlotData<void(const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'setNodesPerSecond'
        QtMocHelpers::SlotData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Slot 'setHashUsage'
        QtMocHelpers::SlotData<void(const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'engineName'
        QtMocHelpers::PropertyData<QString>(11, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'predictiveMove'
        QtMocHelpers::PropertyData<QString>(13, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'searchedMove'
        QtMocHelpers::PropertyData<QString>(15, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 2),
        // property 'searchDepth'
        QtMocHelpers::PropertyData<QString>(17, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 3),
        // property 'nodeCount'
        QtMocHelpers::PropertyData<QString>(19, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 4),
        // property 'nodesPerSecond'
        QtMocHelpers::PropertyData<QString>(21, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 5),
        // property 'hashUsage'
        QtMocHelpers::PropertyData<QString>(23, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 6),
        // property 'usiCommLog'
        QtMocHelpers::PropertyData<QString>(24, QMetaType::QString, QMC::DefaultPropertyFlags, 7),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UsiCommLogModel, qt_meta_tag_ZN15UsiCommLogModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UsiCommLogModel::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UsiCommLogModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UsiCommLogModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15UsiCommLogModelE_t>.metaTypes,
    nullptr
} };

void UsiCommLogModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UsiCommLogModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->engineNameChanged(); break;
        case 1: _t->predictiveMoveChanged(); break;
        case 2: _t->searchedMoveChanged(); break;
        case 3: _t->searchDepthChanged(); break;
        case 4: _t->nodeCountChanged(); break;
        case 5: _t->nodesPerSecondChanged(); break;
        case 6: _t->hashUsageChanged(); break;
        case 7: _t->usiCommLogChanged(); break;
        case 8: _t->setEngineName((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->setPredictiveMove((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->setSearchedMove((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->setSearchDepth((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->setNodeCount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->setNodesPerSecond((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->setHashUsage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::engineNameChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::predictiveMoveChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::searchedMoveChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::searchDepthChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::nodeCountChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::nodesPerSecondChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::hashUsageChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiCommLogModel::*)()>(_a, &UsiCommLogModel::usiCommLogChanged, 7))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->engineName(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->predictiveMove(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->searchedMove(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->searchDepth(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->nodeCount(); break;
        case 5: *reinterpret_cast<QString*>(_v) = _t->nodesPerSecond(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->hashUsage(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->usiCommLog(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setEngineName(*reinterpret_cast<QString*>(_v)); break;
        case 1: _t->setPredictiveMove(*reinterpret_cast<QString*>(_v)); break;
        case 2: _t->setSearchedMove(*reinterpret_cast<QString*>(_v)); break;
        case 3: _t->setSearchDepth(*reinterpret_cast<QString*>(_v)); break;
        case 4: _t->setNodeCount(*reinterpret_cast<QString*>(_v)); break;
        case 5: _t->setNodesPerSecond(*reinterpret_cast<QString*>(_v)); break;
        case 6: _t->setHashUsage(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *UsiCommLogModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UsiCommLogModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UsiCommLogModelE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int UsiCommLogModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void UsiCommLogModel::engineNameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void UsiCommLogModel::predictiveMoveChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void UsiCommLogModel::searchedMoveChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void UsiCommLogModel::searchDepthChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void UsiCommLogModel::nodeCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void UsiCommLogModel::nodesPerSecondChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void UsiCommLogModel::hashUsageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void UsiCommLogModel::usiCommLogChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}
QT_WARNING_POP
