#ifndef KIFUNAVIGATIONCONTROLLER_H
#define KIFUNAVIGATIONCONTROLLER_H

/// @file kifunavigationcontroller.h
/// @brief 棋譜を移る利用者の操作（ボタン・棋譜欄・分岐候補・分岐ツリー・評価値グラフ）

#include <QObject>
#include <array>
#include <functional>

class KifuBranchNode;
class KifuNavigationState;
class StepButtons;
class QAction;

/**
 * @brief 棋譜を移る操作を受けて、表示中の手（KifuNavigationState）を動かす
 *
 * 表示の更新はしない。表示する部品は KifuNavigationState::changed() を受けて自分で合わせる。
 * 利用者の操作は、移る前に確認（未更新のコメントの破棄など）を通す。確認でやめたときは
 * navigationRejected() を出す（クリックした行の強調を表示中の手に戻すため）。
 *
 * 局面送りの6つの操作は QAction として1組だけ持つ。棋譜欄と評価値グラフのボタンは
 * bindButtons() でこの QAction に結び付け、有効・無効も QAction に合わせる。
 */
class KifuNavigationController : public QObject
{
    Q_OBJECT

public:
    /// 局面送りの操作
    enum Step { First, Back10, Prev, Next, Fwd10, Last, StepCount };

    explicit KifuNavigationController(KifuNavigationState* state, QObject* parent = nullptr);

    /// 局面送りの操作の QAction（所有はこのクラス）
    QAction* action(Step step) const { return m_actions.at(static_cast<std::size_t>(step)); }

    /// 局面送りのボタンを押すと QAction を実行し、ボタンの説明（ツールチップ）と有効・無効を QAction に合わせる
    void bindButtons(const StepButtons* buttons);

    /// 局面送りの6つの操作をまとめて有効・無効にする（対局中などは無効）
    void setStepActionsEnabled(bool on);

    /// 利用者の操作で別の手へ移る前の確認。false なら移らない
    /// 移動前の確認を足す（足した順に聞き、1つでも false なら移らない）
    void addLeaveGuard(std::function<bool()> guard) { m_leaveGuards.append(std::move(guard)); }

    KifuNavigationState* state() const { return m_state; }

    /// 利用者の操作で移る前の確認を通す。やめたら navigationRejected() を出して false
    bool confirmLeave();

public slots:
    // --- 確認を通さない移動（プログラムからの移動） ---
    void goToFirst();
    void goToLast();
    void goBack(int count = 1);
    void goForward(int count = 1);
    /// 表示中の手順の ply 手目へ移る
    void goToPly(int ply);
    /// 指定ノードへ移る（開始局面からの経路を選んだ手順として記憶する）
    void goToNode(KifuBranchNode* node);
    /// lineEndNodeId の手で終わる手順の ply 手目へ移る（KifuBranchTree::nodeOnLine。手が無ければ移らない）
    void goToPlyOnLine(int lineEndNodeId, int ply);
    /// 本譜の同じ手数へ移る（本譜が短ければ本譜の最終手）
    void goToMainLineAtCurrentPly();

    // --- 利用者の操作（確認を通す） ---
    /// 棋譜欄の行（表示中の手順の手数）を選んだ
    void selectRecordRow(int row);
    /// 分岐候補欄の行を選んだ（候補の後ろの行は「本譜へ戻る」）
    void selectBranchCandidateRow(int row);
    /**
     * @brief 分岐ツリーの手を選んだ
     *
     * 表示中の手順にある手なら、その手順のまま移る。ほかの手順の手なら、その手から先頭の子を
     * たどった手順（図でその手を描いた行）に移る。開始局面なら分岐の選択を忘れて開始局面へ。
     */
    void selectBranchTreeNode(int nodeId);
    /// 評価値グラフの手数を選んだ
    void selectPly(int ply);
    void onFirstClicked();
    void onBack10Clicked();
    void onPrevClicked();
    void onNextClicked();
    void onFwd10Clicked();
    void onLastClicked();

signals:
    /// 利用者の操作を確認でやめた（クリックした行の強調を、表示中の手に戻す）
    void navigationRejected();

private:
    KifuNavigationState* m_state = nullptr;   ///< 表示中の手（非所有）
    QList<std::function<bool()>> m_leaveGuards;   ///< 移動前の確認（無ければ常に移動する）
    std::array<QAction*, StepCount> m_actions{}; ///< 局面送りの操作（Step の順）
};

#endif // KIFUNAVIGATIONCONTROLLER_H
