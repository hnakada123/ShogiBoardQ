#ifndef CHECKABLE_ACTION_H
#define CHECKABLE_ACTION_H

/// @file checkable_action.h
/// @brief テストでパネルに渡す、チェックできる QAction

#include <QAction>

/// パネルのチェックが結ぶ切り替え（アプリでは ui の BoardArrowActions・JosekiWindowController が作る）。
/// パネルより先に作り、パネルより長く生かす
class CheckableAction : public QAction
{
public:
    explicit CheckableAction(bool checked = false, QObject* parent = nullptr)
        : QAction(parent)
    {
        setCheckable(true);
        setChecked(checked);
    }
};

#endif // CHECKABLE_ACTION_H
