/// @file kifuanalysisdialog.cpp
/// @brief 棋譜解析ダイアログクラスの実装

#include "kifuanalysisdialog.h"
#include "dialogfontscale.h"
#include "changeenginesettingsdialog.h"
#include "ui_kifuanalysisdialog.h"
#include "analysissettings.h"
#include "enginelistsettings.h"
#include "engineselector.h"
#include "buttonstyles.h"

#include <QFile>
#include <QComboBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QTextStream>
#include <qmessagebox.h>

// 棋譜解析ダイアログのUIを設定する。
KifuAnalysisDialog::KifuAnalysisDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::KifuAnalysisDialog>())
{
    // UIをセットアップする。
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("解析開始"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(ButtonStyles::primaryAction());
    applyRangeLabels();
    ui->spinBoxStartPly->setAccessibleName(tr("解析開始手数"));
    ui->spinBoxEndPly->setAccessibleName(tr("解析終了手数"));
    ui->label->setBuddy(ui->byoyomiSec);
    ui->comboBoxEngine1->setAccessibleName(tr("解析エンジン"));
    ui->comboBoxEngine1->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->comboBoxEngine1->setMinimumContentsLength(16);
    ui->analysisSummary->setStyleSheet(QStringLiteral(
        "background: #e7f0f8; color: #243b53; border-radius: 4px; padding: 10px;"));

    // セクション見出しは太字（文字の大きさは DialogFontScale が本文と一緒に変える）
    for (QLabel* label : {ui->labelSectionEngine, ui->labelSectionRange, ui->labelSectionTime}) {
        QFont bold = label->font();
        bold.setBold(true);
        label->setFont(bold);
    }

    // エンジンの一覧と前回選んだエンジン
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::kifuAnalysisSelection();
    m_engineSelector = new EngineSelector({ui->comboBoxEngine1, ui->engineSetting, ui->engineHint}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested, this, &KifuAnalysisDialog::showEngineSettingsDialog);
    
    // 設定から前回の思考時間を復元
    int savedByoyomi = AnalysisSettings::kifuAnalysisByoyomiSec();
    if (savedByoyomi > 0) {
        ui->byoyomiSec->setValue(savedByoyomi);
    }
    
    // 設定から解析範囲を復元
    bool savedFullRange = AnalysisSettings::kifuAnalysisFullRange();
    if (savedFullRange) {
        ui->radioButtonInitPosition->setChecked(true);
    } else {
        ui->radioButtonRangePosition->setChecked(true);
    }
    
    // 設定から開始・終了手数を復元（setMaxPlyで上書きされる可能性があるが初期値として設定）
    m_savedStartPly = AnalysisSettings::kifuAnalysisStartPly();
    m_savedEndPly = AnalysisSettings::kifuAnalysisEndPly();

    // エンジン設定ボタンが押されたときの処理

    // 条件を検証・保存してからダイアログを閉じる。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::processEngineSettings);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &KifuAnalysisDialog::reject);
    
    // 範囲指定ラジオボタンのトグル処理
    connect(ui->radioButtonRangePosition, &QRadioButton::toggled, this, &KifuAnalysisDialog::onRangeRadioToggled);
    
    // 開始手数が変更された場合、終了手数の最小値を更新
    connect(ui->spinBoxStartPly, QOverload<int>::of(&QSpinBox::valueChanged), this, &KifuAnalysisDialog::onStartPlyChanged);
    connect(ui->spinBoxEndPly, &QSpinBox::valueChanged, this, &KifuAnalysisDialog::updateSummary);
    connect(ui->byoyomiSec, &QSpinBox::valueChanged, this, &KifuAnalysisDialog::updateSummary);
    
    // 範囲指定のスピンボックスの有効/無効を設定
    bool rangeEnabled = ui->radioButtonRangePosition->isChecked();
    ui->spinBoxStartPly->setEnabled(rangeEnabled);
    ui->spinBoxEndPly->setEnabled(rangeEnabled);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_engineSelector->hasEngines());
    updateSummary();

    // 文字の大きさ（A-/A+ は .ui に置いたボタン）。折り返す説明が欠けない大きさも DialogFontScale が保つ
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->btnFontDecrease;
    fontOptions.increase = ui->btnFontIncrease;
    fontOptions.size = &AnalysisSettings::kifuAnalysisDialogSize;
    DialogFontScale::install(this, AnalysisSettings::kifuAnalysisFontSize, fontOptions);
}

KifuAnalysisDialog::~KifuAnalysisDialog() = default;


void KifuAnalysisDialog::updateSummary()
{
    const int count = ui->radioButtonInitPosition->isChecked()
        ? m_maxPly + 1 : ui->spinBoxEndPly->value() - ui->spinBoxStartPly->value() + 1;
    const qint64 seconds = static_cast<qint64>(count) * ui->byoyomiSec->value();
    QString duration;
    if (seconds < 60) duration = tr("約%1秒").arg(seconds);
    else if (seconds < 3600) duration = tr("約%1分%2秒").arg(seconds / 60).arg(seconds % 60);
    else duration = tr("約%1時間%2分").arg(seconds / 3600).arg((seconds % 3600) / 60);
    ui->analysisSummary->setText(tr("解析対象: %1局面　所要時間の目安: %2").arg(count).arg(duration));
    ui->analysisSummary->setToolTip(tr("エンジンの起動時間などにより、実際の所要時間は前後します。"));
}

// 範囲指定ラジオボタンが選択された場合
void KifuAnalysisDialog::onRangeRadioToggled(bool checked)
{
    ui->spinBoxStartPly->setEnabled(checked);
    ui->spinBoxEndPly->setEnabled(checked);
    updateSummary();
}

// 開始手数が変更された場合
void KifuAnalysisDialog::onStartPlyChanged(int value)
{
    // 終了手数は開始手数以上でなければならない
    ui->spinBoxEndPly->setMinimum(value);
    if (ui->spinBoxEndPly->value() < value) {
        ui->spinBoxEndPly->setValue(value);
    }
    updateSummary();
}

// 最大手数を設定する
void KifuAnalysisDialog::setMaxPly(int maxPly)
{
    maxPly = qMax(0, maxPly);
    m_maxPly = maxPly;
    
    // スピンボックスの最大値を設定
    ui->spinBoxStartPly->setMaximum(maxPly);
    ui->spinBoxEndPly->setMaximum(maxPly);
    
    // 保存された値を復元（最大値の範囲内に収める）
    int startVal = qBound(0, m_savedStartPly, maxPly);
    int endVal = qBound(startVal, m_savedEndPly, maxPly);
    
    // 未設定の場合は最終局面までを初期値とする（0は開始局面のみ）。
    if (m_savedEndPly < 0) {
        endVal = maxPly;
    }
    
    ui->spinBoxStartPly->setValue(startVal);
    ui->spinBoxEndPly->setValue(endVal);
    updateSummary();
}

void KifuAnalysisDialog::showEngineSettingsDialog(const EngineEntry& engine)
{
    ChangeEngineSettingsDialog::showFor(engine, this);
}

// OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
void KifuAnalysisDialog::processEngineSettings()
{
    if (!m_engineSelector->selected()) return;

    // 範囲（「開始局面から最終手まで」か範囲指定）と1局面の思考時間
    const bool fullRange = !ui->radioButtonRangePosition->isChecked();
    m_options.startPly = fullRange ? 0 : qBound(0, ui->spinBoxStartPly->value(), m_maxPly);
    m_options.endPly = fullRange ? m_maxPly : qBound(m_options.startPly, ui->spinBoxEndPly->value(), m_maxPly);
    m_options.movetimeMs = ui->byoyomiSec->value() * 1000;

    // 設定を保存
    m_engineSelector->saveSelection();
    AnalysisSettings::kifuAnalysisByoyomiSec.set(ui->byoyomiSec->value());
    AnalysisSettings::kifuAnalysisFullRange.set(fullRange);
    AnalysisSettings::kifuAnalysisStartPly.set(ui->spinBoxStartPly->value());
    AnalysisSettings::kifuAnalysisEndPly.set(ui->spinBoxEndPly->value());
    accept();
}

std::optional<KifuAnalysisDialog::Engine> KifuAnalysisDialog::engine() const
{
    return m_engineSelector->selected();
}

// 範囲指定の数値欄の前後に置く文字列を設定する。
// 語順は言語ごとに異なるため、1つの文を %1・%2 の位置で分けて数値欄の前・間・後ろに置く。
void KifuAnalysisDialog::applyRangeLabels()
{
    QString pattern = tr("%1手目から%2手目まで");
    qsizetype first = pattern.indexOf(QStringLiteral("%1"));
    qsizetype second = pattern.indexOf(QStringLiteral("%2"));
    if (first < 0 || second < first) {
        pattern = QStringLiteral("%1手目から%2手目まで");
        first = pattern.indexOf(QStringLiteral("%1"));
        second = pattern.indexOf(QStringLiteral("%2"));
    }
    ui->labelRangePrefix->setText(pattern.left(first).trimmed());
    ui->labelFrom->setText(pattern.mid(first + 2, second - first - 2).trimmed());
    ui->labelTo->setText(pattern.mid(second + 2).trimmed());
    ui->labelRangePrefix->setVisible(!ui->labelRangePrefix->text().isEmpty());
    ui->labelTo->setVisible(!ui->labelTo->text().isEmpty());
}
