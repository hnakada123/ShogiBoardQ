/****************************************************************************
** Meta object code from reading C++ file 'kifunavigationcontroller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/navigation/kifunavigationcontroller.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kifunavigationcontroller.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN24KifuNavigationControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto KifuNavigationController::qt_create_metaobjectdata<qt_meta_tag_ZN24KifuNavigationControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KifuNavigationController",
        "navigationCompleted",
        "",
        "KifuBranchNode*",
        "newNode",
        "boardUpdateRequired",
        "sfen",
        "recordHighlightRequired",
        "ply",
        "branchTreeHighlightRequired",
        "lineIndex",
        "branchCandidatesUpdateRequired",
        "QList<KifuBranchNode*>",
        "candidates",
        "lineSelectionChanged",
        "newLineIndex",
        "branchNodeHandled",
        "previousFileTo",
        "previousRankTo",
        "lastUsiMove",
        "goToFirst",
        "goToLast",
        "goBack",
        "count",
        "goForward",
        "goToNode",
        "node",
        "goToPly",
        "switchToLine",
        "selectBranchCandidate",
        "candidateIndex",
        "goToMainLineAtCurrentPly",
        "handleBranchNodeActivated",
        "row",
        "onFirstClicked",
        "checked",
        "onBack10Clicked",
        "onPrevClicked",
        "onNextClicked",
        "onFwd10Clicked",
        "onLastClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'navigationCompleted'
        QtMocHelpers::SignalData<void(KifuBranchNode *)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'boardUpdateRequired'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'recordHighlightRequired'
        QtMocHelpers::SignalData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Signal 'branchTreeHighlightRequired'
        QtMocHelpers::SignalData<void(int, int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { QMetaType::Int, 8 },
        }}),
        // Signal 'branchCandidatesUpdateRequired'
        QtMocHelpers::SignalData<void(const QList<KifuBranchNode*> &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'lineSelectionChanged'
        QtMocHelpers::SignalData<void(int)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Signal 'branchNodeHandled'
        QtMocHelpers::SignalData<void(int, const QString &, int, int, const QString &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::QString, 6 }, { QMetaType::Int, 17 }, { QMetaType::Int, 18 },
            { QMetaType::QString, 19 },
        }}),
        // Slot 'goToFirst'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'goToLast'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'goBack'
        QtMocHelpers::SlotData<void(int)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 23 },
        }}),
        // Slot 'goBack'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'goForward'
        QtMocHelpers::SlotData<void(int)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 23 },
        }}),
        // Slot 'goForward'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'goToNode'
        QtMocHelpers::SlotData<void(KifuBranchNode *)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 26 },
        }}),
        // Slot 'goToPly'
        QtMocHelpers::SlotData<void(int)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Slot 'switchToLine'
        QtMocHelpers::SlotData<void(int)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'selectBranchCandidate'
        QtMocHelpers::SlotData<void(int)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 30 },
        }}),
        // Slot 'goToMainLineAtCurrentPly'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleBranchNodeActivated'
        QtMocHelpers::SlotData<void(int, int)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { QMetaType::Int, 8 },
        }}),
        // Slot 'onFirstClicked'
        QtMocHelpers::SlotData<void(bool)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onFirstClicked'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'onBack10Clicked'
        QtMocHelpers::SlotData<void(bool)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onBack10Clicked'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'onPrevClicked'
        QtMocHelpers::SlotData<void(bool)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onPrevClicked'
        QtMocHelpers::SlotData<void()>(37, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'onNextClicked'
        QtMocHelpers::SlotData<void(bool)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onNextClicked'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'onFwd10Clicked'
        QtMocHelpers::SlotData<void(bool)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onFwd10Clicked'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Slot 'onLastClicked'
        QtMocHelpers::SlotData<void(bool)>(40, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 35 },
        }}),
        // Slot 'onLastClicked'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KifuNavigationController, qt_meta_tag_ZN24KifuNavigationControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KifuNavigationController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24KifuNavigationControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24KifuNavigationControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN24KifuNavigationControllerE_t>.metaTypes,
    nullptr
} };

void KifuNavigationController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KifuNavigationController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->navigationCompleted((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 1: _t->boardUpdateRequired((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->recordHighlightRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->branchTreeHighlightRequired((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->branchCandidatesUpdateRequired((*reinterpret_cast<std::add_pointer_t<QList<KifuBranchNode*>>>(_a[1]))); break;
        case 5: _t->lineSelectionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->branchNodeHandled((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[5]))); break;
        case 7: _t->goToFirst(); break;
        case 8: _t->goToLast(); break;
        case 9: _t->goBack((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->goBack(); break;
        case 11: _t->goForward((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->goForward(); break;
        case 13: _t->goToNode((*reinterpret_cast<std::add_pointer_t<KifuBranchNode*>>(_a[1]))); break;
        case 14: _t->goToPly((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->switchToLine((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 16: _t->selectBranchCandidate((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->goToMainLineAtCurrentPly(); break;
        case 18: _t->handleBranchNodeActivated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 19: _t->onFirstClicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: _t->onFirstClicked(); break;
        case 21: _t->onBack10Clicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 22: _t->onBack10Clicked(); break;
        case 23: _t->onPrevClicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 24: _t->onPrevClicked(); break;
        case 25: _t->onNextClicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 26: _t->onNextClicked(); break;
        case 27: _t->onFwd10Clicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 28: _t->onFwd10Clicked(); break;
        case 29: _t->onLastClicked((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 30: _t->onLastClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(KifuBranchNode * )>(_a, &KifuNavigationController::navigationCompleted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(const QString & )>(_a, &KifuNavigationController::boardUpdateRequired, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(int )>(_a, &KifuNavigationController::recordHighlightRequired, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(int , int )>(_a, &KifuNavigationController::branchTreeHighlightRequired, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(const QList<KifuBranchNode*> & )>(_a, &KifuNavigationController::branchCandidatesUpdateRequired, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(int )>(_a, &KifuNavigationController::lineSelectionChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (KifuNavigationController::*)(int , const QString & , int , int , const QString & )>(_a, &KifuNavigationController::branchNodeHandled, 6))
            return;
    }
}

const QMetaObject *KifuNavigationController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KifuNavigationController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN24KifuNavigationControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int KifuNavigationController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 31)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 31;
    }
    return _id;
}

// SIGNAL 0
void KifuNavigationController::navigationCompleted(KifuBranchNode * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KifuNavigationController::boardUpdateRequired(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void KifuNavigationController::recordHighlightRequired(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void KifuNavigationController::branchTreeHighlightRequired(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void KifuNavigationController::branchCandidatesUpdateRequired(const QList<KifuBranchNode*> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void KifuNavigationController::lineSelectionChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void KifuNavigationController::branchNodeHandled(int _t1, const QString & _t2, int _t3, int _t4, const QString & _t5)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2, _t3, _t4, _t5);
}
QT_WARNING_POP
