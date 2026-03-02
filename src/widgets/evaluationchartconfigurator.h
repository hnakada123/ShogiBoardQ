/// @file evaluationchartconfigurator.h
/// @brief 評価値グラフの設定パネル管理クラスの定義

#ifndef EVALUATIONCHARTCONFIGURATOR_H
#define EVALUATIONCHARTCONFIGURATOR_H

#include <QObject>
#include <QList>

class QWidget;
class QComboBox;
class QLabel;
class QPushButton;

/// @brief 評価値グラフの設定パネルとコントロールを管理する
///
/// コントロールパネル（ComboBox、フォントサイズボタン）の生成・接続と、
/// 軸設定値の管理・永続化を担当する。
class EvaluationChartConfigurator : public QObject
{
    Q_OBJECT
public:
    explicit EvaluationChartConfigurator(QObject* parent = nullptr);

    /// コントロールパネルウィジェットを生成して返す
    QWidget* createControlPanel(QWidget* parentWidget);

    /// 設定の保存・復元
    void saveSettings();
    void loadSettings();

    // --- Getters ---
    int yAxisLimit() const { return m_yLimit; }
    int yAxisInterval() const { return m_yInterval; }
    int xAxisLimit() const { return m_xLimit; }
    int xAxisInterval() const { return m_xInterval; }
    int labelFontSize() const { return m_labelFontSize; }

    // --- Setters（プログラムからの変更用、シグナル発行あり） ---
    void setYAxisLimit(int limit);
    void setYAxisInterval(int interval);
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);
    void setLabelFontSize(int size);

    /// 評価値が軸範囲を超えた場合の自動拡大
    void autoExpandYAxisIfNeeded(int cp);
    void autoExpandXAxisIfNeeded(int ply);

    /// 評価値上限に応じた適切な間隔を計算（上限の半分）
    int calculateAppropriateYInterval(int yLimit) const;

    // 利用可能な値リスト（static）
    static const QList<int>& availableYLimits();
    static const QList<int>& availableXLimits();

signals:
    void yAxisSettingsChanged(int limit, int interval);
    void xAxisSettingsChanged(int limit, int interval);
    void fontSizeChanged(int size);

private slots:
    void onYLimitChanged(int index);
    void onYIntervalChanged(int index);
    void onXLimitChanged(int index);
    void onXIntervalChanged(int index);
    void onIncreaseFontSizeClicked();
    void onDecreaseFontSizeClicked();

private:
    void updateComboBoxSelections();
    void updateYLimitComboItems();
    void syncComboBoxToYLimit();
    void syncComboBoxToYInterval();
    void syncComboBoxToXLimit();
    void syncComboBoxToXInterval();

    // 設定値
    int m_yLimit = 2000;
    int m_yInterval = 1000;
    int m_xLimit = 500;
    int m_xInterval = 10;
    int m_labelFontSize = 7;

    // ComboBox（非所有、親ウィジェット所有）
    QComboBox* m_comboYLimit = nullptr;
    QComboBox* m_comboYInterval = nullptr;
    QComboBox* m_comboXLimit = nullptr;
    QComboBox* m_comboXInterval = nullptr;

    // ラベル・ボタン（非所有、親ウィジェット所有）
    QLabel* m_lblYInterval = nullptr;
    QPushButton* m_btnFontUp = nullptr;
    QPushButton* m_btnFontDown = nullptr;

    // 利用可能な値リスト
    static const QList<int> s_availableYLimits;
    static const QList<int> s_availableYIntervals;
    static const QList<int> s_availableXLimits;
    static const QList<int> s_availableXIntervals;
    static const QList<int> s_availableFontSizes;
};

#endif // EVALUATIONCHARTCONFIGURATOR_H
