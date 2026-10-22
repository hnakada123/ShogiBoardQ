#ifndef BRANCHTREEEDITCONTROLLER_H
#define BRANCHTREEEDITCONTROLLER_H

/// @file branchtreeeditcontroller.h
/// @brief 分岐ツリーの右クリックメニューから分岐を編集するコントローラの定義

#include <QObject>
#include <QString>

#include "recordline.h"
#include "usimove.h"

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
 * BranchTreeView の右クリックメニューに操作を追加し、選ばれた操作を
 * KifuBranchTree に適用する。編集後は現在の手を表示し直し、棋譜を未保存にする。
 * 対局中（ライブセッション中）は編集しない。
 */
class BranchTreeEditController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        KifuBranchTree* tree = nullptr;                  ///< 棋譜の正本（必須）
        KifuNavigationState* navState = nullptr;         ///< 表示中の手（必須）
        KifuNavigationController* navController = nullptr; ///< 移る前の確認（必須）
        LiveGameSession* liveSession = nullptr;          ///< 対局中は編集しない（必須）
        QWidget* parentWidget = nullptr;                 ///< 確認ダイアログの親
    };

    /// 操作の種類（メニュー項目の data）
    enum class Operation {
        PromoteToMainLine = 1,
        MoveUp,
        MoveDown,
        DeleteFromHere,
    };

    explicit BranchTreeEditController(const Deps& deps, QObject* parent = nullptr);

    /// 編集できる状態か（棋譜があり、対局中でない）
    bool canEdit() const;

    /// 盤上で指した手を記録できる状態か（対局中でない。棋譜が空でも開始局面から記録する）
    bool canRecordBoardMove() const;

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

    /// 成り・不成まで決まった手（定跡の手など）を、recordBoardMove と同じく変化として記録する
    bool recordMove(const UsiMove& move, ShogiGameController* gc);

    /**
     * @brief 手順をまとめて parent の後の変化として足す（定跡の展開）
     * @param lines parent の局面から始まる手順（先の手順ほど前の変化になる）
     * @return 新しく足した手の数（同じ手が既にあればその手をたどり、足さない）
     *
     * 表示中の手は変えない。対局中は足さない。
     */
    int addVariationLines(KifuBranchNode* parent, const QList<RecordLine>& lines);

signals:
    /// 対局していないときに盤上で指した手を記録した（同じ手の既存ノードへ移ったときも出す。sfen は指す前の局面）
    void boardMoveRecorded(const QString& sfen, const QString& usiMove);

public slots:
    /// BranchTreeView::nodeContextMenuAboutToShow に接続する
    void onNodeContextMenuAboutToShow(int nodeId, QMenu* menu);
    /// RecordPane::branchContextMenuRequested に接続する（分岐候補欄の右クリック）
    void onCandidateContextMenuRequested(int row, const QPoint& globalPos);

private slots:
    void onMenuActionTriggered(QAction* action);

private:
    /// 盤上の手を指す局面（表示中の手。終局の行なら直前の手）。盤をその局面にする。無ければ nullptr
    KifuBranchNode* prepareRecordParent(ShogiGameController* gc) const;
    /// parent の局面で move を指して記録する
    bool addMove(KifuBranchNode* parent, const UsiMove& move, ShogiGameController* gc);
    /// menu に node を対象とする編集操作を追加する
    void addEditActions(QMenu* menu, KifuBranchNode* node);
    bool confirmDeletion(KifuBranchNode* node) const;

    Deps m_deps;
    int m_menuTargetNodeId = -1;   ///< 表示中のメニューの対象ノード
};

#endif // BRANCHTREEEDITCONTROLLER_H
