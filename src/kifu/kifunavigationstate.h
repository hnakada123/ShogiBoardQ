#ifndef KIFUNAVIGATIONSTATE_H
#define KIFUNAVIGATIONSTATE_H

/// @file kifunavigationstate.h
/// @brief 棋譜ナビゲーション状態管理クラスの定義


#include <QObject>
#include <QHash>
#include <QList>

class KifuBranchTree;
class KifuBranchNode;

/**
 * @brief 棋譜ナビゲーションの現在状態を管理するクラス
 *
 * 現在位置と、分岐点での最後の選択を記憶する。
 */
class KifuNavigationState : public QObject
{
    Q_OBJECT

public:
    explicit KifuNavigationState(QObject* parent = nullptr);

    /**
     * @brief ツリーを設定
     */
    void setTree(KifuBranchTree* tree);

    /**
     * @brief ツリーを取得
     */
    KifuBranchTree* tree() const { return m_tree; }

    // === 現在位置 ===

    /**
     * @brief 現在のノードを取得
     */
    KifuBranchNode* currentNode() const { return m_currentNode; }

    /**
     * @brief 現在の手数を取得
     */
    int currentPly() const;

    /**
     * @brief 現在のラインインデックスを取得（0=本譜、1以降=分岐）
     * 分岐点より前にいる場合でも、優先ラインが設定されていればそれを返す
     */
    int currentLineIndex() const;

    /**
     * @brief 優先ラインインデックスを設定
     * 分岐を選択した時に呼び出す。分岐点より前に戻っても維持される。
     */
    void setPreferredLineIndex(int lineIndex);

    /**
     * @brief 優先ラインインデックスをリセット（本譜に戻る時に呼び出す）
     */
    void resetPreferredLineIndex();

    /**
     * @brief 分岐点での子選択の記憶をクリア（本譜に戻る時に呼び出す）
     *
     * 分岐点での選択記憶（m_lastSelectedChildAtBranch）をクリアします。
     * これにより、本譜に戻った後のナビゲーションで正しく本譜を辿ります。
     */
    void clearLineSelectionMemory();

    /**
     * @brief 優先ラインインデックスを取得（デバッグ用）
     */
    int preferredLineIndex() const { return m_preferredLineIndex; }

    /**
     * @brief 現在のライン名を取得（"本譜" または "分岐N"）
     */
    QString currentLineName() const;

    /**
     * @brief 現在位置のSFENを取得
     */
    QString currentSfen() const;

    // === 状態設定 ===

    /**
     * @brief 現在のノードを設定
     */
    void setCurrentNode(KifuBranchNode* node);

    /**
     * @brief ルートに移動
     */
    void goToRoot();

    // === クエリ ===

    /**
     * @brief 現在のライン（棋譜欄に表示中のライン）が本譜かどうか
     *
     * currentLineIndex() == 0 と同義。優先ラインが設定されていればそれを反映する。
     * KifuBranchNode::isMainLine() は「親の最初の子か」しか見ないため、
     * 入れ子分岐の先頭子ノードでは本譜でなくても true になる。
     * 表示上の「本譜」判定（「本譜へ戻る」の表示など）には必ずこちらを使う。
     */
    bool isOnMainLine() const;

    /**
     * @brief 進めるかどうか
     */
    bool canGoForward() const;

    /**
     * @brief 戻れるかどうか
     */
    bool canGoBack() const;

    /**
     * @brief 現在ラインの最大手数を取得
     */
    int maxPlyOnCurrentLine() const;

    // === 分岐候補 ===

    /**
     * @brief 現在位置の分岐候補を取得
     */
    QList<KifuBranchNode*> branchCandidatesAtCurrent() const;

    /**
     * @brief 現在位置に分岐があるかどうか
     */
    bool hasBranchAtCurrent() const;

    // === 分岐点での子選択の記憶 ===
    // 「戻る→進む」で直前に通った分岐を辿れるよう、分岐点ごとに
    // どの子（childAt のインデックス）を選んだかを記憶する。
    // allLines() のラインインデックスとは別物なので注意。

    /**
     * @brief 分岐点で選んだ子を記憶
     * @param branchPoint 分岐点のノード
     * @param childIndex 選んだ子のインデックス（branchPoint->childAt() の添字）
     */
    void rememberChildSelection(KifuBranchNode* branchPoint, int childIndex);

    /**
     * @brief 分岐点で最後に選んだ子のインデックスを取得
     * @param branchPoint 分岐点のノード
     * @return 子のインデックス（未選択の場合は0=最初の子）
     */
    int lastSelectedChildAt(KifuBranchNode* branchPoint) const;

    /**
     * @brief ルートから指定ノードまでの経路上にある全分岐点で、経路が通る子を記憶する
     * @param node 経路の終端ノード
     *
     * 「戻る→進む」でこのノードまでの経路を再び辿れるようにする。
     */
    void rememberPathSelections(KifuBranchNode* node);

private:
    KifuBranchTree* m_tree = nullptr;
    KifuBranchNode* m_currentNode = nullptr;

    // 分岐点のnodeId -> 選んだ子のインデックス
    QHash<int, int> m_lastSelectedChildAtBranch;

    // 優先ラインインデックス（分岐選択時に設定、分岐点より前に戻っても維持）
    // -1 = 未設定（ノードのlineIndexを使用）
    int m_preferredLineIndex = -1;
};

#endif // KIFUNAVIGATIONSTATE_H
