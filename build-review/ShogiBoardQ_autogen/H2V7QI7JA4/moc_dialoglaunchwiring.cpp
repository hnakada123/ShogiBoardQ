/****************************************************************************
** Meta object code from reading C++ file 'dialoglaunchwiring.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/wiring/dialoglaunchwiring.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dialoglaunchwiring.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18DialogLaunchWiringE_t {};
} // unnamed namespace

template <> constexpr inline auto DialogLaunchWiring::qt_create_metaobjectdata<qt_meta_tag_ZN18DialogLaunchWiringE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DialogLaunchWiring",
        "sfenCollectionPositionSelected",
        "",
        "sfen",
        "displayVersionInformation",
        "displayEngineSettingsDialog",
        "displayPromotionDialog",
        "displayJishogiScoreDialog",
        "handleNyugyokuDeclaration",
        "displayTsumeShogiSearchDialog",
        "displayTsumeshogiGeneratorDialog",
        "displayMenuWindow",
        "displayCsaGameDialog",
        "displayKifuAnalysisDialog",
        "displaySfenCollectionViewer",
        "onCsaEngineScoreUpdatedInternal",
        "scoreCp",
        "ply"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'sfenCollectionPositionSelected'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'displayVersionInformation'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayEngineSettingsDialog'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayPromotionDialog'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayJishogiScoreDialog'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleNyugyokuDeclaration'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayTsumeShogiSearchDialog'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayTsumeshogiGeneratorDialog'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayMenuWindow'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayCsaGameDialog'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displayKifuAnalysisDialog'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'displaySfenCollectionViewer'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onCsaEngineScoreUpdatedInternal'
        QtMocHelpers::SlotData<void(int, int)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 }, { QMetaType::Int, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DialogLaunchWiring, qt_meta_tag_ZN18DialogLaunchWiringE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DialogLaunchWiring::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DialogLaunchWiringE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DialogLaunchWiringE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18DialogLaunchWiringE_t>.metaTypes,
    nullptr
} };

void DialogLaunchWiring::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DialogLaunchWiring *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sfenCollectionPositionSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->displayVersionInformation(); break;
        case 2: _t->displayEngineSettingsDialog(); break;
        case 3: _t->displayPromotionDialog(); break;
        case 4: _t->displayJishogiScoreDialog(); break;
        case 5: _t->handleNyugyokuDeclaration(); break;
        case 6: _t->displayTsumeShogiSearchDialog(); break;
        case 7: _t->displayTsumeshogiGeneratorDialog(); break;
        case 8: _t->displayMenuWindow(); break;
        case 9: _t->displayCsaGameDialog(); break;
        case 10: _t->displayKifuAnalysisDialog(); break;
        case 11: _t->displaySfenCollectionViewer(); break;
        case 12: _t->onCsaEngineScoreUpdatedInternal((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DialogLaunchWiring::*)(const QString & )>(_a, &DialogLaunchWiring::sfenCollectionPositionSelected, 0))
            return;
    }
}

const QMetaObject *DialogLaunchWiring::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DialogLaunchWiring::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18DialogLaunchWiringE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DialogLaunchWiring::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void DialogLaunchWiring::sfenCollectionPositionSelected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
