#ifndef EVALUATIONGRAPHCONTROLLER_H
#define EVALUATIONGRAPHCONTROLLER_H

/// @file evaluationgraphcontroller.h
/// @brief 評価値グラフコントローラクラスの定義

#include <QObject>
#include <QString>

#include "shogitypes.h"

class EngineLifecycleManager;
class EvaluationChartWidget;
class KifuBranchTree;
class KifuNavigationState;

/**
 * @brief 対局中のエンジンの評価値を評価値グラフに描く
 *
 * エンジンが指した手ごとに、その手番のエンジンが最後に出した評価値をグラフへ足す。
 * 先手のエンジンは P1、後手のエンジンは P2（先手から見た値に符号を反転）として描く。
 */
class EvaluationGraphController : public QObject
{
    Q_OBJECT

public:
    explicit EvaluationGraphController(QObject* parent = nullptr);
    ~EvaluationGraphController() override;

    void setEvalChart(EvaluationChartWidget* chart);
    /// 評価値を読むエンジンの枠
    void setEngines(EngineLifecycleManager* engines);
    /**
     * @brief 横軸の長さ（表示中の手順の手数）を求める分岐ツリーと表示位置を設定する
     *
     * 棋譜の正本（分岐ツリーの表示中の手順）の行数（終局行を含む）- 1 を長さとする。
     */
    void setNavigation(const KifuBranchTree* tree, const KifuNavigationState* navState);

    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);

    /// グラフを空にする
    void clear();
    /// 指定した手数より後の点を消す（待った・途中からの再開）
    void trimToPly(int maxPly);

public slots:
    /**
     * @brief エンジンが指した手の評価値を描く
     *
     * bestmove と同時に届いた info 行を読み終えてから値を取るため、少し遅らせて描く。
     * @param side 指したエンジンの手番
     * @param ply 指した後の手数
     */
    void plotEngineScore(Turn side, int ply);

    /// 現在の手数を設定する（縦線表示用）
    void setCurrentPly(int ply);

private slots:
    void plotPendingBlackScore();
    void plotPendingWhiteScore();

private:
    void plotScore(Turn side, int ply);
    int recordLength() const;

    EvaluationChartWidget* m_evalChart = nullptr;
    EngineLifecycleManager* m_engines = nullptr;
    const KifuBranchTree* m_branchTree = nullptr;
    const KifuNavigationState* m_navState = nullptr;

    QString m_engine1Name;
    QString m_engine2Name;

    int m_pendingBlackPly = -1;  ///< 遅らせて描く先手の手数
    int m_pendingWhitePly = -1;  ///< 遅らせて描く後手の手数
};

#endif // EVALUATIONGRAPHCONTROLLER_H
