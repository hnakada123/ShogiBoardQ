#ifndef KIFUSELECTIONSYNC_H
#define KIFUSELECTIONSYNC_H

/// @file kifuselectionsync.h
/// @brief 棋譜選択状態同期クラスの定義


#include <QString>
#include <QList>
#include <QList>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class KifuRecordListModel;
class KifuBranchListModel;
class BranchTreeManager;
class BranchTreeWidget;
class RecordPane;

/**
 * @brief 棋譜欄・分岐ツリー・分岐候補の選択状態同期を担当するクラス
 *
 * KifuDisplayCoordinatorから分離された責務:
 * - 棋譜欄のハイライト適用とスクロール
 * - 分岐ツリーのハイライト適用
 * - 分岐候補モデルの更新
 * - 位置変更時の分岐候補・ツリーハイライト同期
 */
class KifuSelectionSync
{
public:
    struct Refs {
        KifuBranchTree* tree = nullptr;
        KifuNavigationState* state = nullptr;
        KifuRecordListModel* recordModel = nullptr;
        KifuBranchListModel* branchModel = nullptr;
        BranchTreeManager* branchTreeManager = nullptr;
        BranchTreeWidget* branchTreeWidget = nullptr;
        RecordPane* recordPane = nullptr;
    };

    struct CommentInfo {
        int ply = -1;
        QString comment;
        bool asHtml = false;
    };

    KifuSelectionSync();

    void updateRefs(const Refs& refs);

    // === 棋譜欄ハイライト ===

    /**
     * @brief 棋譜欄モデルとナビゲーション状態のライン不一致を検出する
     * @param lastModelLineIndex プレゼンタが管理するモデルのラインインデックス
     * @return モデル再構築が必要な場合は true
     */
    bool checkModelLineMismatch(int lastModelLineIndex) const;

    /**
     * @brief 棋譜欄にハイライトを適用し、ビューをスクロールする
     * @param ply ハイライトする手数
     */
    void applyRecordHighlight(int ply);

    // === 分岐ツリーハイライト ===

    /**
     * @brief 分岐ツリーにハイライトを適用する
     * @param lineIndex ラインインデックス
     * @param ply 手数
     * @return 状態との不一致が検出された場合は true
     */
    bool applyBranchTreeHighlight(int lineIndex, int ply);

    // === 分岐候補更新 ===

    /**
     * @brief 分岐候補をモデルに設定する
     * @param candidates 分岐候補ノードリスト
     */
    void applyBranchCandidates(const QList<KifuBranchNode*>& candidates);

    // === 複合操作 ===

    /**
     * @brief 現在位置のハイライトを全UIに適用する
     * @param lastModelLineIndex プレゼンタが管理するモデルのラインインデックス
     * @return コメント情報（KDCがシグナル発火に使用）
     */
    CommentInfo highlightCurrentPosition(int lastModelLineIndex);

    /**
     * @brief onPositionChanged用: ノードに基づく分岐候補同期
     * @param targetNode 対象ノード
     */
    void syncBranchCandidatesForNode(KifuBranchNode* targetNode);

    /**
     * @brief onPositionChanged用: ノードに基づく分岐ツリーハイライト同期
     * @param targetNode 対象ノード
     * @param ply 手数
     * @param currentLineIndex 現在のラインインデックス
     * @return 実際にハイライトされたラインインデックス
     */
    int syncBranchTreeHighlightForNode(KifuBranchNode* targetNode, int ply, int currentLineIndex);

    // === アクセサ ===

    int expectedTreeLineIndex() const { return m_expectedTreeLineIndex; }
    int expectedTreePly() const { return m_expectedTreePly; }

private:
    Refs m_refs;
    int m_expectedTreeLineIndex = 0;
    int m_expectedTreePly = 0;
};

#endif // KIFUSELECTIONSYNC_H
