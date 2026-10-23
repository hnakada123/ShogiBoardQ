#ifndef ANALYSISRESULTHANDLER_H
#define ANALYSISRESULTHANDLER_H

/// @file analysisresulthandler.h
/// @brief 解析結果ハンドラクラスの定義


#include <QCoreApplication>
#include <QString>
#include <optional>

#include "analysisjob.h"
#include "recordline.h"

class KifuAnalysisListModel;
class AnalysisResultsPanel;

/**
 * @brief 棋譜解析の1局面の結果（PlyAnalysis）を結果の表の1行にする
 *
 * KifuAnalysisController が解析1回ごとに作る。評価値は先手から見た値に直し、直前の行との増減と
 * 候補手（前の行の読み筋の初手）を添える。結果の行のダブルクリックで読み筋の盤面ダイアログも出す。
 */
class AnalysisResultHandler
{
    Q_DECLARE_TR_FUNCTIONS(AnalysisResultHandler)
public:
    /// 外部参照オブジェクト
    struct Refs {
        KifuAnalysisListModel* analysisModel = nullptr;  ///< 解析結果モデル（非所有）
        const RecordLine* line = nullptr;               ///< 実行が所有する解析対象
        QSet<int> branchPlies;                           ///< 分岐がある手の手数（表で分岐の印を付ける）
        AnalysisResultsPanel* presenter = nullptr;   ///< 結果表示プレゼンタ（非所有）
        QString blackPlayerName;                         ///< 先手名
        QString whitePlayerName;                         ///< 後手名
        bool boardFlipped = false;                       ///< GUI本体の盤面反転状態
    };

    /// 解析1回分の参照を受け取る（参照先は解析の実行が持つ）
    explicit AnalysisResultHandler(const Refs& refs);

    /**
     * @brief 1局面の結果を表に足す
     * @return 先手から見た評価値（定跡・評価値なしは None。評価値グラフの点に使う）
     */
    UsiScore append(const PlyAnalysis& result);

    /// 結果行ダブルクリック時に読み筋盤面ダイアログを表示する
    void showPvBoardDialog(int row);

private:
    Refs m_refs;
    std::optional<int> m_previousCp;   ///< 直前の結果の評価値（増減の計算。比べられなければ nullopt）
};

#endif // ANALYSISRESULTHANDLER_H
