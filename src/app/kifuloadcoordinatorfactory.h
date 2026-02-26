#ifndef KIFULOADCOORDINATORFACTORY_H
#define KIFULOADCOORDINATORFACTORY_H

/// @file kifuloadcoordinatorfactory.h
/// @brief KifuLoadCoordinator の生成・配線ロジックを担うファクトリ

class MainWindow;

/**
 * @brief KifuLoadCoordinator の生成・依存設定・シグナル接続を担うファクトリ
 *
 * createAndWireKifuLoadCoordinator の生成・配線ロジックを MainWindow から分離し、
 * MainWindow 側は1行の委譲呼び出しのみにする。
 *
 * MainWindow の private メンバへアクセスするため friend 宣言が必要。
 */
class KifuLoadCoordinatorFactory
{
public:
    /// KifuLoadCoordinator を生成し、依存設定・シグナル接続を行う
    static void createAndWire(MainWindow& mw);
};

#endif // KIFULOADCOORDINATORFACTORY_H
