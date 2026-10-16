#ifndef EVALUATIONGRAPHPRESENTER_H
#define EVALUATIONGRAPHPRESENTER_H

/// @file evaluationgraphpresenter.h
/// @brief 対局中の評価値と表示中の手数を評価値グラフに出す

#include <QObject>
#include <QString>

#include "shogitypes.h"
#include "usiscore.h"

class EvaluationChartWidget;
class KifuNavigationState;
class LiveGameSession;

/**
 * @brief 対局中のエンジンの評価値を評価値グラフに描く
 *
 * エンジンが指した手ごとに、その手番のエンジンが最後に出した評価値をグラフへ足す。
 * 先手のエンジンは P1、後手のエンジンは P2（先手から見た値に符号を反転）として描く。
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

    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);

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

    QString m_engine1Name;
    QString m_engine2Name;
};

#endif // EVALUATIONGRAPHPRESENTER_H
