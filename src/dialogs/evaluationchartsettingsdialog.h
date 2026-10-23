#ifndef EVALUATIONCHARTSETTINGSDIALOG_H
#define EVALUATIONCHARTSETTINGSDIALOG_H

/// @file evaluationchartsettingsdialog.h
/// @brief 評価値グラフの表示設定のダイアログ

#include <QDialog>

#include "evaluationchartrange.h"

class QComboBox;
class QSpinBox;

/**
 * @brief 評価値グラフの表示設定（表示範囲・目盛りの間隔・文字の大きさ）を選ぶダイアログ
 *
 * 値の範囲は EvaluationChartSettings の定数で決まる。決めた値は settings() で受け取り、
 * EvaluationChartWidget::setChartSettings に渡す。
 */
class EvaluationChartSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// current: 今の設定、axes: 今の目盛り（説明に出す）
    EvaluationChartSettingsDialog(const EvaluationChartSettings& current, const EvaluationChartAxes& axes,
                                  QWidget* parent = nullptr);
    ~EvaluationChartSettingsDialog() override;

    /// ダイアログで選んだ設定
    EvaluationChartSettings settings() const;

private:
    QComboBox* m_automatic = nullptr;
    QSpinBox* m_yLimit = nullptr;
    QSpinBox* m_xLimit = nullptr;
    QSpinBox* m_yInterval = nullptr;
    QSpinBox* m_xInterval = nullptr;
    QSpinBox* m_fontSize = nullptr;
};

#endif // EVALUATIONCHARTSETTINGSDIALOG_H
