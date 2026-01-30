#ifndef KIFUNAVIGATIONSTATE_H
#define KIFUNAVIGATIONSTATE_H

#include <QObject>
#include <QHash>
#include <QVector>

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
     */
    int currentLineIndex() const;

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
     * @brief 本譜にいるかどうか
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
    QVector<KifuBranchNode*> branchCandidatesAtCurrent() const;

    /**
     * @brief 現在位置に分岐があるかどうか
     */
    bool hasBranchAtCurrent() const;

    // === ライン選択の記憶 ===

    /**
     * @brief 分岐点での選択を記憶
     * @param branchPoint 分岐点のノード
     * @param lineIndex 選択したラインのインデックス
     */
    void rememberLineSelection(KifuBranchNode* branchPoint, int lineIndex);

    /**
     * @brief 分岐点での最後の選択を取得
     * @param branchPoint 分岐点のノード
     * @return 選択されていたラインインデックス（未選択の場合は0=本譜）
     */
    int lastSelectedLineAt(KifuBranchNode* branchPoint) const;

    /**
     * @brief ライン選択の記憶をクリア
     */
    void clearLineSelections();

signals:
    /**
     * @brief 現在ノードが変更された
     */
    void currentNodeChanged(KifuBranchNode* newNode, KifuBranchNode* oldNode);

    /**
     * @brief ラインが変更された
     */
    void lineChanged(int newLineIndex, int oldLineIndex);

private:
    KifuBranchTree* m_tree = nullptr;
    KifuBranchNode* m_currentNode = nullptr;

    // 分岐点のnodeId -> 選択したラインのインデックス
    QHash<int, int> m_lastSelectedLineAtBranch;
};

#endif // KIFUNAVIGATIONSTATE_H
