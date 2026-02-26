#ifndef MAINWINDOWWIRINGASSEMBLER_H
#define MAINWINDOWWIRINGASSEMBLER_H

/// @file mainwindowwiringassembler.h
/// @brief MainWindow の配線組み立てロジックを分離するアセンブラ

#include "matchcoordinatorwiring.h"

class MainWindow;

/**
 * @brief MainWindow の配線組み立て（Deps 構築 + signal 接続）を担うアセンブラ
 *
 * 責務:
 * - MatchCoordinatorWiring::Deps の構築（std::bind/lambda の塊）
 * - MatchCoordinatorWiring 転送シグナルの接続
 * - DialogLaunchWiring の初期化（Deps 構築 + 生成 + signal 接続）
 *
 * MainWindow の private メンバへアクセスするため friend 宣言が必要。
 */
class MainWindowWiringAssembler
{
public:
    /// MatchCoordinatorWiring::Deps を構築する
    static MatchCoordinatorWiring::Deps buildMatchWiringDeps(MainWindow& mw);

    /// DialogLaunchWiring を初期化する（Deps 構築 + 生成 + signal 接続）
    static void initializeDialogLaunchWiring(MainWindow& mw);
};

#endif // MAINWINDOWWIRINGASSEMBLER_H
