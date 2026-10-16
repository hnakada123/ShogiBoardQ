#ifndef EVALUATIONGRAPHPRESENTER_H
#define EVALUATIONGRAPHPRESENTER_H

/// @file evaluationgraphpresenter.h
/// @brief 評価値グラフの点（対局・通信対局のエンジンの手・棋譜解析）と表示中の手数

#include <QObject>
#include <QString>

#include "shogitypes.h"
#include "usiscore.h"

class EvaluationChartWidget;
class EvaluationSeries;
class KifuNavigationState;
class LiveGameSession;

/**
 * @brief 評価値グラフの点（EvaluationSeries）の書き手
 *
 * 点を足す・消す・系列の名前を付けるのはこのクラスだけで、グラフ（EvaluationChartWidget）は点を描くだけ。
 * - 対局・通信対局: エンジンが指した手ごとに、その手番のエンジンが最後に出した評価値を足す
 *   （先手のエンジンは1つ目、後手のエンジンは2つ目の系列。先手から見た値への直しは EvaluationSeries）
 * - 棋譜解析: 解析した局面の評価値（先手から見た値）を1つ目の系列に足す
 */
class EvaluationGraphPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EvaluationChartWidget* evalChart = nullptr;        ///< 評価値グラフ（必須）
        /// 表示中の手。棋譜をたどるたびに縦線をその手数に合わせる（横軸の長さは表示中の手順の行数 - 1）
        const KifuNavigationState* navState = nullptr;
        const LiveGameSession* liveSession = nullptr;      ///< 対局中の手では縦線を動かさない
    };

    explicit EvaluationGraphPresenter(const Deps& deps, QObject* parent = nullptr);
    ~EvaluationGraphPresenter() override;

    /// グラフの点（グラフ・テストが読む）
    const EvaluationSeries* series() const { return m_series; }

    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);
    /// 棋譜解析を始める（点を消し、解析のエンジン名と解析する手順の番号を付ける）
    void beginAnalysis(const QString& engineName, int lineIndex);
    /// 棋譜解析の ply 手目の評価値（先手から見た値）を足す
    void plotAnalysisScore(int ply, const UsiScore& blackScore);

    /// グラフを空にする
    void clear();
    /// 指定した手数より後の点を消す（待った・途中からの再開）
    void trimToPly(int maxPly);

public slots:
    /**
     * @brief エンジンが指した手の評価値を描く（先手の手は1つ目、後手の手は2つ目の系列）
     * @param side 指したエンジンの手番
     * @param ply 指した後の手数
     * @param score 指した側から見た評価値
     */
    void plotEngineScore(Turn side, int ply, const UsiScore& score);

    /// 縦線を表示中の手の手数に合わせる
    void showCurrentPly();

private:
    int recordLength() const;

    EvaluationChartWidget* m_evalChart = nullptr;
    const KifuNavigationState* m_navState = nullptr;
    const LiveGameSession* m_liveSession = nullptr;

    EvaluationSeries* m_series = nullptr;   ///< グラフの点（このクラスが持ち、書く）
};

#endif // EVALUATIONGRAPHPRESENTER_H
