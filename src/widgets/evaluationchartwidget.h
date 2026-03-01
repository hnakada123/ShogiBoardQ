#ifndef EVALUATIONCHARTWIDGET_H
#define EVALUATIONCHARTWIDGET_H

/// @file evaluationchartwidget.h
/// @brief 評価値グラフウィジェットクラスの定義


#pragma once
#include <QWidget>
#include <QPointF>

class QChart;
class QLineSeries;
class QValueAxis;
class QChartView;
class QLabel;
class QTimer;
class EvaluationChartConfigurator;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget() override;

    /// チャート描画ウィジェット（画像保存用）
    QWidget* chartViewWidget() const;

    void appendScoreP1(int ply, int cp, bool invert = false);
    void appendScoreP2(int ply, int cp, bool invert = false);

    /// バッファリング版: 100ms間隔でまとめて描画更新する
    void appendScoreP1Buffered(int ply, int cp, bool invert = false);
    void appendScoreP2Buffered(int ply, int cp, bool invert = false);

    /// ペンディングバッファを即座にフラッシュする
    void flushPendingScores();

    /// 全スコアを一括置換する（QLineSeries::replace()使用）
    void replaceAllScoresP1(const QList<QPointF>& points);
    void replaceAllScoresP2(const QList<QPointF>& points);

    void clearAll();

    void removeLastP1();
    void removeLastP2();

    // 指定した手数以降のデータを削除
    void trimToPly(int maxPly);

    int countP1() const;
    int countP2() const;

    // Y軸設定の取得
    int yAxisLimit() const;
    int yAxisInterval() const;

    // Y軸設定の変更
    void setYAxisLimit(int limit);

    // X軸設定の取得
    int xAxisLimit() const;
    int xAxisInterval() const;

    // Y軸間隔の変更
    void setYAxisInterval(int interval);

    // X軸設定の変更
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);

    // フォントサイズの取得・設定
    int labelFontSize() const;
    void setLabelFontSize(int size);

    // エンジン情報の設定
    void setEngine1Info(const QString& name, int ply, int cp);
    void setEngine2Info(const QString& name, int ply, int cp);
    void setEngine1Name(const QString& name);
    void setEngine2Name(const QString& name);

    // 現在の手数を示す縦線の設定
    void setCurrentPly(int ply);
    int currentPly() const { return m_currentPly; }

signals:
    // Y軸設定が変更されたときに発行
    void yAxisSettingsChanged(int limit, int interval);
    // X軸設定が変更されたときに発行
    void xAxisSettingsChanged(int limit, int interval);
    // グラフクリック時に手数を発行
    void plyClicked(int ply);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

public slots:
    // ドッキング状態の変更（QDockWidget::topLevelChangedから呼び出す）
    void setFloating(bool floating);

private slots:
    // Configuratorからの設定変更通知に対応
    void applyYAxisSettings();
    void applyXAxisSettings();
    void applyFontSize();

    // プロットホバー時のツールチップ表示
    void onSeriesHovered(const QPointF& point, bool state);

private:
    void updateLabelFonts();

    // ゼロライン
    void setupZeroLine();
    void updateZeroLine();

    // 現在手数を示す縦線（カーソルライン）
    void setupCursorLine();
    void updateCursorLine();

    // ツールチップ（付箋）のセットアップ
    void setupTooltip();

    // チャート関連
    QChart*      m_chart = nullptr;
    QLineSeries* m_s1    = nullptr;
    QLineSeries* m_s2    = nullptr;
    QLineSeries* m_zeroLine = nullptr;  // ゼロライン
    QLineSeries* m_cursorLine = nullptr;  // 現在手数を示す縦線
    QValueAxis*  m_axX   = nullptr;
    QValueAxis*  m_axY   = nullptr;
    QChartView*  m_chartView = nullptr;

    // ツールチップ（付箋）ラベル
    QLabel* m_tooltip = nullptr;

    // 設定パネル管理
    EvaluationChartConfigurator* m_configurator = nullptr;

    // エンジン情報
    QString m_engine1Name;
    QString m_engine2Name;
    int m_engine1Ply = 0;
    int m_engine2Ply = 0;
    int m_engine1Cp = 0;
    int m_engine2Cp = 0;

    // 現在の手数（縦線表示用）
    int m_currentPly = 0;

    // フローティング状態（ドッキング時はfalse）
    bool m_isFloating = false;

    // バッチ更新用
    struct PendingScore {
        int ply;
        int cp;
        bool invert;
    };
    QList<PendingScore> m_pendingP1;
    QList<PendingScore> m_pendingP2;
    QTimer* m_flushTimer = nullptr;

    void initFlushTimer();
    void appendScoreToSeries(QLineSeries* series, int ply, int cp, bool invert);
};

#endif // EVALUATIONCHARTWIDGET_H
