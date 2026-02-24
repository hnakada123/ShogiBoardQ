#ifndef VIEWACTIONSWIRING_H
#define VIEWACTIONSWIRING_H

/// @file viewactionswiring.h
/// @brief 盤操作・表示関連アクションの配線クラスの定義

#include <QObject>

namespace Ui { class MainWindow; }
class MainWindow;
class DialogLaunchWiring;

/**
 * @brief 盤操作・表示関連のQActionをMainWindowスロットに接続するクラス
 *
 * Flip, Enlarge, Shrink, CopyBoard, CopyEvalGraph,
 * ImmediateMove, MenuWindow, UndoMove, SaveBoardImage,
 * SaveEvalGraph, ToolBar の接続を担当する。
 */
class ViewActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        MainWindow* mw = nullptr;
        DialogLaunchWiring* dlw = nullptr;
    };

    explicit ViewActionsWiring(const Deps& d, QObject* parent = nullptr)
        : QObject(parent), m_d(d) {}

    void wire();

private:
    Deps m_d;
};

#endif // VIEWACTIONSWIRING_H
