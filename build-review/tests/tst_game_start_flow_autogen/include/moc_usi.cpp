/****************************************************************************
** Meta object code from reading C++ file 'usi.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/engine/usi.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'usi.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN3UsiE_t {};
} // unnamed namespace

template <> constexpr inline auto Usi::qt_create_metaobjectdata<qt_meta_tag_ZN3UsiE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Usi",
        "bestMoveResignReceived",
        "",
        "bestMoveWinReceived",
        "checkmateSolved",
        "pvMoves",
        "checkmateNoMate",
        "checkmateNotImplemented",
        "checkmateUnknown",
        "errorOccurred",
        "message",
        "usiOkReceived",
        "readyOkReceived",
        "bestMoveReceived",
        "infoLineReceived",
        "line",
        "thinkingInfoUpdated",
        "time",
        "depth",
        "nodes",
        "score",
        "pvKanjiStr",
        "usiPv",
        "baseSfen",
        "multipv",
        "scoreCp",
        "onProcessError",
        "QProcess::ProcessError",
        "error",
        "onCommandSent",
        "command",
        "onDataReceived",
        "onStderrReceived",
        "onSearchedMoveUpdated",
        "move",
        "onSearchDepthUpdated",
        "onNodeCountUpdated",
        "onNpsUpdated",
        "nps",
        "onHashUsageUpdated",
        "hashUsage",
        "onCommLogAppended",
        "log",
        "onClearThinkingInfoRequested",
        "onAnalysisStopTimeout",
        "onConsiderationStopTimeout",
        "onThinkingInfoUpdated"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'bestMoveResignReceived'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bestMoveWinReceived'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkmateSolved'
        QtMocHelpers::SignalData<void(const QStringList &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 5 },
        }}),
        // Signal 'checkmateNoMate'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkmateNotImplemented'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkmateUnknown'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'usiOkReceived'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'readyOkReceived'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bestMoveReceived'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'infoLineReceived'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Signal 'thinkingInfoUpdated'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, int, int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 }, { QMetaType::QString, 18 }, { QMetaType::QString, 19 }, { QMetaType::QString, 20 },
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 }, { QMetaType::QString, 23 }, { QMetaType::Int, 24 },
            { QMetaType::Int, 25 },
        }}),
        // Slot 'onProcessError'
        QtMocHelpers::SlotData<void(QProcess::ProcessError, const QString &)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 27, 28 }, { QMetaType::QString, 10 },
        }}),
        // Slot 'onCommandSent'
        QtMocHelpers::SlotData<void(const QString &)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 30 },
        }}),
        // Slot 'onDataReceived'
        QtMocHelpers::SlotData<void(const QString &)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'onStderrReceived'
        QtMocHelpers::SlotData<void(const QString &)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'onSearchedMoveUpdated'
        QtMocHelpers::SlotData<void(const QString &)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 34 },
        }}),
        // Slot 'onSearchDepthUpdated'
        QtMocHelpers::SlotData<void(const QString &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'onNodeCountUpdated'
        QtMocHelpers::SlotData<void(const QString &)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'onNpsUpdated'
        QtMocHelpers::SlotData<void(const QString &)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 38 },
        }}),
        // Slot 'onHashUsageUpdated'
        QtMocHelpers::SlotData<void(const QString &)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 40 },
        }}),
        // Slot 'onCommLogAppended'
        QtMocHelpers::SlotData<void(const QString &)>(41, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 42 },
        }}),
        // Slot 'onClearThinkingInfoRequested'
        QtMocHelpers::SlotData<void()>(43, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAnalysisStopTimeout'
        QtMocHelpers::SlotData<void()>(44, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onConsiderationStopTimeout'
        QtMocHelpers::SlotData<void()>(45, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onThinkingInfoUpdated'
        QtMocHelpers::SlotData<void(const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, const QString &, int, int)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 17 }, { QMetaType::QString, 18 }, { QMetaType::QString, 19 }, { QMetaType::QString, 20 },
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 }, { QMetaType::QString, 23 }, { QMetaType::Int, 24 },
            { QMetaType::Int, 25 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Usi, qt_meta_tag_ZN3UsiE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Usi::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3UsiE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3UsiE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3UsiE_t>.metaTypes,
    nullptr
} };

void Usi::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Usi *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->bestMoveResignReceived(); break;
        case 1: _t->bestMoveWinReceived(); break;
        case 2: _t->checkmateSolved((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 3: _t->checkmateNoMate(); break;
        case 4: _t->checkmateNotImplemented(); break;
        case 5: _t->checkmateUnknown(); break;
        case 6: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->usiOkReceived(); break;
        case 8: _t->readyOkReceived(); break;
        case 9: _t->bestMoveReceived(); break;
        case 10: _t->infoLineReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->thinkingInfoUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[8])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[9]))); break;
        case 12: _t->onProcessError((*reinterpret_cast<std::add_pointer_t<QProcess::ProcessError>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 13: _t->onCommandSent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->onDataReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->onStderrReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->onSearchedMoveUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->onSearchDepthUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->onNodeCountUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->onNpsUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->onHashUsageUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->onCommLogAppended((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->onClearThinkingInfoRequested(); break;
        case 23: _t->onAnalysisStopTimeout(); break;
        case 24: _t->onConsiderationStopTimeout(); break;
        case 25: _t->onThinkingInfoUpdated((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[7])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[8])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[9]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::bestMoveResignReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::bestMoveWinReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)(const QStringList & )>(_a, &Usi::checkmateSolved, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::checkmateNoMate, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::checkmateNotImplemented, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::checkmateUnknown, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)(const QString & )>(_a, &Usi::errorOccurred, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::usiOkReceived, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::readyOkReceived, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)()>(_a, &Usi::bestMoveReceived, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)(const QString & )>(_a, &Usi::infoLineReceived, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (Usi::*)(const QString & , const QString & , const QString & , const QString & , const QString & , const QString & , const QString & , int , int )>(_a, &Usi::thinkingInfoUpdated, 11))
            return;
    }
}

const QMetaObject *Usi::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Usi::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3UsiE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Usi::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 26)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 26)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 26;
    }
    return _id;
}

// SIGNAL 0
void Usi::bestMoveResignReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Usi::bestMoveWinReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Usi::checkmateSolved(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void Usi::checkmateNoMate()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Usi::checkmateNotImplemented()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Usi::checkmateUnknown()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Usi::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void Usi::usiOkReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Usi::readyOkReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Usi::bestMoveReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void Usi::infoLineReceived(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void Usi::thinkingInfoUpdated(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, const QString & _t5, const QString & _t6, const QString & _t7, int _t8, int _t9)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2, _t3, _t4, _t5, _t6, _t7, _t8, _t9);
}
QT_WARNING_POP
