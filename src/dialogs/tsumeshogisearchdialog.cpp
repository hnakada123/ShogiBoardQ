/// @file tsumeshogisearchdialog.cpp
/// @brief 詰み探索専用の条件設定ダイアログの実装

#include "tsumeshogisearchdialog.h"
#include "dialogfontscale.h"
#include "engineselector.h"
#include "ui_tsumeshogisearchdialog.h"
#include "analysissettings.h"
#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"

#include <QAbstractItemView>

TsumeShogiSearchDialog::TsumeShogiSearchDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::TsumeShogiSearchDialog>())
{
    ui->setupUi(this);
    auto* start = ui->buttonBox->button(QDialogButtonBox::Ok);
    start->setText(tr("探索開始"));
    start->setStyleSheet(ButtonStyles::primaryAction());
    start->setDefault(true);
    ui->engineSetting->setAutoDefault(false);
    ui->searchDescription->setStyleSheet(QStringLiteral(
        "background: #e7f0f8; color: #243b53; border-radius: 4px; padding: 10px;"));
    ui->comboBoxEngine1->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->comboBoxEngine1->setMinimumContentsLength(18);

    // エンジンの一覧と前回選んだエンジン（案内文は選べるかで変えるのでこのダイアログが出す）
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::tsumeSearchSelection();
    m_engineSelector = new EngineSelector({ui->comboBoxEngine1, ui->engineSetting, nullptr}, engineOptions, this);
    ui->byoyomiSec->setValue(AnalysisSettings::tsumeSearchTimeLimitSec());
    const bool unlimited = AnalysisSettings::tsumeSearchUnlimitedTime();
    ui->unlimitedTimeRadioButton->setChecked(unlimited);
    ui->considerationTimeRadioButton->setChecked(!unlimited);
    ui->byoyomiSec->setEnabled(!unlimited);

    connect(m_engineSelector, &EngineSelector::selectionChanged, this, &TsumeShogiSearchDialog::updateEngineSelection);
    connect(m_engineSelector, &EngineSelector::settingsRequested,
            this, &TsumeShogiSearchDialog::showEngineSettingsDialog);
    connect(ui->considerationTimeRadioButton, &QRadioButton::toggled,
            ui->byoyomiSec, &QSpinBox::setEnabled);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &TsumeShogiSearchDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &TsumeShogiSearchDialog::reject);
    updateEngineSelection();
    // 文字の大きさ（A-/A+ は .ui に置いたボタン）。折り返す説明が欠けない大きさも DialogFontScale が保つ
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->toolButtonFontDecrease;
    fontOptions.increase = ui->toolButtonFontIncrease;
    fontOptions.size = &AnalysisSettings::tsumeSearchDialogSize;
    DialogFontScale::install(this, AnalysisSettings::tsumeSearchFontSize, fontOptions);
}

TsumeShogiSearchDialog::~TsumeShogiSearchDialog() = default;

std::optional<EngineListSettings::EngineEntry> TsumeShogiSearchDialog::engine() const
{
    return m_engineSelector->selected();
}

int TsumeShogiSearchDialog::byoyomiSec() const
{
    return ui->byoyomiSec->value();
}

bool TsumeShogiSearchDialog::unlimitedTimeFlag() const
{
    return ui->unlimitedTimeRadioButton->isChecked();
}

bool TsumeShogiSearchDialog::hasValidEngine() const
{
    const std::optional<EngineListSettings::EngineEntry> selected = engine();
    return selected && !selected->name.isEmpty() && !selected->path.isEmpty();
}

void TsumeShogiSearchDialog::updateEngineSelection()
{
    const bool valid = hasValidEngine();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
    ui->engineHint->setText(valid
        ? tr("詰み探索に対応したエンジンを選択してください。")
        : tr("使用できるエンジンがありません。「設定」→「エンジン設定」で登録してください。"));
}

void TsumeShogiSearchDialog::showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine)
{
    ChangeEngineSettingsDialog dialog(engine, this);
    dialog.exec();
}

void TsumeShogiSearchDialog::accept()
{
    if (!hasValidEngine()) return;
    ui->byoyomiSec->interpretText();
    m_engineSelector->saveSelection();
    AnalysisSettings::tsumeSearchUnlimitedTime.set(unlimitedTimeFlag());
    AnalysisSettings::tsumeSearchTimeLimitSec.set(byoyomiSec());
    QDialog::accept();
}

