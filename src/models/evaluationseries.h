#ifndef EVALUATIONSERIES_H
#define EVALUATIONSERIES_H

/// @file evaluationseries.h
/// @brief 評価値グラフの点（手数ごとの、先手から見た評価値の列が2本）

#include <QMap>
#include <QObject>
#include <QString>
#include <array>

#include "shogitypes.h"
#include "usiscore.h"

/**
 * @brief 評価値グラフに描く評価値の列（1つ目と2つ目の系列）
 *
 * 評価値は先手から見た値で持つ。エンジンの評価値はそれを出した側から見た値なので、
 * 先手から見た値への直しは blackView の1か所だけで行う。グラフ（EvaluationChartWidget）はこの列を描くだけ。
 * 対局では1つ目が先手のエンジン、2つ目が後手のエンジン。検討・棋譜解析・通信対局の自分のエンジンも同じ規則で置く。
 */
class EvaluationSeries : public QObject
{
    Q_OBJECT

public:
    static constexpr int kSeriesCount = 2;

    explicit EvaluationSeries(QObject* parent = nullptr);

    /// viewpoint の側から見た評価値を、先手から見た値にする
    static UsiScore blackView(Turn viewpoint, const UsiScore& score);

    /// viewpoint の側から見た score を、series（0・1）の ply 手目に置く（評価値なしは置かない）
    void setScore(int series, int ply, Turn viewpoint, const UsiScore& score);
    /// series の点（手数 → 先手から見た評価値）
    const QMap<int, UsiScore>& scores(int series) const { return m_scores.at(index(series)); }
    int count(int series) const { return static_cast<int>(scores(series).size()); }
    bool isEmpty() const { return m_scores[0].isEmpty() && m_scores[1].isEmpty(); }

    /// 系列の名前（エンジン名。空なら表示側の既定の名前）
    QString name(int series) const { return m_names.at(index(series)); }
    void setName(int series, const QString& name);

    /// 棋譜解析の点なら、解析した手順の番号（対局・検討の点は -1）
    int analysisLineIndex() const { return m_analysisLineIndex; }
    void setAnalysisLineIndex(int lineIndex);

    /// 全部の点を消す（解析した手順の番号も -1 に戻す）
    void clear();
    /// maxPly 手目より後の点を消す（待った・途中からの再開）
    void trimToPly(int maxPly);

signals:
    /// series の ply 手目に点を置いた
    void scoreAdded(int series, int ply);
    /// 点を消した。maxPly はその手数より後を消した（全部消したときは -1）
    void scoresTrimmed(int maxPly);
    /// 系列の名前・解析した手順の番号が変わった
    void labelsChanged();

private:
    static size_t index(int series) { return static_cast<size_t>(series == 1 ? 1 : 0); }

    std::array<QMap<int, UsiScore>, kSeriesCount> m_scores;
    std::array<QString, kSeriesCount> m_names;
    int m_analysisLineIndex = -1;
};

#endif // EVALUATIONSERIES_H
