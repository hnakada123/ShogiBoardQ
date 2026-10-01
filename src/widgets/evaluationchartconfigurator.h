#ifndef EVALUATIONCHARTCONFIGURATOR_H
#define EVALUATIONCHARTCONFIGURATOR_H

#include <QObject>
#include <QSizeF>

class QWidget;
class QComboBox;

/// 表示範囲・目盛り・文字サイズの設定と永続化。手動設定と自動計算値を分離する。
class EvaluationChartConfigurator : public QObject
{
    Q_OBJECT
public:
    explicit EvaluationChartConfigurator(QObject* parent = nullptr);
    QWidget* createControlPanel(QWidget* parentWidget);
    void saveSettings();
    void loadSettings();

    int yAxisLimit() const { return m_yLimit; }
    int yAxisInterval() const { return m_yInterval; }
    int xAxisLimit() const { return m_xLimit; }
    int xAxisInterval() const { return m_xInterval; }
    int labelFontSize() const { return m_labelFontSize; }
    bool automaticRange() const { return m_automatic; }

    void setYAxisLimit(int limit);
    void setYAxisInterval(int interval);
    void setXAxisLimit(int limit);
    void setXAxisInterval(int interval);
    void setLabelFontSize(int size);
    void setAutomaticRange(bool automatic);
    void updateDataRange(int maxCp, int maxPly);
    void updatePlotSize(const QSizeF& size);

signals:
    void yAxisSettingsChanged(int limit, int interval);
    void xAxisSettingsChanged(int limit, int interval);
    void fontSizeChanged(int size);

private slots:
    void onRangeModeChanged(int index);
    void showSettings();

private:
    void recalculate();
    int m_yLimit = 1000;
    int m_yInterval = 500;
    int m_xLimit = 20;
    int m_xInterval = 5;
    int m_manualYLimit = 2000;
    int m_manualXLimit = 100;
    int m_requestedYInterval = 0;
    int m_requestedXInterval = 0;
    int m_labelFontSize = 10;
    int m_maxCp = 0;
    int m_maxPly = 0;
    bool m_automatic = true;
    QSizeF m_plotSize{800, 240};
    QComboBox* m_mode = nullptr; // 親ウィジェット所有
    QWidget* m_panel = nullptr; // 親ウィジェット所有
};

#endif
