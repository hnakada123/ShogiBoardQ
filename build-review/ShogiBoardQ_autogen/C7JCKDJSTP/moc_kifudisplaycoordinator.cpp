/****************************************************************************
** Meta object code from reading C++ file 'kifudisplaycoordinator.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/coordinators/kifudisplaycoordinator.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#include <QtCore/QSet>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kifudisplaycoordinator.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN22KifuDisplayCoordinatorE_t {};
} // unnamed namespace

template <> constexpr inline auto KifuDisplayCoordinator::qt_create_metaobjectdata<qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KifuDisplayCoordinator",
        "boardSfenChanged",
        "",
        "sfen",
        "boardWithHighlightsRequired",
        "currentSfen",
        "prevSfen",
        "commentUpdateRequired",
        "ply",
        "comment",
        "asHtml",
        "onNavigationCompleted",
        "KifuBranchNode*",
        "node",
        "onBoardUpdateRequired",
        "onRecordHighlightRequired",
        "onBranchTreeHighlightRequired",
        "lineIndex",
        "onBranchCandidatesUpdateRequired",
        "QList<KifuBranchNode*>",
        "candidates",
        "onTreeChanged",
        "resetTracking",
        "onBranchTreeNodeClicked",
        "onBranchCandidateActivated",
        "QModelIndex",
        "index",
        "onPositionChanged",
        "onLiveGameMoveAdded",
        "displayText",
        "onLiveGameSessionStarted",
        "branchPoint",
        "onLiveGameBranchMarksUpdated",
        "QSet<int>",
        "branchPlys",
        "onLiveGameCommitted",
        "newLineEnd",
        "onLiveGameDiscarded",
        "onLiveGameRecordModelUpdateRequired"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'boardSfenChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'boardWithHighlightsRequired'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'commentUpdateRequired'
        QtMocHelpers::SignalData<void(int, const QString &, bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::QString, 9 }, { QMetaType::Bool, 10 },
        }}),
        // Slot 'onNavigationCompleted'
        QtMocHelpers::SlotData<void(KifuBranchNode *)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onBoardUpdateRequired'
        QtMocHelpers::SlotData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Slot 'onRecordHighlightRequired'
        QtMocHelpers::SlotData<void(int)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Slot 'onBranchTreeHighlightRequired'
        QtMocHelpers::SlotData<void(int, int)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Int, 8 },
        }}),
        // Slot 'onBranchCandidatesUpdateRequired'
        QtMocHelpers::SlotData<void(const QList<KifuBranchNode*> &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 19, 20 },
        }}),
        // Slot 'onTreeChanged'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'resetTracking'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onBranchTreeNodeClicked'
        QtMocHelpers::SlotData<void(int, int)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Int, 8 },
        }}),
        // Slot 'onBranchCandidateActivated'
        QtMocHelpers::SlotData<void(const QModelIndex &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 25, 26 },
        }}),
        // Slot 'onPositionChanged'
        QtMocHelpers::SlotData<void(int, int, const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { QMetaType::Int, 8 }, { QMetaType::QString, 3 },
        }}),
        // Slot 'onLiveGameMoveAdded'
        QtMocHelpers::SlotData<void(int, const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::QString, 29 },
        }}),
        // Slot 'onLiveGameSessionStarted'
        QtMocHelpers::SlotData<void(KifuBranchNode *)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 31 },
        }}),
        // Slot 'onLiveGameBranchMarksUpdated'
        QtMocHelpers::SlotData<void(const QSet<int> &)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 33, 34 },
        }}),
        // Slot 'onLiveGameCommitted'
        QtMocHelpers::SlotData<void(KifuBranchNode *)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 36 },
        }}),
        // Slot 'onLiveGameDiscarded'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onLiveGameRecordModelUpdateRequired'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KifuDisplayCoordinator, qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KifuDisplayCoordinator::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>.metaTypes,
    nullptr
} };

void KifuDisplayCoordinator::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KifuDisplayCoordinator *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->boardSfenChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->boardWithHighlightsRequired((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->commentUpdateRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 3: _t->onNavigationCompleted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 4: _t->onBoardUpdateRequired((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->onRecordHighlightRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onBranchTreeHighlightRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->onBranchCandidatesUpdateRequired((*reinterpret_cast<std::add_pointer_t<QList<KifuBranchNode*>>>(_a[1]))); break;
        case 8: _t->onTreeChanged(); break;
        case 9: _t->resetTracking(); break;
        case 10: _t->onBranchTreeNodeClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->onBranchCandidateActivated((*reinterpret_cast<std::add_pointer_t<QModelIndex>>(_a[1]))); break;
        case 12: _t->onPositionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 13: _t->onLiveGameMoveAdded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->onLiveGameSessionStarted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 15: _t->onLiveGameBranchMarksUpdated((*reinterpret_cast<std::add_pointer_t<QSet<int>>>(_a[1]))); break;
        case 16: _t->onLiveGameCommitted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 17: _t->onLiveGameDiscarded(); break;
        case 18: _t->onLiveGameRecordModelUpdateRequired(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 15:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QSet<int> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KifuDisplayCoordinator::*)(const QString & )>(_a, &KifuDisplayCoordinator::boardSfenChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuDisplayCoordinator::*)(const QString & , const QString & )>(_a, &KifuDisplayCoordinator::boardWithHighlightsRequired, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuDisplayCoordinator::*)(int , const QString & , bool )>(_a, &KifuDisplayCoordinator::commentUpdateRequired, 2))
            return;
    }
}

const QMetaObject *KifuDisplayCoordinator::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KifuDisplayCoordinator::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22KifuDisplayCoordinatorE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int KifuDisplayCoordinator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void KifuDisplayCoordinator::boardSfenChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KifuDisplayCoordinator::boardWithHighlightsRequired(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void KifuDisplayCoordinator::commentUpdateRequired(int _t1, const QString & _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
