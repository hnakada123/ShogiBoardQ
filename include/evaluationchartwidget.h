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
class QComboBox;
class QHBoxLayout;
class QSettings;

class EvaluationChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvaluationChartWidget(QWidget* parent = nullptr);
    ~EvaluationChartWidget() override;

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

    // X軸設定の取得
    int xAxisLimit() const { return m_xLimit; }
    int xAxisInterval() const { return m_xInterval; }

    // X軸設定の変更
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);

    // フォントサイズの取得・設定
    int labelFontSize() const { return m_labelFontSize; }
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

private slots:
    // ComboBox選択変更時のスロット
    void onYLimitChanged(int index);
    void onXLimitChanged(int index);
    void onXIntervalChanged(int index);

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

    // 評価値上限に応じた適切な間隔を計算（上限の半分）
    int calculateAppropriateYInterval(int yLimit) const;

    // ComboBoxの選択を更新（値からインデックスを探して設定）
    void updateComboBoxSelections();

    // 評価値上限のComboBox選択肢を更新
    void updateYLimitComboItems();

    // エンジン情報ラベルの更新
    void updateEngineInfoLabel();

    // 設定の保存・復元
    void saveSettings();
    void loadSettings();

    // ゼロライン
    void setupZeroLine();
    void updateZeroLine();

    // 現在手数を示す縦線（カーソルライン）
    void setupCursorLine();
    void updateCursorLine();

    // チャート関連
    QChart*      m_chart = nullptr;
    QLineSeries* m_s1    = nullptr;
    QLineSeries* m_s2    = nullptr;
    QLineSeries* m_zeroLine = nullptr;  // ゼロライン
    QLineSeries* m_cursorLine = nullptr;  // 現在手数を示す縦線
    QValueAxis*  m_axX   = nullptr;
    QValueAxis*  m_axY   = nullptr;
    QChartView*  m_chartView = nullptr;

    // コントロールパネル
    QWidget*     m_controlPanel = nullptr;

    // ComboBox
    QComboBox* m_comboYLimit = nullptr;
    QComboBox* m_comboXLimit = nullptr;
    QComboBox* m_comboXInterval = nullptr;

    // フォントサイズボタン
    QPushButton* m_btnFontUp = nullptr;
    QPushButton* m_btnFontDown = nullptr;

    // エンジン情報ラベル
    QLabel* m_lblEngineInfo = nullptr;

    // エンジン情報
    QString m_engine1Name;
    QString m_engine2Name;
    int m_engine1Ply = 0;
    int m_engine2Ply = 0;
    int m_engine1Cp = 0;
    int m_engine2Cp = 0;

    // Y軸設定
    int m_yLimit = 2000;      // Y軸の上限（下限は -m_yLimit）
    int m_yInterval = 1000;   // Y軸の目盛り間隔（上限の半分）

    // X軸設定
    int m_xLimit = 500;       // X軸の上限
    int m_xInterval = 10;     // X軸の目盛り間隔

    // フォントサイズ
    int m_labelFontSize = 7;

    // 現在の手数（縦線表示用）
    int m_currentPly = 0;

    // 利用可能な上限値のリスト（Y軸）
    static const QList<int> s_availableYLimits;
    // 利用可能な上限値のリスト（X軸）
    static const QList<int> s_availableXLimits;
    // 利用可能な間隔値のリスト（X軸）
    static const QList<int> s_availableXIntervals;
    // 利用可能なフォントサイズのリスト
    static const QList<int> s_availableFontSizes;
};

#endif // EVALUATIONCHARTWIDGET_H
