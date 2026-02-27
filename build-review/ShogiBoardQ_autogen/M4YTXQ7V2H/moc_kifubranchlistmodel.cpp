/****************************************************************************
** Meta object code from reading C++ file 'kifubranchlistmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/models/kifubranchlistmodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kifubranchlistmodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19KifuBranchListModelE_t {};
} // unnamed namespace

template <> constexpr inline auto KifuBranchListModel::qt_create_metaobjectdata<qt_meta_tag_ZN19KifuBranchListModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KifuBranchListModel",
        "activeVidPlyChanged",
        "",
        "vid",
        "ply"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'activeVidPlyChanged'
        QtMocHelpers::SignalData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KifuBranchListModel, qt_meta_tag_ZN19KifuBranchListModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KifuBranchListModel::staticMetaObject = { {
    QMetaObject::SuperData::link<AbstractListModel<KifuBranchDisplay>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuBranchListModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuBranchListModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19KifuBranchListModelE_t>.metaTypes,
    nullptr
} };

void KifuBranchListModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KifuBranchListModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->activeVidPlyChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KifuBranchListModel::*)(int , int )>(_a, &KifuBranchListModel::activeVidPlyChanged, 0))
            return;
    }
}

const QMetaObject *KifuBranchListModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KifuBranchListModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuBranchListModelE_t>.strings))
        return static_cast<void*>(this);
    return AbstractListModel<KifuBranchDisplay>::qt_metacast(_clname);
}

int KifuBranchListModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = AbstractListModel<KifuBranchDisplay>::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void KifuBranchListModel::activeVidPlyChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}
QT_WARNING_POP
