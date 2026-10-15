#ifndef KIFUNAVIGATIONSTATE_H
#define KIFUNAVIGATIONSTATE_H

/// @file kifunavigationstate.h
/// @brief 棋譜ナビゲーション状態管理クラスの定義


#include <QObject>
#include <QHash>
#include <QList>

#include "recordline.h"

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
    KifuBranchNode* currentNode() const;

    /**
     * @brief 現在の手数を取得
     */
    int currentPly() const;

    /**
     * @brief 現在のラインインデックスを取得（0=本譜、1以降=分岐）
     * 選択したノードIDの経路から、その時点の行番号を導出する。
     */
    int currentLineIndex() const;

    /**
     * @brief 表示行に対応する経路を選択（保存するのはノードID）
     * 分岐を選択した時に呼び出す。分岐点より前に戻っても維持される。
     */
    void setPreferredLineIndex(int lineIndex);

    /**
     * @brief 経路の選択記憶を消去する
     */
    void resetPreferredLineIndex();

    /**
     * @brief 分岐点での子選択の記憶をクリア（本譜に戻る時に呼び出す）
     *
     * 分岐点での選択記憶（ノードIDで保持する経路）をクリアします。
     * これにより、本譜に戻った後のナビゲーションで正しく本譜を辿ります。
     */
    void clearLineSelectionMemory();

    /**
     * @brief 現在の経路に対応する分岐行番号。本譜は -1（互換API）
     */
    int preferredLineIndex() const;

    /// 選択した経路で次に進むノード。表示上の行番号には依存しない。
    KifuBranchNode* nextNode(KifuBranchNode* node) const;
    QList<KifuBranchNode*> selectedPath() const;

    /**
     * @brief 現在位置のSFENを取得
     */
    QString currentSfen() const;

    /**
     * @brief 表示中の手順（選んでいる分岐の手順。終局行は除く）
     *
     * 局面列・指し手・棋譜の表記と表示中の手数をまとめて返す。対局中はライブの手も含む。
     */
    RecordLine currentRecordLine() const;

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
    // どの子（安定したノードID）を選んだかを記憶する。
    // 引数と返値のインデックスは、呼び出した時点でノードIDから変換する。

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
    int m_currentNodeId = -1;

    // 分岐点のノードID → 選んだ子のノードID。並べ替えで意味が変わらない。
    // ライン番号はこの経路から表示時に導出し、別の状態として保持しない。
    QHash<int, int> m_selectedChildIds;
};

#endif // KIFUNAVIGATIONSTATE_H
