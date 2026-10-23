#ifndef JOSEKIWINDOWCONTROLLER_H
#define JOSEKIWINDOWCONTROLLER_H

/// @file josekiwindowcontroller.h
/// @brief 定跡ウィンドウの表示・更新と、定跡手の着手


#include <QObject>
#include <functional>
#include <utility>
#include <QString>
#include <QStringList>
#include <QPoint>

#include "josekilines.h"

class BoardMoveController;
class BranchTreeEditController;
class JosekiBookSession;
class KifuNavigationController;
class JosekiWindow;
class KifuNavigationState;
class QAction;
class QDockWidget;

/**
 * @brief 定跡ウィンドウを作って表示中の手の局面に合わせ、選んだ定跡手を盤への着手として要求する
 *
 * 表示中の手（KifuNavigationState）が変わるたびに、定跡ウィンドウが前面にあれば局面と、表示中の手順が定跡を
 * 外れた手を合わせる。定跡ウィンドウの「棋譜に展開...」で、表示中の局面から定跡を棋譜の変化に展開する。
 *
 * 定跡の表示の切り替え（displayAction。保存する）と「盤の手を登録」（recordBoardMovesAction。保存しない）の
 * QAction を持ち、「盤に定跡手を表示」（メニューの QAction。保存する）と合わせて定跡ウィンドウに渡す。
 * 盤の矢印（CandidateArrowPresenter）と盤の手の登録（JosekiRegistrationController）もこの QAction を読む。
 */
class JosekiWindowController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* parentWidget = nullptr;
        JosekiBookSession* book = nullptr;               ///< 開いている定跡（必須・非所有）
        QAction* boardArrows = nullptr;                  ///< 「表示」メニューの「盤に定跡手を表示」（必須）
        /// 表示中の手。定跡ウィンドウの局面と、棋譜のマージに使う（非所有）
        const KifuNavigationState* navState = nullptr;
        /// 盤面クリックと同じ「人間が今指せるか」の判定
        std::function<bool()> isHumanTurnNow;
        /// 定跡手を盤に指す（盤の操作と同じ処理。必須）
        BoardMoveController* boardMoves = nullptr;
        /// 定跡を棋譜の変化に展開する（必須）
        BranchTreeEditController* branchEdit = nullptr;
        /// 定跡を外れた手へ移る（必須）
        KifuNavigationController* navController = nullptr;
    };

    explicit JosekiWindowController(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~JosekiWindowController() override = default;

    /**
     * @brief 定跡ウィンドウを更新する
     *
     * 現在の局面と手番情報を反映する。
     */
    void updateJosekiWindow();

    /**
     * @brief 定跡ウィンドウを取得
     * @return JosekiWindowへのポインタ（コンストラクタで作る。ドックに入れると所有はドックに移る）
     */
    JosekiWindow* josekiWindow() const { return m_josekiWindow; }

    /// 定跡を表に出し、盤に矢印を出すか（「■ 停止」で外す。設定に保存する）
    QAction* displayAction() const { return m_displayAction; }
    /// 盤で指した手・対局で指された手を定跡に登録するか（「盤の手を登録」。起動するとオフ）
    QAction* recordBoardMovesAction() const { return m_recordBoardMovesAction; }

    /**
     * @brief 定跡ウィンドウを入れたドックを登録する
     *
     * タブにまとめたドックは裏に回っても Qt では表示中のままなので、ドックの
     * visibilityChanged で前面にあるかを覚え、前面にある間だけ更新する。
     */
    void attachDock(QDockWidget* dock);

private slots:
    /**
     * @brief 定跡手選択時の処理
     * @param usiMove USI形式の指し手
     */
    void onJosekiMoveSelected(const QString& usiMove);

    /**
     * @brief 棋譜データマージ要求時の処理
     */
    void onRequestKifuDataForMerge();

    /// ドックが前面に出た・裏に回った・閉じたときの処理
    void onDockVisibilityChanged(bool visible);

    /// 表示中の局面から定跡を棋譜の変化に展開する
    void onExpandRequested(const JosekiLines::ExpandOptions& options);
    /// 定跡の表示を止めた・再開した（保存し、定跡を外れた手の表示を合わせる）
    void onDisplayToggled(bool displayed);
    /// 「盤に定跡手を表示」を変えた（保存する）
    void onBoardArrowsToggled(bool visible);

private:
    /// 人間が今指せるか（盤面クリックと同じ判定）
    bool determineHumanCanPlay() const;
    /// 表示中の手順が定跡を外れた手を定跡ウィンドウに出す
    void updateBookDeviation();

    // 依存オブジェクト
    QWidget* m_parentWidget = nullptr;
    const JosekiBookSession* m_book = nullptr;
    BranchTreeEditController* m_branchEdit = nullptr;
    const KifuNavigationState* m_navState = nullptr;
    std::function<bool()> m_isHumanTurnNow;  ///< 人間が今指せるか
    BoardMoveController* m_boardMoves = nullptr;

    // 内部オブジェクト
    QAction* m_displayAction = nullptr;
    QAction* m_recordBoardMovesAction = nullptr;
    JosekiWindow* m_josekiWindow = nullptr;
    /// 定跡ウィンドウを入れたドック（無ければ nullptr。単独のウィンドウとして使う場合）
    QDockWidget* m_dockWidget = nullptr;
    /// ドックが前面に出ているか（visibilityChanged で更新）
    bool m_dockShown = false;
};

#endif // JOSEKIWINDOWCONTROLLER_H
