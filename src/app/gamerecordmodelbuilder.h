#ifndef GAMERECORDMODELBUILDER_H
#define GAMERECORDMODELBUILDER_H

/// @file gamerecordmodelbuilder.h
/// @brief GameRecordModel の構築ロジックを集約するビルダーの定義

#include <QList>

class QObject;
class GameRecordModel;
class GameRecordPresenter;
class CommentCoordinator;
class KifuRecordListModel;
class KifuBranchTree;
class KifuNavigationState;
struct KifDisplayItem;

/**
 * @brief GameRecordModel の構築ロジックを集約するビルダー
 *
 * 責務:
 * - GameRecordModel のインスタンス生成
 * - 外部データストア（liveDisp, branchTree, navState）のバインド
 * - CommentCoordinator とのシグナル接続・コールバック設定
 *
 * ensure* パターンの null チェックは MainWindow 側に残す。
 * このビルダーは「構築のみ」を担当する。
 */
class GameRecordModelBuilder
{
public:
    /// ビルダーに渡す依存情報
    struct Deps {
        QObject* parent = nullptr;                          ///< GameRecordModel の親オブジェクト
        GameRecordPresenter* recordPresenter = nullptr;     ///< 棋譜表示プレゼンタ（非所有）
        KifuBranchTree* branchTree = nullptr;               ///< 分岐ツリー（非所有）
        KifuNavigationState* navState = nullptr;            ///< ナビゲーション状態（非所有）
        CommentCoordinator* commentCoordinator = nullptr;   ///< コメントコーディネータ（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;     ///< 棋譜リストモデル（非所有）
    };

    /**
     * @brief GameRecordModel を構築し、依存をバインドして返す
     * @param deps ビルダー依存情報
     * @return 構築済みの GameRecordModel（親が所有）
     */
    static GameRecordModel* build(const Deps& deps);
};

#endif // GAMERECORDMODELBUILDER_H
