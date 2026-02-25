#ifndef MAINWINDOWCOMPOSITIONROOT_H
#define MAINWINDOWCOMPOSITIONROOT_H

/// @file mainwindowcompositionroot.h
/// @brief ensure* の生成ロジックを集約する CompositionRoot の定義

#include <QObject>
#include "mainwindowdepsfactory.h"

class DialogCoordinatorWiring;
class DialogCoordinator;
class KifuFileController;
class RecordNavigationWiring;

/**
 * @brief ensure* の生成ロジックを集約する CompositionRoot
 *
 * 責務:
 * - 「いつ factory を呼ぶか」と「オブジェクト生成順序の制御」のみ
 * - 所有権は MainWindow 側に残す（CompositionRoot は生成結果を out パラメータで返す）
 * - MainWindowDepsFactory は CompositionRoot 内部でのみ使用する
 */
class MainWindowCompositionRoot : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowCompositionRoot(QObject* parent = nullptr);

    /**
     * @brief DialogCoordinatorWiring + DialogCoordinator を確保する
     * @param refs ランタイム参照スナップショット
     * @param callbacks DialogCoordinator 用コールバック群
     * @param parent Wiring の親オブジェクト
     * @param[in,out] wiring 生成された Wiring（未生成なら生成）
     * @param[in,out] coordinator 生成された Coordinator
     */
    void ensureDialogCoordinator(const MainWindowRuntimeRefs& refs,
                                 const MainWindowDepsFactory::DialogCoordinatorCallbacks& callbacks,
                                 QObject* parent,
                                 DialogCoordinatorWiring*& wiring,
                                 DialogCoordinator*& coordinator);

    /**
     * @brief KifuFileController を確保し依存を更新する
     * @param refs ランタイム参照スナップショット
     * @param callbacks KifuFileController 用コールバック群
     * @param parent Controller の親オブジェクト
     * @param[in,out] controller 生成された Controller（未生成なら生成）
     */
    void ensureKifuFileController(const MainWindowRuntimeRefs& refs,
                                  const MainWindowDepsFactory::KifuFileCallbacks& callbacks,
                                  QObject* parent,
                                  KifuFileController*& controller);

    /**
     * @brief RecordNavigationWiring を確保し依存を更新する
     * @param refs ランタイム参照スナップショット
     * @param parent Wiring の親オブジェクト
     * @param[in,out] wiring 生成された Wiring（未生成なら生成）
     */
    void ensureRecordNavigationWiring(const MainWindowRuntimeRefs& refs,
                                      QObject* parent,
                                      RecordNavigationWiring*& wiring);
};

#endif // MAINWINDOWCOMPOSITIONROOT_H
