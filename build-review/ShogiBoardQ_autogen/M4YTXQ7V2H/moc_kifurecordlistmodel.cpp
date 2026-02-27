/****************************************************************************
** Meta object code from reading C++ file 'kifurecordlistmodel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/models/kifurecordlistmodel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kifurecordlistmodel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19KifuRecordListModelE_t {};
} // unnamed namespace

template <> constexpr inline auto KifuRecordListModel::qt_create_metaobjectdata<qt_meta_tag_ZN19KifuRecordListModelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KifuRecordListModel",
        "removeLastItem",
        "",
        "removeLastItems",
        "n"
    };

    QtMocHelpers::UintData qt_methods {
        // Method 'removeLastItem'
        QtMocHelpers::MethodData<bool()>(1, 2, QMC::AccessPublic, QMetaType::Bool),
        // Method 'removeLastItems'
        QtMocHelpers::MethodData<bool(int)>(3, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KifuRecordListModel, qt_meta_tag_ZN19KifuRecordListModelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KifuRecordListModel::staticMetaObject = { {
    QMetaObject::SuperData::link<AbstractListModel<KifuDisplay>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuRecordListModelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuRecordListModelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19KifuRecordListModelE_t>.metaTypes,
    nullptr
} };

void KifuRecordListModel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KifuRecordListModel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: { bool _r = _t->removeLastItem();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 1: { bool _r = _t->removeLastItems((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

const QMetaObject *KifuRecordListModel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KifuRecordListModel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19KifuRecordListModelE_t>.strings))
        return static_cast<void*>(this);
    return AbstractListModel<KifuDisplay>::qt_metacast(_clname);
}

int KifuRecordListModel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = AbstractListModel<KifuDisplay>::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}
QT_WARNING_POP
