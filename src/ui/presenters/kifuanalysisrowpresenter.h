#ifndef KIFUANALYSISROWPRESENTER_H
#define KIFUANALYSISROWPRESENTER_H

/// @file kifuanalysisrowpresenter.h
/// @brief 棋譜解析の1局面の結果を結果の表の1行にする

#include <QSet>
#include <optional>

#include "engineanalysisrunner.h"
#include "recordline.h"

class KifuAnalysisListModel;

/**
 * @brief 棋譜解析の1局面の結果（PlyAnalysis）を結果の表の1行にして足す
 *
 * KifuAnalysisController が解析1回ごとに作る。評価値は先手から見た値に直し、直前の行との増減と
 * 候補手（前の行の読み筋の初手）を添える。
 */
class KifuAnalysisRowPresenter
{
public:
    /// 外部参照オブジェクト
    struct Refs {
        KifuAnalysisListModel* analysisModel = nullptr;  ///< 解析結果モデル（非所有）
        const RecordLine* line = nullptr;               ///< 実行が所有する解析対象
        QSet<int> branchPlies;                           ///< 分岐がある手の手数（表で分岐の印を付ける）
    };

    /// 解析1回分の参照を受け取る（参照先は解析の実行が持つ）
    explicit KifuAnalysisRowPresenter(const Refs& refs);

    /**
     * @brief 1局面の結果を表に足す
     * @return 先手から見た評価値（定跡・評価値なしは None。評価値グラフの点に使う）
     */
    UsiScore append(const PlyAnalysis& result);

private:
    Refs m_refs;
    std::optional<int> m_previousCp;   ///< 直前の結果の評価値（増減の計算。比べられなければ nullopt）
};

#endif // KIFUANALYSISROWPRESENTER_H
