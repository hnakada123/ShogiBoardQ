#ifndef KIFUNAVIGATIONCONTROLLER_H
#define KIFUNAVIGATIONCONTROLLER_H

/// @file kifunavigationcontroller.h
/// @brief 棋譜を移る利用者の操作（ボタン・棋譜欄・分岐候補・分岐ツリー・評価値グラフ）

#include <QObject>
#include <functional>

class KifuBranchNode;
class KifuNavigationState;
class QPushButton;

/**
 * @brief 棋譜を移る操作を受けて、表示中の手（KifuNavigationState）を動かす
 *
 * 表示の更新はしない。表示する部品は KifuNavigationState::changed() を受けて自分で合わせる。
 * 利用者の操作は、移る前に確認（未更新のコメントの破棄など）を通す。確認でやめたときは
 * navigationRejected() を出す（クリックした行の強調を表示中の手に戻すため）。
 */
class KifuNavigationController : public QObject
{
    Q_OBJECT

public:
    /// ナビゲーションボタン群
    struct Buttons {
        QPushButton* first = nullptr;  ///< 開始局面ボタン（非所有）
        QPushButton* back10 = nullptr; ///< 10手戻るボタン（非所有）
        QPushButton* prev = nullptr;   ///< 1手戻るボタン（非所有）
        QPushButton* next = nullptr;   ///< 1手進むボタン（非所有）
        QPushButton* fwd10 = nullptr;  ///< 10手進むボタン（非所有）
        QPushButton* last = nullptr;   ///< 最終手ボタン（非所有）
    };

    explicit KifuNavigationController(KifuNavigationState* state, QObject* parent = nullptr);

    /// ナビゲーションボタンの clicked を接続する
    void connectButtons(const Buttons& buttons);

    /// 利用者の操作で別の手へ移る前の確認。false なら移らない
    void setLeaveGuard(std::function<bool()> guard) { m_leaveGuard = std::move(guard); }

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
    /// lineIndex 番の手順（KifuBranchTree::allLines()）の ply 手目へ移る
    void goToLine(int lineIndex, int ply);
    /// 本譜の同じ手数へ移る（本譜が短ければ本譜の最終手）
    void goToMainLineAtCurrentPly();

    // --- 利用者の操作（確認を通す） ---
    /// 棋譜欄の行（表示中の手順の手数）を選んだ
    void selectRecordRow(int row);
    /// 分岐候補欄の行を選んだ（候補の後ろの行は「本譜へ戻る」）
    void selectBranchCandidateRow(int row);
    /// 分岐ツリーのノードを選んだ（row は allLines() の添字）
    void selectBranchTreeNode(int row, int ply);
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
    std::function<bool()> m_leaveGuard;       ///< 移動前の確認（未設定なら常に移動する）
};

#endif // KIFUNAVIGATIONCONTROLLER_H
