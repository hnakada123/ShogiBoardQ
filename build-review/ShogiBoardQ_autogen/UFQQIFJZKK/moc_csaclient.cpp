/****************************************************************************
** Meta object code from reading C++ file 'csaclient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/network/csaclient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'csaclient.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9CsaClientE_t {};
} // unnamed namespace

template <> constexpr inline auto CsaClient::qt_create_metaobjectdata<qt_meta_tag_ZN9CsaClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CsaClient",
        "connectionStateChanged",
        "",
        "CsaClient::ConnectionState",
        "state",
        "errorOccurred",
        "message",
        "loginSucceeded",
        "loginFailed",
        "reason",
        "logoutCompleted",
        "gameSummaryReceived",
        "CsaClient::GameSummary",
        "summary",
        "gameStarted",
        "gameId",
        "gameRejected",
        "rejector",
        "moveReceived",
        "move",
        "consumedTimeMs",
        "moveConfirmed",
        "gameEnded",
        "CsaClient::GameResult",
        "result",
        "CsaClient::GameEndCause",
        "cause",
        "gameInterrupted",
        "rawMessageReceived",
        "rawMessageSent",
        "onSocketConnected",
        "onSocketDisconnected",
        "onSocketError",
        "QAbstractSocket::SocketError",
        "error",
        "onReadyRead",
        "onConnectionTimeout",
        "ConnectionState",
        "Disconnected",
        "Connecting",
        "Connected",
        "LoggedIn",
        "WaitingForGame",
        "GameReady",
        "InGame",
        "GameOver",
        "GameResult",
        "Win",
        "Lose",
        "Draw",
        "Censored",
        "Chudan",
        "Unknown",
        "GameEndCause",
        "Resign",
        "TimeUp",
        "IllegalMove",
        "Sennichite",
        "OuteSennichite",
        "Jishogi",
        "MaxMoves",
        "IllegalAction"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'connectionStateChanged'
        QtMocHelpers::SignalData<void(CsaClient::ConnectionState)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'loginSucceeded'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loginFailed'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'logoutCompleted'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gameSummaryReceived'
        QtMocHelpers::SignalData<void(const CsaClient::GameSummary &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'gameStarted'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Signal 'gameRejected'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 }, { QMetaType::QString, 17 },
        }}),
        // Signal 'moveReceived'
        QtMocHelpers::SignalData<void(const QString &, int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 19 }, { QMetaType::Int, 20 },
        }}),
        // Signal 'moveConfirmed'
        QtMocHelpers::SignalData<void(const QString &, int)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 19 }, { QMetaType::Int, 20 },
        }}),
        // Signal 'gameEnded'
        QtMocHelpers::SignalData<void(CsaClient::GameResult, CsaClient::GameEndCause, int)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 }, { 0x80000000 | 25, 26 }, { QMetaType::Int, 20 },
        }}),
        // Signal 'gameInterrupted'
        QtMocHelpers::SignalData<void()>(27, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'rawMessageReceived'
        QtMocHelpers::SignalData<void(const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'rawMessageSent'
        QtMocHelpers::SignalData<void(const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Slot 'onSocketConnected'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketDisconnected'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSocketError'
        QtMocHelpers::SlotData<void(QAbstractSocket::SocketError)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 33, 34 },
        }}),
        // Slot 'onReadyRead'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onConnectionTimeout'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'ConnectionState'
        QtMocHelpers::EnumData<enum ConnectionState>(37, 37, QMC::EnumIsScoped).add({
            {   38, ConnectionState::Disconnected },
            {   39, ConnectionState::Connecting },
            {   40, ConnectionState::Connected },
            {   41, ConnectionState::LoggedIn },
            {   42, ConnectionState::WaitingForGame },
            {   43, ConnectionState::GameReady },
            {   44, ConnectionState::InGame },
            {   45, ConnectionState::GameOver },
        }),
        // enum 'GameResult'
        QtMocHelpers::EnumData<enum GameResult>(46, 46, QMC::EnumIsScoped).add({
            {   47, GameResult::Win },
            {   48, GameResult::Lose },
            {   49, GameResult::Draw },
            {   50, GameResult::Censored },
            {   51, GameResult::Chudan },
            {   52, GameResult::Unknown },
        }),
        // enum 'GameEndCause'
        QtMocHelpers::EnumData<enum GameEndCause>(53, 53, QMC::EnumIsScoped).add({
            {   54, GameEndCause::Resign },
            {   55, GameEndCause::TimeUp },
            {   56, GameEndCause::IllegalMove },
            {   57, GameEndCause::Sennichite },
            {   58, GameEndCause::OuteSennichite },
            {   59, GameEndCause::Jishogi },
            {   60, GameEndCause::MaxMoves },
            {   51, GameEndCause::Chudan },
            {   61, GameEndCause::IllegalAction },
            {   52, GameEndCause::Unknown },
        }),
    };
    return QtMocHelpers::metaObjectData<CsaClient, qt_meta_tag_ZN9CsaClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CsaClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9CsaClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9CsaClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9CsaClientE_t>.metaTypes,
    nullptr
} };

void CsaClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CsaClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->connectionStateChanged((*reinterpret_cast<std::add_pointer_t<CsaClient::ConnectionState>>(_a[1]))); break;
        case 1: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->loginSucceeded(); break;
        case 3: _t->loginFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->logoutCompleted(); break;
        case 5: _t->gameSummaryReceived((*reinterpret_cast<std::add_pointer_t<CsaClient::GameSummary>>(_a[1]))); break;
        case 6: _t->gameStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->gameRejected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->moveReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->moveConfirmed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 10: _t->gameEnded((*reinterpret_cast<std::add_pointer_t<CsaClient::GameResult>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<CsaClient::GameEndCause>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 11: _t->gameInterrupted(); break;
        case 12: _t->rawMessageReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->rawMessageSent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->onSocketConnected(); break;
        case 15: _t->onSocketDisconnected(); break;
        case 16: _t->onSocketError((*reinterpret_cast<std::add_pointer_t<QAbstractSocket::SocketError>>(_a[1]))); break;
        case 17: _t->onReadyRead(); break;
        case 18: _t->onConnectionTimeout(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 16:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAbstractSocket::SocketError >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(CsaClient::ConnectionState )>(_a, &CsaClient::connectionStateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & )>(_a, &CsaClient::errorOccurred, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)()>(_a, &CsaClient::loginSucceeded, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & )>(_a, &CsaClient::loginFailed, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)()>(_a, &CsaClient::logoutCompleted, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const CsaClient::GameSummary & )>(_a, &CsaClient::gameSummaryReceived, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & )>(_a, &CsaClient::gameStarted, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & , const QString & )>(_a, &CsaClient::gameRejected, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & , int )>(_a, &CsaClient::moveReceived, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & , int )>(_a, &CsaClient::moveConfirmed, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(CsaClient::GameResult , CsaClient::GameEndCause , int )>(_a, &CsaClient::gameEnded, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)()>(_a, &CsaClient::gameInterrupted, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & )>(_a, &CsaClient::rawMessageReceived, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaClient::*)(const QString & )>(_a, &CsaClient::rawMessageSent, 13))
            return;
    }
}

const QMetaObject *CsaClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CsaClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9CsaClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CsaClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 19)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void CsaClient::connectionStateChanged(CsaClient::ConnectionState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void CsaClient::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void CsaClient::loginSucceeded()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void CsaClient::loginFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void CsaClient::logoutCompleted()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void CsaClient::gameSummaryReceived(const CsaClient::GameSummary & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void CsaClient::gameStarted(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void CsaClient::gameRejected(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void CsaClient::moveReceived(const QString & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}

// SIGNAL 9
void CsaClient::moveConfirmed(const QString & _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}

// SIGNAL 10
void CsaClient::gameEnded(CsaClient::GameResult _t1, CsaClient::GameEndCause _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3);
}

// SIGNAL 11
void CsaClient::gameInterrupted()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void CsaClient::rawMessageReceived(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void CsaClient::rawMessageSent(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}
QT_WARNING_POP
