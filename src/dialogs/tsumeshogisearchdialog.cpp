/// @file tsumeshogisearchdialog.cpp
/// @brief 詰み探索専用の条件設定ダイアログの実装

#include "tsumeshogisearchdialog.h"
#include "ui_tsumeshogisearchdialog.h"
#include "analysissettings.h"
#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "dialogutils.h"

#include <QAbstractItemView>
#include <QTimer>

TsumeShogiSearchDialog::TsumeShogiSearchDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::TsumeShogiSearchDialog>())
    , m_engines(EngineListSettings::loadEngines())
    , m_fontHelper({AnalysisSettings::tsumeSearchFontSize() > 0
                        ? AnalysisSettings::tsumeSearchFontSize() : qMax(10, font().pointSize()),
                    8, 24, 1, AnalysisSettings::setTsumeSearchFontSize})
{
    ui->setupUi(this);
    auto* start = ui->buttonBox->button(QDialogButtonBox::Ok);
    start->setText(tr("探索開始"));
    start->setStyleSheet(ButtonStyles::primaryAction());
    start->setDefault(true);
    ui->engineSetting->setAutoDefault(false);
    ui->toolButtonFontDecrease->setAutoDefault(false);
    ui->toolButtonFontIncrease->setAutoDefault(false);
    ui->toolButtonFontDecrease->setStyleSheet(ButtonStyles::panelToolButton());
    ui->toolButtonFontIncrease->setStyleSheet(ButtonStyles::panelToolButton());
    ui->searchDescription->setStyleSheet(QStringLiteral(
        "background: #e7f0f8; color: #243b53; border-radius: 4px; padding: 10px;"));
    ui->comboBoxEngine1->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->comboBoxEngine1->setMinimumContentsLength(18);

    const QString savedPath = AnalysisSettings::tsumeSearchEnginePath();
    for (const auto& engine : std::as_const(m_engines)) {
        ui->comboBoxEngine1->addItem(engine.name, engine.path);
        const int index = ui->comboBoxEngine1->count() - 1;
        ui->comboBoxEngine1->setItemData(index, engine.name, Qt::ToolTipRole);
        if (engine.path == savedPath) ui->comboBoxEngine1->setCurrentIndex(index);
    }
    ui->byoyomiSec->setValue(AnalysisSettings::tsumeSearchTimeLimitSec());
    const bool unlimited = AnalysisSettings::tsumeSearchUnlimitedTime();
    ui->unlimitedTimeRadioButton->setChecked(unlimited);
    ui->considerationTimeRadioButton->setChecked(!unlimited);
    ui->byoyomiSec->setEnabled(!unlimited);

    connect(ui->comboBoxEngine1, &QComboBox::currentIndexChanged,
            this, &TsumeShogiSearchDialog::updateEngineSelection);
    connect(ui->engineSetting, &QPushButton::clicked,
            this, &TsumeShogiSearchDialog::showEngineSettingsDialog);
    connect(ui->considerationTimeRadioButton, &QRadioButton::toggled,
            ui->byoyomiSec, &QSpinBox::setEnabled);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &TsumeShogiSearchDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &TsumeShogiSearchDialog::reject);
    connect(ui->toolButtonFontIncrease, &QPushButton::clicked,
            this, &TsumeShogiSearchDialog::onFontIncrease);
    connect(ui->toolButtonFontDecrease, &QPushButton::clicked,
            this, &TsumeShogiSearchDialog::onFontDecrease);

    updateEngineSelection();
    applyFontSize();
    DialogUtils::restoreDialogSize(this, AnalysisSettings::tsumeSearchDialogSize());
}

TsumeShogiSearchDialog::~TsumeShogiSearchDialog() = default;

const QList<EngineListSettings::EngineEntry>& TsumeShogiSearchDialog::engineList() const
{
    return m_engines;
}

int TsumeShogiSearchDialog::engineNumber() const
{
    return ui->comboBoxEngine1->currentIndex();
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
    const int index = engineNumber();
    return index >= 0 && index < m_engines.size()
        && !m_engines.at(index).name.isEmpty() && !m_engines.at(index).path.isEmpty();
}

void TsumeShogiSearchDialog::updateEngineSelection()
{
    const bool valid = hasValidEngine();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
    ui->engineSetting->setEnabled(valid);
    ui->comboBoxEngine1->setEnabled(!m_engines.isEmpty());
    ui->comboBoxEngine1->setToolTip(ui->comboBoxEngine1->currentText());
    ui->engineHint->setText(valid
        ? tr("詰み探索に対応したエンジンを選択してください。")
        : tr("使用できるエンジンがありません。「設定」→「エンジン設定」で登録してください。"));
}

void TsumeShogiSearchDialog::showEngineSettingsDialog()
{
    if (!hasValidEngine()) return;
    ChangeEngineSettingsDialog dialog(this);
    dialog.setEngineName(m_engines.at(engineNumber()).name);
    dialog.setupEngineOptionsDialog();
    dialog.exec();
}

void TsumeShogiSearchDialog::accept()
{
    if (!hasValidEngine()) return;
    ui->byoyomiSec->interpretText();
    AnalysisSettings::setTsumeSearchEnginePath(m_engines.at(engineNumber()).path);
    AnalysisSettings::setTsumeSearchUnlimitedTime(unlimitedTimeFlag());
    AnalysisSettings::setTsumeSearchTimeLimitSec(byoyomiSec());
    QDialog::accept();
}

void TsumeShogiSearchDialog::done(int result)
{
    DialogUtils::saveDialogSize(this, AnalysisSettings::setTsumeSearchDialogSize);
    QDialog::done(result);
}

void TsumeShogiSearchDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void TsumeShogiSearchDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateMinimumSize();
}

void TsumeShogiSearchDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void TsumeShogiSearchDialog::applyFontSize()
{
    const int size = m_fontHelper.fontSize();
    QFont dialogFont = font();
    dialogFont.setPointSize(size);
    DialogUtils::applyFontToAllChildren(this, dialogFont);
    ui->comboBoxEngine1->view()->setFont(dialogFont);
    ui->toolButtonFontDecrease->setEnabled(size > 8);
    ui->toolButtonFontIncrease->setEnabled(size < 24);
    const int buttonWidth = qMax(40, fontMetrics().horizontalAdvance(QStringLiteral("A+")) + 20);
    ui->toolButtonFontDecrease->setFixedWidth(buttonWidth);
    ui->toolButtonFontIncrease->setFixedWidth(buttonWidth);
    ui->mainLayout->invalidate();
    QTimer::singleShot(0, this, &TsumeShogiSearchDialog::updateMinimumSize);
}

void TsumeShogiSearchDialog::updateMinimumSize()
{
    // 折り返す説明文も含め、文字拡大時に下部の操作ボタンを隠さない。
    ui->mainLayout->activate();
    const int width = qMax(this->width(), minimumSizeHint().width());
    const int height = qMax(minimumSizeHint().height(), ui->mainLayout->totalHeightForWidth(width));
    setMinimumHeight(height);
    resize(width, qMax(this->height(), height));
}
