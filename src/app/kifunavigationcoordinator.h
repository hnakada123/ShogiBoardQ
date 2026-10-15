#ifndef KIFUNAVIGATIONCOORDINATOR_H
#define KIFUNAVIGATIONCOORDINATOR_H

/// @file kifunavigationcoordinator.h
/// @brief 棋譜ナビゲーション同期コーディネータの定義

#include <QObject>
#include <QString>

#include "usimove.h"

struct AppState;
class ShogiView;
class KifuNavigationState;
class KifuNavigationController;
class AnalysisSession;
class EngineAnalysisTab;
class UiStatePolicyManager;
class JosekiWindowController;

/**
 * @brief 棋譜ナビゲーション同期コーディネータ
 *
 * 移動要求を KifuNavigationController に委譲し、完了後の定跡・検討表示を更新する。
 */
class KifuNavigationCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView* shogiView = nullptr;                        ///< 将棋盤ビュー（非所有）

        // --- Navigation state ---
        KifuNavigationController* navController = nullptr;
        KifuNavigationState* navState = nullptr;               ///< ナビゲーション状態（非所有）

        // --- 共有状態 ---
        AppState* state = nullptr;                             ///< 表示中の局面を書く（非所有）

        // --- Coordinators ---
        AnalysisSession* analysis = nullptr;                   ///< 検討のセッション（非所有）
        EngineAnalysisTab* analysisTab = nullptr;              ///< エンジン解析タブ（非所有）
        UiStatePolicyManager* uiStatePolicy = nullptr;         ///< UI状態ポリシーマネージャ（非所有）

        // --- 連動して更新する部品 ---
        JosekiWindowController* josekiController = nullptr;            ///< 定跡ウィンドウの更新（非所有）
    };

    explicit KifuNavigationCoordinator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// 棋譜読込後の指定局面を表示する。
    void syncLoadedPositionAtRow(int ply);

    /// 棋譜ビューを指定手数の行にスクロールし盤面を同期する
    void navigateToRow(int ply);

    /// 分岐ノード処理完了後の盤面・ハイライト更新
    void handleBranchNodeHandled(int ply, const QString& sfen,
                                 BoardSquare previousTo,
                                 const QString& lastUsiMove);

    /// 棋譜欄の指定行を選択し盤面を同期する
    void selectKifuRow(int row);

    /// ナビゲーション状態を手数に同期する
    void syncNavStateToPly(int selectedPly);

private:
    Deps m_deps;
};

#endif // KIFUNAVIGATIONCOORDINATOR_H
