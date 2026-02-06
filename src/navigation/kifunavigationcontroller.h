#ifndef KIFUNAVIGATIONCONTROLLER_H
#define KIFUNAVIGATIONCONTROLLER_H

#include <QObject>
#include <QVector>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class QPushButton;

/**
 * @brief 統一されたナビゲーションコントローラー
 *
 * 6つのボタン、棋譜欄クリック、分岐ツリークリック、分岐候補クリック
 * の全てを統一的に処理する。
 */
class KifuNavigationController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief ボタン群の構造体
     */
    struct Buttons {
        QPushButton* first = nullptr;
        QPushButton* back10 = nullptr;
        QPushButton* prev = nullptr;
        QPushButton* next = nullptr;
        QPushButton* fwd10 = nullptr;
        QPushButton* last = nullptr;
    };

    explicit KifuNavigationController(QObject* parent = nullptr);

    /**
     * @brief ツリーと状態を設定
     */
    void setTreeAndState(KifuBranchTree* tree, KifuNavigationState* state);

    /**
     * @brief ボタンを接続
     */
    void connectButtons(const Buttons& buttons);

    /**
     * @brief ツリーを取得
     */
    KifuBranchTree* tree() const { return m_tree; }

    /**
     * @brief 状態を取得
     */
    KifuNavigationState* state() const { return m_state; }

public slots:
    // === ナビゲーション操作 ===

    /**
     * @brief 開始局面へ移動
     */
    void goToFirst();

    /**
     * @brief 現在ラインの最終手へ移動
     */
    void goToLast();

    /**
     * @brief N手戻る
     */
    void goBack(int count = 1);

    /**
     * @brief N手進む
     */
    void goForward(int count = 1);

    /**
     * @brief 指定ノードへ移動
     */
    void goToNode(KifuBranchNode* node);

    /**
     * @brief 現在ラインの指定手数へ移動
     */
    void goToPly(int ply);

    /**
     * @brief 分岐ラインを切り替え
     */
    void switchToLine(int lineIndex);

    /**
     * @brief 分岐候補を選択
     */
    void selectBranchCandidate(int candidateIndex);

    /**
     * @brief 本譜の同じ手数に移動
     */
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

    // === ボタンスロット（bool引数付き） ===
    void onFirstClicked(bool checked = false);
    void onBack10Clicked(bool checked = false);
    void onPrevClicked(bool checked = false);
    void onNextClicked(bool checked = false);
    void onFwd10Clicked(bool checked = false);
    void onLastClicked(bool checked = false);

signals:
    /**
     * @brief ナビゲーションが完了した
     */
    void navigationCompleted(KifuBranchNode* newNode);

    /**
     * @brief 盤面更新が必要
     */
    void boardUpdateRequired(const QString& sfen);

    /**
     * @brief 棋譜欄のハイライト更新が必要
     */
    void recordHighlightRequired(int ply);

    /**
     * @brief 分岐ツリーのハイライト更新が必要
     */
    void branchTreeHighlightRequired(int lineIndex, int ply);

    /**
     * @brief 分岐候補欄の更新が必要
     */
    void branchCandidatesUpdateRequired(const QVector<KifuBranchNode*>& candidates);

    /**
     * @brief 分岐ライン選択が変更された
     */
    void lineSelectionChanged(int newLineIndex);

    /**
     * @brief 分岐ノードが処理された（MainWindow側のply/SFEN更新用）
     * @param ply 選択された手数
     * @param sfen ノードのSFEN（空の場合は更新不要）
     */
    void branchNodeHandled(int ply, const QString& sfen);

private:
    void emitUpdateSignals();
    KifuBranchNode* findForwardNode() const;

    KifuBranchTree* m_tree = nullptr;
    KifuNavigationState* m_state = nullptr;
};

#endif // KIFUNAVIGATIONCONTROLLER_H
