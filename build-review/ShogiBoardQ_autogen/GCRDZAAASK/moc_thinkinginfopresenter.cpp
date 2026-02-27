/****************************************************************************
** Meta object code from reading C++ file 'thinkinginfopresenter.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/engine/thinkinginfopresenter.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'thinkinginfopresenter.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN21ThinkingInfoPresenterE_t {};
} // unnamed namespace

template <> constexpr inline auto ThinkingInfoPresenter::qt_create_metaobjectdata<qt_meta_tag_ZN21ThinkingInfoPresenterE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ThinkingInfoPresenter",
        "thinkingInfoUpdated",
        "",
        "time",
        "depth",
        "nodes",
        "score",
        "pvKanjiStr",
        "usiPv",
        "baseSfen",
        "multipv",
        "scoreCp",
        "clearThinkingInfoRequested",
        "searchedMoveUpdated",
        "move",
        "searchDepthUpdated",
        "nodeCountUpdated",
        "npsUpdated",
        "nps",
        "hashUsageUpdated",
        "hashUsage",
        "commLogAppended",
        "log",
        "onInfoReceived",
        "line"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'thinkingInfoUpdated'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 4 }, { QMetaType::QString, 5 }, { QMetaType::QString, 6 },
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 }, { QMetaType::QString, 9 }, { QMetaType::Int, 10 },
            { QMetaType::Int, 11 },
        }}),
        // Signal 'clearThinkingInfoRequested'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchedMoveUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'searchDepthUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Signal 'nodeCountUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'npsUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Signal 'hashUsageUpdated'
        QtMocHelpers::SignalData<void(const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 },
        }}),
        // Signal 'commLogAppended'
        QtMocHelpers::SignalData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Slot 'onInfoReceived'
        QtMocHelpers::SlotData<void(const QString &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 24 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ThinkingInfoPresenter, qt_meta_tag_ZN21ThinkingInfoPresenterE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ThinkingInfoPresenter::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21ThinkingInfoPresenterE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21ThinkingInfoPresenterE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN21ThinkingInfoPresenterE_t>.metaTypes,
    nullptr
} };

void ThinkingInfoPresenter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ThinkingInfoPresenter *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->thinkingInfoUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[8])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[9]))); break;
        case 1: _t->clearThinkingInfoRequested(); break;
        case 2: _t->searchedMoveUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->searchDepthUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->nodeCountUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->npsUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->hashUsageUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->commLogAppended((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->onInfoReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & , const QString & , const QString & , const QString & , const QString & , const QString & , const QString & , int , int )>(_a, &ThinkingInfoPresenter::thinkingInfoUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)()>(_a, &ThinkingInfoPresenter::clearThinkingInfoRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::searchedMoveUpdated, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::searchDepthUpdated, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::nodeCountUpdated, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::npsUpdated, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::hashUsageUpdated, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (ThinkingInfoPresenter::*)(const QString & )>(_a, &ThinkingInfoPresenter::commLogAppended, 7))
            return;
    }
}

const QMetaObject *ThinkingInfoPresenter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ThinkingInfoPresenter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21ThinkingInfoPresenterE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ThinkingInfoPresenter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void ThinkingInfoPresenter::thinkingInfoUpdated(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, const QString & _t5, const QString & _t6, const QString & _t7, int _t8, int _t9)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4, _t5, _t6, _t7, _t8, _t9);
}

// SIGNAL 1
void ThinkingInfoPresenter::clearThinkingInfoRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ThinkingInfoPresenter::searchedMoveUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ThinkingInfoPresenter::searchDepthUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void ThinkingInfoPresenter::nodeCountUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void ThinkingInfoPresenter::npsUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void ThinkingInfoPresenter::hashUsageUpdated(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void ThinkingInfoPresenter::commLogAppended(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}
QT_WARNING_POP
