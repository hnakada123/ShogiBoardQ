/// @file evaluationchartsettingsdialog.cpp
/// @brief 評価値グラフの表示設定のダイアログの実装

#include "evaluationchartsettingsdialog.h"

#include "analysissettings.h"
#include "appsettings.h"
#include "dialogfontscale.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QSpinBox* spinBox(QWidget* parent, const char* name, int min, int max, int value)
{
    auto* spin = new QSpinBox(parent);
    spin->setObjectName(QString::fromLatin1(name));
    spin->setRange(min, max);
    spin->setValue(value);
    return spin;
}
} // namespace

EvaluationChartSettingsDialog::EvaluationChartSettingsDialog(const EvaluationChartSettings& current,
                                                             const EvaluationChartAxes& axes, QWidget* parent)
    : QDialog(parent)
{
    using S = EvaluationChartSettings;
    setObjectName(QStringLiteral("evalChartSettings"));
    setWindowTitle(tr("評価値グラフの表示設定"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_automatic = new QComboBox(this);
    m_automatic->setObjectName(QStringLiteral("evalAutomaticRange"));
    m_automatic->addItems({tr("自動"), tr("手動固定")});
    m_automatic->setCurrentIndex(current.automatic ? 0 : 1);
    m_yLimit = spinBox(this, "evalYLimit", S::kMinYLimit, S::kMaxYLimit, current.yLimit);
    m_yLimit->setPrefix(QStringLiteral("±"));
    m_yLimit->setEnabled(!current.automatic);
    m_xLimit = spinBox(this, "evalXLimit", S::kMinXLimit, S::kMaxXLimit, current.xLimit);
    m_xLimit->setEnabled(!current.automatic);
    connect(m_automatic, &QComboBox::currentIndexChanged, m_yLimit, &QWidget::setEnabled);
    connect(m_automatic, &QComboBox::currentIndexChanged, m_xLimit, &QWidget::setEnabled);
    m_yInterval = spinBox(this, "evalYInterval", 0, S::kMaxYInterval, current.yInterval);
    m_xInterval = spinBox(this, "evalXInterval", 0, S::kMaxXInterval, current.xInterval);
    m_yInterval->setSpecialValueText(tr("自動"));
    m_xInterval->setSpecialValueText(tr("自動"));
    m_fontSize = spinBox(this, "evalFontSize", S::kMinFontSize, S::kMaxFontSize, current.labelFontSize);
    m_fontSize->setSuffix(QStringLiteral(" pt"));
    form->addRow(tr("表示範囲:"), m_automatic);
    form->addRow(tr("評価値の範囲（手動）:"), m_yLimit);
    form->addRow(tr("手数の上限（手動）:"), m_xLimit);
    form->addRow(tr("評価値の目盛り間隔:"), m_yInterval);
    form->addRow(tr("手数の目盛り間隔:"), m_xInterval);
    form->addRow(tr("文字サイズ:"), m_fontSize);
    layout->addLayout(form);
    auto* note = new QLabel(tr("目盛りは文字が重ならないように間引きます。詰みはグラフの端に表示します。"), this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addWidget(new QLabel(tr("現在の目盛り間隔：評価値 %1 ／ 手数 %2").arg(axes.yInterval).arg(axes.xInterval), this));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    DialogFontScale::Options fontOptions;
    fontOptions.size = &AnalysisSettings::evalChartSettingsSize;
    DialogFontScale::install(this, AppSettings::evaluationChartDialogFontSize, fontOptions);
}

EvaluationChartSettingsDialog::~EvaluationChartSettingsDialog() = default;

EvaluationChartSettings EvaluationChartSettingsDialog::settings() const
{
    EvaluationChartSettings result;
    result.automatic = m_automatic->currentIndex() == 0;
    result.yLimit = m_yLimit->value();
    result.xLimit = m_xLimit->value();
    result.yInterval = m_yInterval->value();
    result.xInterval = m_xInterval->value();
    result.labelFontSize = m_fontSize->value();
    return result.normalized();
}
