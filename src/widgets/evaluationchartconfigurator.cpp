/// @file evaluationchartconfigurator.cpp
/// @brief 評価値グラフの表示設定と自動目盛り
#include "evaluationchartconfigurator.h"
#include "analysissettings.h"
#include "dialogfontscale.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

namespace {
int niceCeiling(double value)
{
    const double magnitude = std::pow(10.0, std::floor(std::log10(qMax(1.0, value))));
    for (int step : {1, 2, 5, 10}) {
        const double candidate = step * magnitude;
        if (candidate >= value) return qMax(1, static_cast<int>(qMin(candidate, 100000000.0)));
    }
    return 1;
}

QSpinBox* spinBox(QWidget* parent, const char* name, int min, int max, int value)
{
    auto* spin = new QSpinBox(parent);
    spin->setObjectName(QString::fromLatin1(name));
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}
} // namespace

EvaluationChartConfigurator::EvaluationChartConfigurator(QObject* parent) : QObject(parent) {}

QWidget* EvaluationChartConfigurator::createRangeSelector(QWidget* parentWidget)
{
    auto* selector = new QWidget(parentWidget);
    auto* layout = new QHBoxLayout(selector);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* label = new QLabel(tr("表示範囲:"), selector);
    m_mode = new QComboBox(selector);
    m_mode->setObjectName(QStringLiteral("evalRangeMode"));
    m_mode->addItems({tr("自動"), tr("手動固定")});
    m_mode->setCurrentIndex(m_automatic ? 0 : 1);
    label->setBuddy(m_mode);
    layout->addWidget(label);
    layout->addWidget(m_mode);
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EvaluationChartConfigurator::onRangeModeChanged);
    return selector;
}

QPushButton* EvaluationChartConfigurator::createSettingsButton(QWidget* parentWidget)
{
    auto* settings = new QPushButton(tr("表示設定…"), parentWidget);
    settings->setObjectName(QStringLiteral("evalDisplaySettings"));
    m_dialogParent = settings;
    connect(settings, &QPushButton::clicked, this, &EvaluationChartConfigurator::showSettings);
    return settings;
}

void EvaluationChartConfigurator::loadSettings()
{
    m_manualYLimit = qBound(100, AnalysisSettings::evalChartYLimit(), 100000);
    m_manualXLimit = qBound(1, AnalysisSettings::evalChartXLimit(), 10000);
    m_requestedYInterval = qBound(0, AnalysisSettings::evalChartYInterval(), 100000);
    m_requestedXInterval = qBound(0, AnalysisSettings::evalChartXInterval(), 10000);
    m_labelFontSize = qBound(8, AnalysisSettings::evalChartLabelFontSize(), 18);
    m_automatic = AnalysisSettings::evalChartAutomaticRange();
    recalculate();
}

void EvaluationChartConfigurator::saveSettings()
{
    AnalysisSettings::setEvalChartYLimit(m_manualYLimit);
    AnalysisSettings::setEvalChartXLimit(m_manualXLimit);
    AnalysisSettings::setEvalChartYInterval(m_requestedYInterval);
    AnalysisSettings::setEvalChartXInterval(m_requestedXInterval);
    AnalysisSettings::setEvalChartLabelFontSize(m_labelFontSize);
    AnalysisSettings::setEvalChartAutomaticRange(m_automatic);
}

void EvaluationChartConfigurator::recalculate()
{
    const int yLimit = m_automatic ? niceCeiling(qMax(1000, m_maxCp)) : m_manualYLimit;
    const int xLimit = m_automatic ? qMax(20, ((m_maxPly + 9) / 10) * 10) : m_manualXLimit;
    QFont labelFont;
    labelFont.setPointSize(m_labelFontSize);
    const QFontMetrics fm(labelFont);
    const int xSlots = qMax(1, static_cast<int>(m_plotSize.width() /
                            qMax(64, fm.horizontalAdvance(QString::number(xLimit)) + 28)));
    // 正負対称の目盛りを保ち、文字サイズに応じて縦方向も間引く。
    const int halfYSlots = qBound(1, static_cast<int>(m_plotSize.height() / (fm.height() * 3.0)), 2);
    const int yMinimum = qMax(1, yLimit / halfYSlots);
    const int yInterval = m_requestedYInterval == 0 ? yMinimum :
        m_requestedYInterval * qMax(1, static_cast<int>(std::ceil(double(yMinimum) / m_requestedYInterval)));
    const int xMinimum = niceCeiling(double(xLimit) / xSlots);
    const int xInterval = m_requestedXInterval == 0 ? xMinimum :
        m_requestedXInterval * qMax(1, static_cast<int>(std::ceil(double(xMinimum) / m_requestedXInterval)));
    if (m_yLimit != yLimit || m_yInterval != yInterval) {
        m_yLimit = yLimit;
        m_yInterval = yInterval;
        emit yAxisSettingsChanged(yLimit, yInterval);
    }
    if (m_xLimit != xLimit || m_xInterval != xInterval) {
        m_xLimit = xLimit;
        m_xInterval = xInterval;
        emit xAxisSettingsChanged(xLimit, xInterval);
    }
}

void EvaluationChartConfigurator::setAutomaticRange(bool automatic)
{
    m_automatic = automatic;
    if (m_mode) {
        const QSignalBlocker blocker(m_mode);
        m_mode->setCurrentIndex(automatic ? 0 : 1);
    }
    recalculate();
}

void EvaluationChartConfigurator::onRangeModeChanged(int index)
{
    setAutomaticRange(index == 0);
    saveSettings();
}

void EvaluationChartConfigurator::setYAxisLimit(int limit)
{
    m_manualYLimit = qBound(100, limit, 100000);
    setAutomaticRange(false);
}

void EvaluationChartConfigurator::setXAxisLimit(int limit)
{
    m_manualXLimit = qBound(1, limit, 10000);
    setAutomaticRange(false);
}

void EvaluationChartConfigurator::setYAxisInterval(int interval)
{
    m_requestedYInterval = qBound(0, interval, 100000);
    recalculate();
}

void EvaluationChartConfigurator::setXAxisInterval(int interval)
{
    m_requestedXInterval = qBound(0, interval, 10000);
    recalculate();
}

void EvaluationChartConfigurator::setLabelFontSize(int size)
{
    m_labelFontSize = qBound(8, size, 18);
    emit fontSizeChanged(m_labelFontSize);
    recalculate();
}

void EvaluationChartConfigurator::updateDataRange(int maxCp, int maxPly)
{
    m_maxCp = maxCp;
    m_maxPly = maxPly;
    recalculate();
}

void EvaluationChartConfigurator::updatePlotSize(const QSizeF& size)
{
    if (size.width() <= 0 || size.height() <= 0 || size == m_plotSize) return;
    m_plotSize = size;
    recalculate();
}

void EvaluationChartConfigurator::showSettings()
{
    QDialog dialog(m_dialogParent);
    dialog.setObjectName(QStringLiteral("evalChartSettings"));
    dialog.setWindowTitle(tr("評価値グラフの表示設定"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* automatic = new QComboBox(&dialog);
    automatic->addItems({tr("自動"), tr("手動固定")});
    automatic->setCurrentIndex(m_automatic ? 0 : 1);
    auto* yLimit = spinBox(&dialog, "evalYLimit", 100, 100000, m_manualYLimit);
    yLimit->setPrefix(QStringLiteral("±"));
    yLimit->setEnabled(!m_automatic);
    auto* xLimit = spinBox(&dialog, "evalXLimit", 1, 10000, m_manualXLimit);
    xLimit->setEnabled(!m_automatic);
    connect(automatic, QOverload<int>::of(&QComboBox::currentIndexChanged), yLimit, &QWidget::setEnabled);
    connect(automatic, QOverload<int>::of(&QComboBox::currentIndexChanged), xLimit, &QWidget::setEnabled);
    auto* yInterval = spinBox(&dialog, "evalYInterval", 0, 100000, m_requestedYInterval);
    auto* xInterval = spinBox(&dialog, "evalXInterval", 0, 10000, m_requestedXInterval);
    yInterval->setSpecialValueText(tr("自動"));
    xInterval->setSpecialValueText(tr("自動"));
    auto* size = spinBox(&dialog, "evalFontSize", 8, 18, m_labelFontSize);
    size->setSuffix(QStringLiteral(" pt"));
    form->addRow(tr("表示範囲:"), automatic);
    form->addRow(tr("評価値の範囲（手動）:"), yLimit);
    form->addRow(tr("手数の上限（手動）:"), xLimit);
    form->addRow(tr("評価値の目盛り間隔:"), yInterval);
    form->addRow(tr("手数の目盛り間隔:"), xInterval);
    form->addRow(tr("文字サイズ:"), size);
    layout->addLayout(form);
    auto* note = new QLabel(tr("目盛りは文字が重ならないように間引きます。詰みはグラフの端に表示します。"), &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* effective = new QLabel(tr("現在の目盛り間隔：評価値 %1 ／ 手数 %2")
                                .arg(m_yInterval).arg(m_xInterval), &dialog);
    layout->addWidget(effective);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    DialogFontScale::install(&dialog, QStringLiteral("evaluationChart"));
    dialog.resize(AnalysisSettings::evalChartSettingsSize());
    if (dialog.exec() == QDialog::Accepted) {
        m_manualYLimit = yLimit->value();
        m_manualXLimit = xLimit->value();
        m_requestedYInterval = yInterval->value();
        m_requestedXInterval = xInterval->value();
        setLabelFontSize(size->value());
        setAutomaticRange(automatic->currentIndex() == 0);
        saveSettings();
    }
    AnalysisSettings::setEvalChartSettingsSize(dialog.size());
}
