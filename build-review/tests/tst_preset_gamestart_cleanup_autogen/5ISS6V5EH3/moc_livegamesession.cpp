/****************************************************************************
** Meta object code from reading C++ file 'livegamesession.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/kifu/livegamesession.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QSet>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'livegamesession.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15LiveGameSessionE_t {};
} // unnamed namespace

template <> constexpr inline auto LiveGameSession::qt_create_metaobjectdata<qt_meta_tag_ZN15LiveGameSessionE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "LiveGameSession",
        "sessionStarted",
        "",
        "KifuBranchNode*",
        "branchPoint",
        "moveAdded",
        "ply",
        "displayText",
        "sessionCommitted",
        "newLineEnd",
        "sessionDiscarded",
        "branchMarksUpdated",
        "QSet<int>",
        "branchPlys",
        "recordModelUpdateRequired"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'sessionStarted'
        QtMocHelpers::SignalData<void(KifuBranchNode *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'moveAdded'
        QtMocHelpers::SignalData<void(int, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::QString, 7 },
        }}),
        // Signal 'sessionCommitted'
        QtMocHelpers::SignalData<void(KifuBranchNode *)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 9 },
        }}),
        // Signal 'sessionDiscarded'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'branchMarksUpdated'
        QtMocHelpers::SignalData<void(const QSet<int> &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'recordModelUpdateRequired'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LiveGameSession, qt_meta_tag_ZN15LiveGameSessionE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject LiveGameSession::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiveGameSessionE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiveGameSessionE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15LiveGameSessionE_t>.metaTypes,
    nullptr
} };

void LiveGameSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LiveGameSession *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sessionStarted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 1: _t->moveAdded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->sessionCommitted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 3: _t->sessionDiscarded(); break;
        case 4: _t->branchMarksUpdated((*reinterpret_cast<std::add_pointer_t<QSet<int>>>(_a[1]))); break;
        case 5: _t->recordModelUpdateRequired(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QSet<int> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)(KifuBranchNode * )>(_a, &LiveGameSession::sessionStarted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)(int , const QString & )>(_a, &LiveGameSession::moveAdded, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)(KifuBranchNode * )>(_a, &LiveGameSession::sessionCommitted, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)()>(_a, &LiveGameSession::sessionDiscarded, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)(const QSet<int> & )>(_a, &LiveGameSession::branchMarksUpdated, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (LiveGameSession::*)()>(_a, &LiveGameSession::recordModelUpdateRequired, 5))
            return;
    }
}

const QMetaObject *LiveGameSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LiveGameSession::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LiveGameSessionE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int LiveGameSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void LiveGameSession::sessionStarted(KifuBranchNode * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void LiveGameSession::moveAdded(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void LiveGameSession::sessionCommitted(KifuBranchNode * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void LiveGameSession::sessionDiscarded()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void LiveGameSession::branchMarksUpdated(const QSet<int> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void LiveGameSession::recordModelUpdateRequired()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
