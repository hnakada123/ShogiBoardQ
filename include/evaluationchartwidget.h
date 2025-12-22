#ifndef EVALUATIONCHARTWIDGET_H
#define EVALUATIONCHARTWIDGET_H

#pragma once
#include <QWidget>

class QChart;
class QLineSeries;
class QValueAxis;
class QChartView;
class QLabel;
class QPushButton;
class QHBoxLayout;
class QSettings;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget();

    void appendScoreP1(int ply, int cp, bool invert = false);
    void appendScoreP2(int ply, int cp, bool invert = false);
    void clearAll();

    void removeLastP1();
    void removeLastP2();

    int countP1() const;
    int countP2() const;

    // Y軸設定の取得
    int yAxisLimit() const { return m_yLimit; }
    int yAxisInterval() const { return m_yInterval; }

    // Y軸設定の変更
    void setYAxisLimit(int limit);
    void setYAxisInterval(int interval);

    // X軸設定の取得
    int xAxisLimit() const { return m_xLimit; }
    int xAxisInterval() const { return m_xInterval; }

    // X軸設定の変更
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);

    // フォントサイズの取得・設定
    int labelFontSize() const { return m_labelFontSize; }
    void setLabelFontSize(int size);

signals:
    // Y軸設定が変更されたときに発行
    void yAxisSettingsChanged(int limit, int interval);
    // X軸設定が変更されたときに発行
    void xAxisSettingsChanged(int limit, int interval);

private slots:
    // Y軸上限の増減
    void onIncreaseYLimitClicked();
    void onDecreaseYLimitClicked();

    // Y軸間隔の増減
    void onIncreaseYIntervalClicked();
    void onDecreaseYIntervalClicked();

    // X軸上限の増減
    void onIncreaseXLimitClicked();
    void onDecreaseXLimitClicked();

    // X軸間隔の増減
    void onIncreaseXIntervalClicked();
    void onDecreaseXIntervalClicked();

    // フォントサイズの増減
    void onIncreaseFontSizeClicked();
    void onDecreaseFontSizeClicked();

private:
    void setupControlPanel();
    void updateYAxis();
    void updateXAxis();
    void updateLabelFonts();
    void autoExpandYAxisIfNeeded(int cp);
    void autoExpandXAxisIfNeeded(int ply);

    // 設定の保存・復元
    void saveSettings();
    void loadSettings();

    // ゼロライン
    void setupZeroLine();
    void updateZeroLine();

    // チャート関連
    QChart*      m_chart = nullptr;
    QLineSeries* m_s1    = nullptr;
    QLineSeries* m_s2    = nullptr;
    QLineSeries* m_zeroLine = nullptr;  // ゼロライン
    QValueAxis*  m_axX   = nullptr;
    QValueAxis*  m_axY   = nullptr;
    QChartView*  m_chartView = nullptr;

    // コントロールパネル
    QWidget*     m_controlPanel = nullptr;

    // Y軸ボタン
    QPushButton* m_btnYLimitUp = nullptr;
    QPushButton* m_btnYLimitDown = nullptr;
    QPushButton* m_btnYIntervalUp = nullptr;
    QPushButton* m_btnYIntervalDown = nullptr;

    // X軸ボタン
    QPushButton* m_btnXLimitUp = nullptr;
    QPushButton* m_btnXLimitDown = nullptr;
    QPushButton* m_btnXIntervalUp = nullptr;
    QPushButton* m_btnXIntervalDown = nullptr;

    // フォントサイズボタン
    QPushButton* m_btnFontUp = nullptr;
    QPushButton* m_btnFontDown = nullptr;

    // Y軸設定
    int m_yLimit = 2000;      // Y軸の上限（下限は -m_yLimit）
    int m_yInterval = 1000;   // Y軸の目盛り間隔

    // X軸設定
    int m_xLimit = 100;       // X軸の上限
    int m_xInterval = 20;     // X軸の目盛り間隔

    // フォントサイズ
    int m_labelFontSize = 7;

    // 利用可能な上限値のリスト（Y軸）
    static const QList<int> s_availableYLimits;
    // 利用可能な間隔値のリスト（Y軸）
    static const QList<int> s_availableYIntervals;
    // 利用可能な上限値のリスト（X軸）
    static const QList<int> s_availableXLimits;
    // 利用可能な間隔値のリスト（X軸）
    static const QList<int> s_availableXIntervals;
    // 利用可能なフォントサイズのリスト
    static const QList<int> s_availableFontSizes;
};

#endif // EVALUATIONCHARTWIDGET_H
