#ifndef KIFUNAVIGATIONCONTROLLER_H
#define KIFUNAVIGATIONCONTROLLER_H

/// @file kifunavigationcontroller.h
/// @brief 棋譜ナビゲーションコントローラクラスの定義


#include <QObject>
#include <QVector>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class QPushButton;

/**
 * @brief 棋譜ナビゲーション操作を統一管理するコントローラ
 *
 * 6つのナビゲーションボタン、棋譜欄クリック、分岐ツリークリック、
 * 分岐候補クリックの全操作を統一的に処理し、
 * 盤面/棋譜欄/分岐ツリーへの更新シグナルを発行する。
 *
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

    explicit KifuNavigationController(QObject* parent = nullptr);

    /// 分岐ツリーとナビゲーション状態を設定する
    void setTreeAndState(KifuBranchTree* tree, KifuNavigationState* state);

    /// ナビゲーションボタンのclickedシグナルを接続する
    void connectButtons(const Buttons& buttons);

    KifuBranchTree* tree() const { return m_tree; }
    KifuNavigationState* state() const { return m_state; }

public slots:
    // --- ナビゲーション操作 ---

    /// 開始局面へ移動する
    void goToFirst();

    /// 現在ラインの最終手へ移動する
    void goToLast();

    /// @param count 戻る手数（デフォルト1）
    void goBack(int count = 1);

    /// @param count 進む手数（デフォルト1）
    void goForward(int count = 1);

    /// 指定ノードへ移動する（分岐パスの選択記憶も更新）
    void goToNode(KifuBranchNode* node);

    /// 現在ライン上の指定手数へ移動する
    void goToPly(int ply);

    /// 分岐ラインを切り替える
    void switchToLine(int lineIndex);

    /// 分岐候補を選択して移動する
    void selectBranchCandidate(int candidateIndex);

    /// 本譜の同じ手数に移動する
    void goToMainLineAtCurrentPly();

    /**
     * @brief 分岐ツリーのノードがクリックされたときの処理
     * @param row allLines()のインデックス
     * @param ply 手数
     *
     * ナビゲーション状態の更新と各種シグナルの発行を行う。
     * MainWindow側はbranchNodeHandledシグナルを受けて
     * ply追跡変数とSFEN文字列を更新する。
     */
    void handleBranchNodeActivated(int row, int ply);

    // --- ボタンスロット（QPushButton::clicked用） ---
    void onFirstClicked(bool checked = false);
    void onBack10Clicked(bool checked = false);
    void onPrevClicked(bool checked = false);
    void onNextClicked(bool checked = false);
    void onFwd10Clicked(bool checked = false);
    void onLastClicked(bool checked = false);

signals:
    /// ナビゲーション完了を通知する（Controller → MainWindow）
    void navigationCompleted(KifuBranchNode* newNode);

    /// 盤面SFEN更新を要求する（Controller → BoardSyncPresenter）
    void boardUpdateRequired(const QString& sfen);

    /// 棋譜欄ハイライト更新を要求する（Controller → RecordPresenter）
    void recordHighlightRequired(int ply);

    /// 分岐ツリーハイライト更新を要求する（Controller → BranchTreeWidget）
    void branchTreeHighlightRequired(int lineIndex, int ply);

    /// 分岐候補欄の更新を要求する（Controller → KifuBranchDisplay）
    // NOLINTNEXTLINE(clazy-fully-qualified-moc-types) -- QList<Ptr> false positive in clazy 1.17
    void branchCandidatesUpdateRequired(const QList<KifuBranchNode *>& candidates);

    /// 分岐ライン選択変更を通知する（Controller → BranchTreeWidget）
    void lineSelectionChanged(int newLineIndex);

    /**
     * @brief 分岐ノード処理完了を通知する（Controller → MainWindow）
     * @param ply 選択された手数
     * @param sfen ノードのSFEN（空なら更新不要）
     * @param previousFileTo 移動先の筋（検討モード用、0=なし）
     * @param previousRankTo 移動先の段（検討モード用、0=なし）
     * @param lastUsiMove 最後の指し手USI表記（検討モード用）
     */
    void branchNodeHandled(int ply, const QString& sfen,
                           int previousFileTo, int previousRankTo,
                           const QString& lastUsiMove);

private:
    /// 盤面/棋譜欄/分岐ツリーへの更新シグナル群を一括発行する
    void emitUpdateSignals();

    /// 次に進むべき子ノードを返す（分岐選択記憶を優先）
    KifuBranchNode* findForwardNode() const;

    KifuBranchTree* m_tree = nullptr;          ///< 分岐ツリー（非所有）
    KifuNavigationState* m_state = nullptr;    ///< ナビゲーション状態（非所有）
};

#endif // KIFUNAVIGATIONCONTROLLER_H
