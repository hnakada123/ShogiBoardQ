/****************************************************************************
** Meta object code from reading C++ file 'csagamecoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/network/csagamecoordinator.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'csagamecoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18CsaGameCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto CsaGameCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN18CsaGameCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "CsaGameCoordinator",
        "gameStateChanged",
        "",
        "CsaGameCoordinator::GameState",
        "state",
        "errorOccurred",
        "message",
        "gameStarted",
        "blackName",
        "whiteName",
        "gameEnded",
        "result",
        "cause",
        "consumedTimeMs",
        "moveMade",
        "csaMove",
        "usiMove",
        "prettyMove",
        "turnChanged",
        "isMyTurn",
        "moveHighlightRequested",
        "QPoint",
        "from",
        "to",
        "logMessage",
        "isError",
        "csaCommLogAppended",
        "line",
        "engineScoreUpdated",
        "scoreCp",
        "ply",
        "onHumanMove",
        "promote",
        "onResign",
        "onConnectionStateChanged",
        "CsaClient::ConnectionState",
        "onClientError",
        "onLoginSucceeded",
        "onLoginFailed",
        "reason",
        "onLogoutCompleted",
        "onGameSummaryReceived",
        "CsaClient::GameSummary",
        "summary",
        "onGameStarted",
        "gameId",
        "onGameRejected",
        "rejector",
        "onMoveReceived",
        "move",
        "onMoveConfirmed",
        "onClientGameEnded",
        "CsaClient::GameResult",
        "CsaClient::GameEndCause",
        "onGameInterrupted",
        "onRawMessageReceived",
        "onRawMessageSent",
        "onEngineBestMoveReceived",
        "onEngineResign",
        "GameState",
        "Idle",
        "Connecting",
        "LoggingIn",
        "WaitingForMatch",
        "WaitingForAgree",
        "InGame",
        "GameOver",
        "Error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'gameStateChanged'
        QtMocHelpers::SignalData<void(CsaGameCoordinator::GameState)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'gameStarted'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 }, { QMetaType::QString, 9 },
        }}),
        // Signal 'gameEnded'
        QtMocHelpers::SignalData<void(const QString &, const QString &, int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { QMetaType::QString, 12 }, { QMetaType::Int, 13 },
        }}),
        // Signal 'moveMade'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QString &, int)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 }, { QMetaType::QString, 16 }, { QMetaType::QString, 17 }, { QMetaType::Int, 13 },
        }}),
        // Signal 'turnChanged'
        QtMocHelpers::SignalData<void(bool)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Signal 'moveHighlightRequested'
        QtMocHelpers::SignalData<void(const QPoint &, const QPoint &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 21, 22 }, { 0x80000000 | 21, 23 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &, bool)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 }, { QMetaType::Bool, 25 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &)>(24, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'csaCommLogAppended'
        QtMocHelpers::SignalData<void(const QString &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
        // Signal 'engineScoreUpdated'
        QtMocHelpers::SignalData<void(int, int)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 29 }, { QMetaType::Int, 30 },
        }}),
        // Slot 'onHumanMove'
        QtMocHelpers::SlotData<void(const QPoint &, const QPoint &, bool)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 21, 22 }, { 0x80000000 | 21, 23 }, { QMetaType::Bool, 32 },
        }}),
        // Slot 'onResign'
        QtMocHelpers::SlotData<void()>(33, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onConnectionStateChanged'
        QtMocHelpers::SlotData<void(CsaClient::ConnectionState)>(34, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 35, 4 },
        }}),
        // Slot 'onClientError'
        QtMocHelpers::SlotData<void(const QString &)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Slot 'onLoginSucceeded'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoginFailed'
        QtMocHelpers::SlotData<void(const QString &)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 39 },
        }}),
        // Slot 'onLogoutCompleted'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onGameSummaryReceived'
        QtMocHelpers::SlotData<void(const CsaClient::GameSummary &)>(41, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 42, 43 },
        }}),
        // Slot 'onGameStarted'
        QtMocHelpers::SlotData<void(const QString &)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 },
        }}),
        // Slot 'onGameRejected'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 45 }, { QMetaType::QString, 47 },
        }}),
        // Slot 'onMoveReceived'
        QtMocHelpers::SlotData<void(const QString &, int)>(48, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 49 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'onMoveConfirmed'
        QtMocHelpers::SlotData<void(const QString &, int)>(50, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 49 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'onClientGameEnded'
        QtMocHelpers::SlotData<void(CsaClient::GameResult, CsaClient::GameEndCause, int)>(51, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 52, 11 }, { 0x80000000 | 53, 12 }, { QMetaType::Int, 13 },
        }}),
        // Slot 'onGameInterrupted'
        QtMocHelpers::SlotData<void()>(54, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRawMessageReceived'
        QtMocHelpers::SlotData<void(const QString &)>(55, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Slot 'onRawMessageSent'
        QtMocHelpers::SlotData<void(const QString &)>(56, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Slot 'onEngineBestMoveReceived'
        QtMocHelpers::SlotData<void()>(57, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEngineResign'
        QtMocHelpers::SlotData<void()>(58, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'GameState'
        QtMocHelpers::EnumData<enum GameState>(59, 59, QMC::EnumIsScoped).add({
            {   60, GameState::Idle },
            {   61, GameState::Connecting },
            {   62, GameState::LoggingIn },
            {   63, GameState::WaitingForMatch },
            {   64, GameState::WaitingForAgree },
            {   65, GameState::InGame },
            {   66, GameState::GameOver },
            {   67, GameState::Error },
        }),
    };
    return QtMocHelpers::metaObjectData<CsaGameCoordinator, qt_meta_tag_ZN18CsaGameCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject CsaGameCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CsaGameCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CsaGameCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18CsaGameCoordinatorE_t>.metaTypes,
    nullptr
} };

void CsaGameCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CsaGameCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->gameStateChanged((*reinterpret_cast<std::add_pointer_t<CsaGameCoordinator::GameState>>(_a[1]))); break;
        case 1: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->gameStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->gameEnded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 4: _t->moveMade((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 5: _t->turnChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->moveHighlightRequested((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[2]))); break;
        case 7: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 8: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->csaCommLogAppended((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->engineScoreUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->onHumanMove((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 12: _t->onResign(); break;
        case 13: _t->onConnectionStateChanged((*reinterpret_cast<std::add_pointer_t<CsaClient::ConnectionState>>(_a[1]))); break;
        case 14: _t->onClientError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->onLoginSucceeded(); break;
        case 16: _t->onLoginFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->onLogoutCompleted(); break;
        case 18: _t->onGameSummaryReceived((*reinterpret_cast<std::add_pointer_t<CsaClient::GameSummary>>(_a[1]))); break;
        case 19: _t->onGameStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->onGameRejected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 21: _t->onMoveReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 22: _t->onMoveConfirmed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 23: _t->onClientGameEnded((*reinterpret_cast<std::add_pointer_t<CsaClient::GameResult>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<CsaClient::GameEndCause>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 24: _t->onGameInterrupted(); break;
        case 25: _t->onRawMessageReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->onRawMessageSent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->onEngineBestMoveReceived(); break;
        case 28: _t->onEngineResign(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(CsaGameCoordinator::GameState )>(_a, &CsaGameCoordinator::gameStateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & )>(_a, &CsaGameCoordinator::errorOccurred, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & , const QString & )>(_a, &CsaGameCoordinator::gameStarted, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & , const QString & , int )>(_a, &CsaGameCoordinator::gameEnded, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & , const QString & , const QString & , int )>(_a, &CsaGameCoordinator::moveMade, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(bool )>(_a, &CsaGameCoordinator::turnChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QPoint & , const QPoint & )>(_a, &CsaGameCoordinator::moveHighlightRequested, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & , bool )>(_a, &CsaGameCoordinator::logMessage, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(const QString & )>(_a, &CsaGameCoordinator::csaCommLogAppended, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (CsaGameCoordinator::*)(int , int )>(_a, &CsaGameCoordinator::engineScoreUpdated, 10))
            return;
    }
}

const QMetaObject *CsaGameCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CsaGameCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18CsaGameCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CsaGameCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 29)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 29;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 29)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 29;
    }
    return _id;
}

// SIGNAL 0
void CsaGameCoordinator::gameStateChanged(CsaGameCoordinator::GameState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void CsaGameCoordinator::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void CsaGameCoordinator::gameStarted(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void CsaGameCoordinator::gameEnded(const QString & _t1, const QString & _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void CsaGameCoordinator::moveMade(const QString & _t1, const QString & _t2, const QString & _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 5
void CsaGameCoordinator::turnChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void CsaGameCoordinator::moveHighlightRequested(const QPoint & _t1, const QPoint & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void CsaGameCoordinator::logMessage(const QString & _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 9
void CsaGameCoordinator::csaCommLogAppended(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void CsaGameCoordinator::engineScoreUpdated(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2);
}
QT_WARNING_POP
