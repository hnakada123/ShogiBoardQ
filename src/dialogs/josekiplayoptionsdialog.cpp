/// @file josekiplayoptionsdialog.cpp
/// @brief 対局でエンジンが定跡を使う条件のダイアログの実装

#include "josekiplayoptionsdialog.h"

#include "dialogfontscale.h"
#include "josekisettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

JosekiPlayOptionsDialog::JosekiPlayOptionsDialog(const JosekiPlayOptions &options, const QString &title,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_enabled(options.enabled)
{
    setWindowTitle(title);
    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    mainLayout->addLayout(form);

    m_maxPly = new QSpinBox(this);
    m_maxPly->setObjectName(QStringLiteral("josekiMaxPly"));
    m_maxPly->setRange(1, 9999);
    m_maxPly->setSuffix(tr(" 手目まで"));
    m_maxPly->setToolTip(tr("この手数までの手に定跡を使います（やねうら王の BookMoves）"));
    form->addRow(tr("定跡を使う手数:"), m_maxPly);

    m_method = new QComboBox(this);
    m_method->setObjectName(QStringLiteral("josekiMethod"));
    m_method->addItem(tr("出現頻度に比例して選ぶ"), JosekiPlayOptions::ByFrequency);
    m_method->addItem(tr("評価値で選ぶ（最善との差の範囲から同じ確率で）"), JosekiPlayOptions::ByValue);
    m_method->addItem(tr("表の先頭の手"), JosekiPlayOptions::FirstMove);
    m_method->setToolTip(tr("条件に合う定跡手が複数あるときの選び方"
                            "（やねうら王の ConsiderBookMoveCount・BookEvalDiff）"));
    form->addRow(tr("選び方:"), m_method);

    m_valueDiff = new QSpinBox(this);
    m_valueDiff->setObjectName(QStringLiteral("josekiValueDiff"));
    m_valueDiff->setRange(0, 99999);
    m_valueDiff->setToolTip(tr("「評価値で選ぶ」で、最善の評価値からこの差までの手を選びます（やねうら王の BookEvalDiff）"));
    form->addRow(tr("評価値の差:"), m_valueDiff);

    m_useMinValue = new QCheckBox(tr("使う"), this);
    m_useMinValue->setObjectName(QStringLiteral("josekiUseMinValue"));
    m_minValue = new QSpinBox(this);
    m_minValue->setObjectName(QStringLiteral("josekiMinValue"));
    m_minValue->setRange(-99999, 99999);
    m_minValue->setToolTip(tr("指す側から見た評価値がこれより低い定跡手は使いません"
                              "（やねうら王の BookEvalBlackLimit・BookEvalWhiteLimit）"));
    auto *minValueRow = new QHBoxLayout;
    minValueRow->addWidget(m_useMinValue);
    minValueRow->addWidget(m_minValue, 1);
    form->addRow(tr("評価値の下限:"), minValueRow);

    m_minDepth = new QSpinBox(this);
    m_minDepth->setObjectName(QStringLiteral("josekiMinDepth"));
    m_minDepth->setRange(0, 9999);
    m_minDepth->setToolTip(tr("深さがこれより浅い定跡手は使いません。棋譜から登録した手は深さ0です"
                              "（やねうら王の BookDepthLimit）"));
    form->addRow(tr("深さの下限:"), m_minDepth);

    m_minRatio = new QDoubleSpinBox(this);
    m_minRatio->setObjectName(QStringLiteral("josekiMinRatio"));
    m_minRatio->setRange(0.0, 100.0);
    m_minRatio->setDecimals(1);
    m_minRatio->setSuffix(QStringLiteral(" %"));
    m_minRatio->setToolTip(tr("その局面の出現頻度の合計に対する割合（定跡ウィンドウの「比率」）がこれより低い手は使いません"));
    form->addRow(tr("比率の下限:"), m_minRatio);

    m_minFrequency = new QSpinBox(this);
    m_minFrequency->setObjectName(QStringLiteral("josekiMinFrequency"));
    m_minFrequency->setRange(0, 999999999);
    m_minFrequency->setToolTip(tr("出現頻度がこれより少ない定跡手は使いません"));
    form->addRow(tr("出現頻度の下限:"), m_minFrequency);

    auto *note = new QLabel(tr("※ 定跡は定跡ウィンドウで開いている定跡を使います。定跡を抜けた後は、エンジンに内蔵の定跡が"
                               "使われることがあります（使わせないときは、エンジン設定で定跡を使わない設定にします。"
                               "やねうら王なら BookFile を no_book にします）。"),
                            this);
    note->setWordWrap(true);
    mainLayout->addWidget(note);
    mainLayout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
                                         this);
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("既定値に戻す"));
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &JosekiPlayOptionsDialog::resetToDefaults);
    connect(m_method, &QComboBox::currentIndexChanged, this, &JosekiPlayOptionsDialog::updateEnabledFields);
    connect(m_useMinValue, &QCheckBox::toggled, this, &JosekiPlayOptionsDialog::updateEnabledFields);

    setOptions(options);
    DialogFontScale::Options fontOptions;
    fontOptions.size = &JosekiSettings::josekiPlayOptionsDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiPlayOptionsDialogFontSize, fontOptions);
}

JosekiPlayOptionsDialog::~JosekiPlayOptionsDialog() = default;

void JosekiPlayOptionsDialog::setOptions(const JosekiPlayOptions &options)
{
    const JosekiPlayOptions normalized = options.normalized();
    m_maxPly->setValue(normalized.maxPly);
    m_method->setCurrentIndex(qMax(0, m_method->findData(normalized.method)));
    m_valueDiff->setValue(normalized.valueDiff);
    m_useMinValue->setChecked(normalized.useMinValue);
    m_minValue->setValue(normalized.minValue);
    m_minDepth->setValue(normalized.minDepth);
    m_minRatio->setValue(normalized.minRatioPercent);
    m_minFrequency->setValue(normalized.minFrequency);
    updateEnabledFields();
}

JosekiPlayOptions JosekiPlayOptionsDialog::options() const
{
    JosekiPlayOptions options;
    options.enabled = m_enabled;
    options.maxPly = m_maxPly->value();
    options.method = m_method->currentData().toInt();
    options.valueDiff = m_valueDiff->value();
    options.useMinValue = m_useMinValue->isChecked();
    options.minValue = m_minValue->value();
    options.minDepth = m_minDepth->value();
    options.minRatioPercent = m_minRatio->value();
    options.minFrequency = m_minFrequency->value();
    return options.normalized();
}

void JosekiPlayOptionsDialog::updateEnabledFields()
{
    m_valueDiff->setEnabled(m_method->currentData().toInt() == JosekiPlayOptions::ByValue);
    m_minValue->setEnabled(m_useMinValue->isChecked());
}

void JosekiPlayOptionsDialog::resetToDefaults()
{
    JosekiPlayOptions defaults;
    defaults.enabled = m_enabled;
    setOptions(defaults);
}
