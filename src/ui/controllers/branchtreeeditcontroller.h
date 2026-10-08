#ifndef BRANCHTREEEDITCONTROLLER_H
#define BRANCHTREEEDITCONTROLLER_H

/// @file branchtreeeditcontroller.h
/// @brief 分岐ツリーの右クリックメニューから分岐を編集するコントローラの定義

#include <QObject>
#include <functional>

class BranchTreeManager;
class KifuBranchNode;
class KifuBranchTree;
class KifuNavigationController;
class KifuNavigationState;
class LiveGameSession;
class QAction;
class QMenu;
class QPoint;
class QWidget;
class ShogiGameController;

/**
 * @brief 分岐の編集（本譜にする・並べ替え・削除）を担当するコントローラ
 *
 * BranchTreeManager の右クリックメニューに操作を追加し、選ばれた操作を
 * KifuBranchTree に適用する。編集後は現在の手を表示し直し、棋譜を未保存にする。
 * 対局中（ライブセッション中）は編集しない。
 */
class BranchTreeEditController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuBranchTree* tree = nullptr;
        KifuNavigationState* navState = nullptr;
        KifuNavigationController* navController = nullptr;
        LiveGameSession* liveSession = nullptr;
        QWidget* parentWidget = nullptr;                 ///< 確認ダイアログの親
        std::function<void()> markGameRecordDirty;       ///< 棋譜を未保存にする
    };

    /// 操作の種類（メニュー項目の data）
    enum class Operation {
        PromoteToMainLine = 1,
        MoveUp,
        MoveDown,
        DeleteFromHere,
    };

    explicit BranchTreeEditController(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// 分岐ツリーの右クリックメニューに接続する
    void attach(BranchTreeManager* manager);

    /// 編集できる状態か（棋譜があり、対局中でない）
    bool canEdit() const;

    /// 指定ノードに操作を適用する（右クリックメニュー・テストから使う）。
    /// 削除は confirmDelete が true のとき確認する。適用したら true
    bool apply(Operation operation, KifuBranchNode* node, bool confirmDelete = true);

    /// 操作を適用できるか
    bool isApplicable(Operation operation, KifuBranchNode* node) const;

    /**
     * @brief 対局していないときに盤上で指した手を、現在の手の次の手（変化）として記録する
     * @param from 移動元（盤上 1-9、駒台 10/11）。成り判定などで書き換わる
     * @param to 移動先
     * @param gc 合法判定と指し手表記の生成に使う（盤面は指した後の局面になる）
     * @return 合法手で、記録（または同じ手の既存ノードへ移動）できたら true
     *
     * 同じ手が既にあればそのノードへ移るだけで、新しい変化は作らない。
     */
    bool recordBoardMove(QPoint& from, QPoint& to, ShogiGameController* gc);

public slots:
    /// BranchTreeManager::nodeContextMenuAboutToShow に接続する
    void onNodeContextMenuAboutToShow(int row, int ply, QMenu* menu);
    /// RecordPane::branchContextMenuRequested に接続する（分岐候補欄の右クリック）
    void onCandidateContextMenuRequested(int row, const QPoint& globalPos);

private slots:
    void onMenuActionTriggered(QAction* action);

private:
    KifuBranchNode* nodeAt(int row, int ply) const;
    /// menu に node を対象とする編集操作を追加する
    void addEditActions(QMenu* menu, KifuBranchNode* node);
    bool confirmDeletion(KifuBranchNode* node) const;
    void refreshAfterEdit(KifuBranchNode* current);

    Deps m_deps;
    int m_menuTargetNodeId = -1;   ///< 表示中のメニューの対象ノード
};

#endif // BRANCHTREEEDITCONTROLLER_H
