/****************************************************************************
** Meta object code from reading C++ file 'considerationwiring.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/wiring/considerationwiring.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'considerationwiring.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19ConsiderationWiringE_t {};
} // unnamed namespace

template <> constexpr inline auto ConsiderationWiring::qt_create_metaobjectdata<qt_meta_tag_ZN19ConsiderationWiringE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConsiderationWiring",
        "displayConsiderationDialog",
        "",
        "onEngineSettingsRequested",
        "engineNumber",
        "engineName",
        "onEngineChanged",
        "engineIndex",
        "onModeStarted",
        "onTimeSettingsReady",
        "unlimited",
        "byoyomiSec",
        "onDialogMultiPVReady",
        "multiPV",
        "onMultiPVChangeRequested",
        "value",
        "updateArrows",
        "onShowArrowsChanged",
        "checked",
        "updatePositionIfNeeded",
        "row",
        "newPosition",
        "const QList<ShogiMove>*",
        "gameMoves",
        "KifuRecordListModel*",
        "kifuRecordModel",
        "handleStopRequest"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'displayConsiderationDialog'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onEngineSettingsRequested'
        QtMocHelpers::SlotData<void(int, const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 }, { QMetaType::QString, 5 },
        }}),
        // Slot 'onEngineChanged'
        QtMocHelpers::SlotData<void(int, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::QString, 5 },
        }}),
        // Slot 'onModeStarted'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onTimeSettingsReady'
        QtMocHelpers::SlotData<void(bool, int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 }, { QMetaType::Int, 11 },
        }}),
        // Slot 'onDialogMultiPVReady'
        QtMocHelpers::SlotData<void(int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 13 },
        }}),
        // Slot 'onMultiPVChangeRequested'
        QtMocHelpers::SlotData<void(int)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'updateArrows'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onShowArrowsChanged'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'updatePositionIfNeeded'
        QtMocHelpers::SlotData<bool(int, const QString &, const QVector<ShogiMove> *, KifuRecordListModel *)>(19, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 20 }, { QMetaType::QString, 21 }, { 0x80000000 | 22, 23 }, { 0x80000000 | 24, 25 },
        }}),
        // Slot 'handleStopRequest'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConsiderationWiring, qt_meta_tag_ZN19ConsiderationWiringE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConsiderationWiring::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ConsiderationWiringE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ConsiderationWiringE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19ConsiderationWiringE_t>.metaTypes,
    nullptr
} };

void ConsiderationWiring::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConsiderationWiring *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->displayConsiderationDialog(); break;
        case 1: _t->onEngineSettingsRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->onEngineChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->onModeStarted(); break;
        case 4: _t->onTimeSettingsReady((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->onDialogMultiPVReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onMultiPVChangeRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->updateArrows(); break;
        case 8: _t->onShowArrowsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: { bool _r = _t->updatePositionIfNeeded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<const QList<ShogiMove>*>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<KifuRecordListModel*>>(_a[4])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 10: _t->handleStopRequest(); break;
        default: ;
        }
    }
}

const QMetaObject *ConsiderationWiring::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConsiderationWiring::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19ConsiderationWiringE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ConsiderationWiring::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
