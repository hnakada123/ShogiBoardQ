#ifndef EVALUATIONGRAPHCONTROLLER_H
#define EVALUATIONGRAPHCONTROLLER_H

/// @file evaluationgraphcontroller.h
/// @brief 評価値グラフコントローラクラスの定義


#include <QObject>
#include <QList>

class MatchCoordinator;
class EvaluationChartWidget;

/**
 * @brief EvaluationGraphController - 評価値グラフの管理クラス
 *
 * MainWindowから評価値グラフ関連の処理を分離したクラス。
 * 以下の責務を担当:
 * - 評価値スコアの蓄積 (m_scoreCp)
 * - エンジン1/2の評価値グラフ更新
 * - 遅延実行によるタイミング制御
 */
class EvaluationGraphController : public QObject
{
    Q_OBJECT

public:
    explicit EvaluationGraphController(QObject* parent = nullptr);
    ~EvaluationGraphController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------

    /**
     * @brief EvaluationChartWidget（評価値グラフ表示ウィジェット）を設定
     */
    void setEvalChart(EvaluationChartWidget* chart);

    /**
     * @brief MatchCoordinator（エンジン評価値の取得元）を設定
     */
    void setMatchCoordinator(MatchCoordinator* match);

    /**
     * @brief SFENレコード（手数計算用）を設定
     */
    void setSfenRecord(QStringList* sfenRecord);

    // --------------------------------------------------------
    // エンジン名の設定
    // --------------------------------------------------------

    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);

    // --------------------------------------------------------
    // 評価値スコアの管理
    // --------------------------------------------------------

    /**
     * @brief 評価値スコアリストをクリア
     */
    void clearScores();

    /**
     * @brief 評価値スコアリストを指定した手数までトリム
     * @param maxPly 残す最大手数
     */
    void trimToPly(int maxPly);

    /**
     * @brief 評価値スコアリストへの参照を取得（読み取り専用）
     */
    const QList<int>& scores() const;

    /**
     * @brief 評価値スコアリストへの参照を取得（書き込み可能）
     */
    QList<int>& scoresRef();

    /**
     * @brief 最後のP1評価値プロットを削除（待った機能用）
     */
    void removeLastP1Score();

    /**
     * @brief 最後のP2評価値プロットを削除（待った機能用）
     */
    void removeLastP2Score();

    /**
     * @brief 現在の手数を設定（縦線表示用）
     * @param ply 手数
     */
    void setCurrentPly(int ply);

public slots:
    // --------------------------------------------------------
    // グラフ更新スロット
    // --------------------------------------------------------

    /**
     * @brief エンジン1の評価値グラフを更新（遅延実行）
     * @param ply 手数（-1の場合はsfenRecordから計算）
     */
    void redrawEngine1Graph(int ply = -1);

    /**
     * @brief エンジン2の評価値グラフを更新（遅延実行）
     * @param ply 手数（-1の場合はsfenRecordから計算）
     */
    void redrawEngine2Graph(int ply = -1);

private slots:
    /**
     * @brief エンジン1のグラフ更新の遅延実行
     */
    void doRedrawEngine1Graph();

    /**
     * @brief エンジン2のグラフ更新の遅延実行
     */
    void doRedrawEngine2Graph();

private:
    // 評価値スコアリスト
    QList<int> m_scoreCp;

    // 依存オブジェクト
    EvaluationChartWidget* m_evalChart = nullptr;
    MatchCoordinator* m_match = nullptr;
    QStringList* m_sfenHistory = nullptr;

    // エンジン名
    QString m_engine1Name;
    QString m_engine2Name;

    // 遅延実行用の保留手数
    int m_pendingPlyForEngine1 = -1;
    int m_pendingPlyForEngine2 = -1;
};

#endif // EVALUATIONGRAPHCONTROLLER_H
