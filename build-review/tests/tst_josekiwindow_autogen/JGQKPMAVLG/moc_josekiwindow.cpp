/****************************************************************************
** Meta object code from reading C++ file 'josekiwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/dialogs/josekiwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'josekiwindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN12JosekiWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto JosekiWindow::qt_create_metaobjectdata<qt_meta_tag_ZN12JosekiWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "JosekiWindow",
        "josekiMoveSelected",
        "",
        "usiMove",
        "requestKifuDataForMerge",
        "onOpenButtonClicked",
        "onNewButtonClicked",
        "onSaveButtonClicked",
        "onSaveAsButtonClicked",
        "onAddMoveButtonClicked",
        "setKifuDataForMerge",
        "sfenList",
        "moveList",
        "japaneseMoveList",
        "currentPly",
        "onFontSizeIncrease",
        "onFontSizeDecrease",
        "onMoveResult",
        "success",
        "onClearRecentFilesClicked",
        "onSfenDetailToggled",
        "checked",
        "onPlayButtonClicked",
        "onEditButtonClicked",
        "onDeleteButtonClicked",
        "onAutoLoadCheckBoxChanged",
        "Qt::CheckState",
        "state",
        "onStopButtonClicked",
        "onRecentFileClicked",
        "onMergeFromCurrentKifu",
        "onMergeFromKifuFile",
        "onTableDoubleClicked",
        "row",
        "column",
        "onTableContextMenu",
        "QPoint",
        "pos",
        "onContextMenuPlay",
        "onContextMenuEdit",
        "onContextMenuDelete",
        "onContextMenuCopyMove"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'josekiMoveSelected'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'requestKifuDataForMerge'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onOpenButtonClicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onNewButtonClicked'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onSaveButtonClicked'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onSaveAsButtonClicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onAddMoveButtonClicked'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setKifuDataForMerge'
        QtMocHelpers::SlotData<void(const QStringList &, const QStringList &, const QStringList &, int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 11 }, { QMetaType::QStringList, 12 }, { QMetaType::QStringList, 13 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'onFontSizeIncrease'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onFontSizeDecrease'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onMoveResult'
        QtMocHelpers::SlotData<void(bool, const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 }, { QMetaType::QString, 3 },
        }}),
        // Slot 'onClearRecentFilesClicked'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSfenDetailToggled'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onPlayButtonClicked'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onEditButtonClicked'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDeleteButtonClicked'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAutoLoadCheckBoxChanged'
        QtMocHelpers::SlotData<void(Qt::CheckState)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 26, 27 },
        }}),
        // Slot 'onStopButtonClicked'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRecentFileClicked'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMergeFromCurrentKifu'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onMergeFromKifuFile'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onTableDoubleClicked'
        QtMocHelpers::SlotData<void(int, int)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { QMetaType::Int, 34 },
        }}),
        // Slot 'onTableContextMenu'
        QtMocHelpers::SlotData<void(const QPoint &)>(35, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 36, 37 },
        }}),
        // Slot 'onContextMenuPlay'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onContextMenuEdit'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onContextMenuDelete'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onContextMenuCopyMove'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<JosekiWindow, qt_meta_tag_ZN12JosekiWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject JosekiWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12JosekiWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12JosekiWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12JosekiWindowE_t>.metaTypes,
    nullptr
} };

void JosekiWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<JosekiWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->josekiMoveSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->requestKifuDataForMerge(); break;
        case 2: _t->onOpenButtonClicked(); break;
        case 3: _t->onNewButtonClicked(); break;
        case 4: _t->onSaveButtonClicked(); break;
        case 5: _t->onSaveAsButtonClicked(); break;
        case 6: _t->onAddMoveButtonClicked(); break;
        case 7: _t->setKifuDataForMerge((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 8: _t->onFontSizeIncrease(); break;
        case 9: _t->onFontSizeDecrease(); break;
        case 10: _t->onMoveResult((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->onClearRecentFilesClicked(); break;
        case 12: _t->onSfenDetailToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->onPlayButtonClicked(); break;
        case 14: _t->onEditButtonClicked(); break;
        case 15: _t->onDeleteButtonClicked(); break;
        case 16: _t->onAutoLoadCheckBoxChanged((*reinterpret_cast<std::add_pointer_t<Qt::CheckState>>(_a[1]))); break;
        case 17: _t->onStopButtonClicked(); break;
        case 18: _t->onRecentFileClicked(); break;
        case 19: _t->onMergeFromCurrentKifu(); break;
        case 20: _t->onMergeFromKifuFile(); break;
        case 21: _t->onTableDoubleClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 22: _t->onTableContextMenu((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 23: _t->onContextMenuPlay(); break;
        case 24: _t->onContextMenuEdit(); break;
        case 25: _t->onContextMenuDelete(); break;
        case 26: _t->onContextMenuCopyMove(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (JosekiWindow::*)(const QString & )>(_a, &JosekiWindow::josekiMoveSelected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (JosekiWindow::*)()>(_a, &JosekiWindow::requestKifuDataForMerge, 1))
            return;
    }
}

const QMetaObject *JosekiWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *JosekiWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12JosekiWindowE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int JosekiWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 27)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 27;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 27)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 27;
    }
    return _id;
}

// SIGNAL 0
void JosekiWindow::josekiMoveSelected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void JosekiWindow::requestKifuDataForMerge()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
