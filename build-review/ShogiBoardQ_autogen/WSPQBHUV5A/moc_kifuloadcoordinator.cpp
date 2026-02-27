/****************************************************************************
** Meta object code from reading C++ file 'kifuloadcoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/kifu/kifuloadcoordinator.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kifuloadcoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19KifuLoadCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto KifuLoadCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN19KifuLoadCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KifuLoadCoordinator",
        "errorOccurred",
        "",
        "errorMessage",
        "displayGameRecord",
        "QList<KifDisplayItem>",
        "disp",
        "syncBoardAndHighlightsAtRow",
        "ply1",
        "enableArrowButtons",
        "setupBranchCandidatesWiring",
        "gameInfoPopulated",
        "QList<KifGameInfoItem>",
        "items",
        "branchTreeBuilt"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'displayGameRecord'
        QtMocHelpers::SignalData<void(const QList<KifDisplayItem>)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Signal 'syncBoardAndHighlightsAtRow'
        QtMocHelpers::SignalData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Signal 'enableArrowButtons'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'setupBranchCandidatesWiring'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gameInfoPopulated'
        QtMocHelpers::SignalData<void(const QList<KifGameInfoItem> &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'branchTreeBuilt'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KifuLoadCoordinator, qt_meta_tag_ZN19KifuLoadCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KifuLoadCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuLoadCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuLoadCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19KifuLoadCoordinatorE_t>.metaTypes,
    nullptr
} };

void KifuLoadCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KifuLoadCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->displayGameRecord((*reinterpret_cast<std::add_pointer_t<QList<KifDisplayItem>>>(_a[1]))); break;
        case 2: _t->syncBoardAndHighlightsAtRow((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->enableArrowButtons(); break;
        case 4: _t->setupBranchCandidatesWiring(); break;
        case 5: _t->gameInfoPopulated((*reinterpret_cast<std::add_pointer_t<QList<KifGameInfoItem>>>(_a[1]))); break;
        case 6: _t->branchTreeBuilt(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<KifDisplayItem> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)(const QString & )>(_a, &KifuLoadCoordinator::errorOccurred, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)(const QList<KifDisplayItem> )>(_a, &KifuLoadCoordinator::displayGameRecord, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)(int )>(_a, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)()>(_a, &KifuLoadCoordinator::enableArrowButtons, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)()>(_a, &KifuLoadCoordinator::setupBranchCandidatesWiring, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)(const QList<KifGameInfoItem> & )>(_a, &KifuLoadCoordinator::gameInfoPopulated, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuLoadCoordinator::*)()>(_a, &KifuLoadCoordinator::branchTreeBuilt, 6))
            return;
    }
}

const QMetaObject *KifuLoadCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KifuLoadCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuLoadCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int KifuLoadCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void KifuLoadCoordinator::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KifuLoadCoordinator::displayGameRecord(const QList<KifDisplayItem> _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void KifuLoadCoordinator::syncBoardAndHighlightsAtRow(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void KifuLoadCoordinator::enableArrowButtons()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void KifuLoadCoordinator::setupBranchCandidatesWiring()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void KifuLoadCoordinator::gameInfoPopulated(const QList<KifGameInfoItem> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void KifuLoadCoordinator::branchTreeBuilt()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
