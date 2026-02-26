#ifndef MAINWINDOWRUNTIMEREFSFACTORY_H
#define MAINWINDOWRUNTIMEREFSFACTORY_H

/// @file mainwindowruntimerefsfactory.h
/// @brief MainWindowRuntimeRefs スナップショットの構築ロジックを担うファクトリ

#include "mainwindowruntimerefs.h"

class MainWindow;

/**
 * @brief MainWindow のメンバ変数から MainWindowRuntimeRefs を構築するファクトリ
 *
 * buildRuntimeRefs の構築ロジックを MainWindow から分離し、
 * MainWindow 側は1行の委譲呼び出しのみにする。
 *
 * MainWindow の private メンバへアクセスするため friend 宣言が必要。
 */
class MainWindowRuntimeRefsFactory
{
public:
    /// MainWindow の現在のメンバ変数から RuntimeRefs スナップショットを構築する
    static MainWindowRuntimeRefs build(MainWindow& mw);
};

#endif // MAINWINDOWRUNTIMEREFSFACTORY_H
