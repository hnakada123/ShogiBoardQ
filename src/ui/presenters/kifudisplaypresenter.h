#ifndef KIFUDISPLAYPRESENTER_H
#define KIFUDISPLAYPRESENTER_H

/// @file kifudisplaypresenter.h
/// @brief 棋譜表示データ構築・一致性検証プレゼンタの定義


#include "branchtreemanager.h"

#include <QString>
#include <QVector>
#include <QSet>
#include <functional>

class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;
class KifuRecordListModel;
class KifuBranchListModel;
class RecordPane;
struct BranchLine;

/**
 * @brief 棋譜表示データの構築と一致性検証を担当するプレゼンタ
 *
 * KifuDisplayCoordinatorから分離された責務:
 * - 棋譜欄モデルの構築（populateRecordModel, populateRecordModelFromPath）
 * - 分岐マーク計算（populateBranchMarks）
 * - 分岐ツリー行データ構築（buildBranchTreeRows）
 * - 表示一致性検証（captureDisplaySnapshot, verifyDisplayConsistencyDetailed等）
 */
class KifuDisplayPresenter
{
public:
    struct Refs {
        KifuBranchTree* tree = nullptr;
        KifuNavigationState* state = nullptr;
        KifuRecordListModel* recordModel = nullptr;
        KifuBranchListModel* branchModel = nullptr;
        BranchTreeManager* branchTreeManager = nullptr;
        std::function<QString()> boardSfenProvider;
    };

    struct TrackingState {
        int lastLineIndex = 0;
        int expectedTreeLineIndex = 0;
        int expectedTreePly = 0;
    };

    struct DisplaySnapshot {
        int stateLineIndex = -1;
        int statePly = -1;
        bool stateOnMainLine = true;
        int trackedLineIndex = -1;
        int modelLineIndex = -1;
        int modelRowCount = -1;
        int modelHighlightRow = -1;
        int expectedTreeLineIndex = -1;
        int expectedTreePly = -1;
        int treeHighlightLineIndex = -1;
        int treeHighlightPly = -1;
        int branchCandidateCount = -1;
        bool hasBackToMainRow = false;
        QString stateSfen;
        QString stateSfenNormalized;
        QString boardSfen;
        QString boardSfenNormalized;
        QString displayedMoveAtPly;
        QString expectedMoveAtPly;
    };

    using BoardSfenProvider = std::function<QString()>;

    KifuDisplayPresenter();

    void updateRefs(const Refs& refs);

    // === 棋譜欄モデル構築 ===

    /**
     * @brief 現在のラインから棋譜欄モデルを構築する
     */
    void populateRecordModel();

    /**
     * @brief 指定パスから棋譜欄モデルを構築する
     * @param path ノードパス
     * @param highlightPly ハイライトする手数
     * @return ハイライトすべき行番号
     */
    int populateRecordModelFromPath(const QVector<KifuBranchNode*>& path, int highlightPly);

    /**
     * @brief 分岐マークを計算して棋譜欄モデルに設定する
     */
    void populateBranchMarks();

    // === 分岐ツリー行データ構築 ===

    /**
     * @brief ツリーからBranchTreeManager用の行データを構築する
     * @param tree 分岐ツリー
     * @return ResolvedRowLite のリスト
     */
    static QVector<BranchTreeManager::ResolvedRowLite> buildBranchTreeRows(KifuBranchTree* tree);

    // === 一致性検証 ===

    DisplaySnapshot captureDisplaySnapshot(const TrackingState& tracking) const;
    bool verifyDisplayConsistencyDetailed(const TrackingState& tracking, QString* reason = nullptr) const;
    bool verifyDisplayConsistency(const TrackingState& tracking) const;
    QString getConsistencyReport(const TrackingState& tracking) const;

    // === アクセサ ===

    int lastModelLineIndex() const { return m_lastModelLineIndex; }
    void setLastModelLineIndex(int index) { m_lastModelLineIndex = index; }

private:
    Refs m_refs;
    int m_lastModelLineIndex = -1;
};

#endif // KIFUDISPLAYPRESENTER_H
