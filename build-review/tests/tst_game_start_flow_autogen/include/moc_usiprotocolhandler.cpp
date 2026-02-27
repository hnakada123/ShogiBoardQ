/****************************************************************************
** Meta object code from reading C++ file 'usiprotocolhandler.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/engine/usiprotocolhandler.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'usiprotocolhandler.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18UsiProtocolHandlerE_t {};
} // unnamed namespace

template <> constexpr inline auto UsiProtocolHandler::qt_create_metaobjectdata<qt_meta_tag_ZN18UsiProtocolHandlerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UsiProtocolHandler",
        "usiOkReceived",
        "",
        "readyOkReceived",
        "bestMoveReceived",
        "bestMoveResignReceived",
        "bestMoveWinReceived",
        "stopOrPonderhitSent",
        "errorOccurred",
        "message",
        "infoLineReceived",
        "line",
        "checkmateSolved",
        "pvMoves",
        "checkmateNoMate",
        "checkmateNotImplemented",
        "checkmateUnknown",
        "onDataReceived"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'usiOkReceived'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'readyOkReceived'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bestMoveReceived'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bestMoveResignReceived'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bestMoveWinReceived'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'stopOrPonderhitSent'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'infoLineReceived'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Signal 'checkmateSolved'
        QtMocHelpers::SignalData<void(const QStringList &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 13 },
        }}),
        // Signal 'checkmateNoMate'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkmateNotImplemented'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkmateUnknown'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onDataReceived'
        QtMocHelpers::SlotData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UsiProtocolHandler, qt_meta_tag_ZN18UsiProtocolHandlerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UsiProtocolHandler::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18UsiProtocolHandlerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18UsiProtocolHandlerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18UsiProtocolHandlerE_t>.metaTypes,
    nullptr
} };

void UsiProtocolHandler::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UsiProtocolHandler *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->usiOkReceived(); break;
        case 1: _t->readyOkReceived(); break;
        case 2: _t->bestMoveReceived(); break;
        case 3: _t->bestMoveResignReceived(); break;
        case 4: _t->bestMoveWinReceived(); break;
        case 5: _t->stopOrPonderhitSent(); break;
        case 6: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->infoLineReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->checkmateSolved((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 9: _t->checkmateNoMate(); break;
        case 10: _t->checkmateNotImplemented(); break;
        case 11: _t->checkmateUnknown(); break;
        case 12: _t->onDataReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::usiOkReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::readyOkReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::bestMoveReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::bestMoveResignReceived, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::bestMoveWinReceived, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::stopOrPonderhitSent, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)(const QString & )>(_a, &UsiProtocolHandler::errorOccurred, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)(const QString & )>(_a, &UsiProtocolHandler::infoLineReceived, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)(const QStringList & )>(_a, &UsiProtocolHandler::checkmateSolved, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::checkmateNoMate, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::checkmateNotImplemented, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (UsiProtocolHandler::*)()>(_a, &UsiProtocolHandler::checkmateUnknown, 11))
            return;
    }
}

const QMetaObject *UsiProtocolHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UsiProtocolHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18UsiProtocolHandlerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int UsiProtocolHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void UsiProtocolHandler::usiOkReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void UsiProtocolHandler::readyOkReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void UsiProtocolHandler::bestMoveReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void UsiProtocolHandler::bestMoveResignReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void UsiProtocolHandler::bestMoveWinReceived()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void UsiProtocolHandler::stopOrPonderhitSent()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void UsiProtocolHandler::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void UsiProtocolHandler::infoLineReceived(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void UsiProtocolHandler::checkmateSolved(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void UsiProtocolHandler::checkmateNoMate()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void UsiProtocolHandler::checkmateNotImplemented()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void UsiProtocolHandler::checkmateUnknown()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}
QT_WARNING_POP
