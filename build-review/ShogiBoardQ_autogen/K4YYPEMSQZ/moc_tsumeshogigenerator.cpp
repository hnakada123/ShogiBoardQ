/****************************************************************************
** Meta object code from reading C++ file 'tsumeshogigenerator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/analysis/tsumeshogigenerator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'tsumeshogigenerator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN19TsumeshogiGeneratorE_t {};
} // unnamed namespace

template <> constexpr inline auto TsumeshogiGenerator::qt_create_metaobjectdata<qt_meta_tag_ZN19TsumeshogiGeneratorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TsumeshogiGenerator",
        "positionFound",
        "",
        "sfen",
        "pv",
        "progressUpdated",
        "tried",
        "found",
        "elapsedMs",
        "finished",
        "errorOccurred",
        "message",
        "onCheckmateSolved",
        "onCheckmateNoMate",
        "onCheckmateNotImplemented",
        "onCheckmateUnknown",
        "onSafetyTimeout",
        "onProgressTimerTimeout"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'positionFound'
        QtMocHelpers::SignalData<void(const QString &, const QStringList &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QStringList, 4 },
        }}),
        // Signal 'progressUpdated'
        QtMocHelpers::SignalData<void(int, int, qint64)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 }, { QMetaType::Int, 7 }, { QMetaType::LongLong, 8 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Slot 'onCheckmateSolved'
        QtMocHelpers::SlotData<void(const QStringList &)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QStringList, 4 },
        }}),
        // Slot 'onCheckmateNoMate'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCheckmateNotImplemented'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCheckmateUnknown'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSafetyTimeout'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProgressTimerTimeout'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TsumeshogiGenerator, qt_meta_tag_ZN19TsumeshogiGeneratorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TsumeshogiGenerator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19TsumeshogiGeneratorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19TsumeshogiGeneratorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN19TsumeshogiGeneratorE_t>.metaTypes,
    nullptr
} };

void TsumeshogiGenerator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TsumeshogiGenerator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->positionFound((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[2]))); break;
        case 1: _t->progressUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[3]))); break;
        case 2: _t->finished(); break;
        case 3: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->onCheckmateSolved((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 5: _t->onCheckmateNoMate(); break;
        case 6: _t->onCheckmateNotImplemented(); break;
        case 7: _t->onCheckmateUnknown(); break;
        case 8: _t->onSafetyTimeout(); break;
        case 9: _t->onProgressTimerTimeout(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TsumeshogiGenerator::*)(const QString & , const QStringList & )>(_a, &TsumeshogiGenerator::positionFound, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TsumeshogiGenerator::*)(int , int , qint64 )>(_a, &TsumeshogiGenerator::progressUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TsumeshogiGenerator::*)()>(_a, &TsumeshogiGenerator::finished, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TsumeshogiGenerator::*)(const QString & )>(_a, &TsumeshogiGenerator::errorOccurred, 3))
            return;
    }
}

const QMetaObject *TsumeshogiGenerator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TsumeshogiGenerator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN19TsumeshogiGeneratorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int TsumeshogiGenerator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void TsumeshogiGenerator::positionFound(const QString & _t1, const QStringList & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void TsumeshogiGenerator::progressUpdated(int _t1, int _t2, qint64 _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}

// SIGNAL 2
void TsumeshogiGenerator::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void TsumeshogiGenerator::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
