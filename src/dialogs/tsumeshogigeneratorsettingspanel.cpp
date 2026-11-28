/// @file tsumeshogigeneratorsettingspanel.cpp
/// @brief 詰将棋局面生成ダイアログの、エンジンと生成設定の欄

#include "tsumeshogigeneratorsettingspanel.h"

#include "changeenginesettingsdialog.h"
#include "countspinbox.h"
#include "engineselector.h"
#include "tsumeshogisettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
/// 偶数の直接入力は確定させず、前の値に戻す。
class OddSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    QValidator::State validate(QString& input, int& pos) const override
    {
        const QValidator::State state = QSpinBox::validate(input, pos);
        if (state != QValidator::Acceptable) return state;
        return TsumeshogiSettings::GeneratorLimits::isTargetMoves(valueFromText(input)) ? QValidator::Acceptable
                                                                                        : QValidator::Intermediate;
    }
};
} // namespace

TsumeshogiGeneratorSettingsPanel::TsumeshogiGeneratorSettingsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    buildEngineGroup(layout);
    buildConditionsGroup(layout);
    connectSignals();

    // エンジンの一覧と前回選んだエンジン
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineSelectionSettings::tsumeshogiGeneratorSelection();
    m_engineSelector = new EngineSelector({m_ui.engine, m_ui.engineSettings, nullptr}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested,
            this, &TsumeshogiGeneratorSettingsPanel::showEngineSettingsDialog);
    bindSettings();
}

// ============================================================
// 欄の組み立て
// ============================================================

void TsumeshogiGeneratorSettingsPanel::buildEngineGroup(QVBoxLayout* mainLayout)
{
    auto* engineGroup = new QGroupBox(tr("エンジン設定"), this);
    auto* engineLayout = new QVBoxLayout(engineGroup);
    auto* engineRow = new QHBoxLayout;
    auto* engineLabel = new QLabel(tr("エンジン:"), this);
    engineRow->addWidget(engineLabel);
    m_ui.engine = new QComboBox(this);
    m_ui.engine->setObjectName(QStringLiteral("generatorEngine"));
    m_ui.engine->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_ui.engine->setMinimumContentsLength(20);
    engineLabel->setBuddy(m_ui.engine);
    engineRow->addWidget(m_ui.engine, 1);
    m_ui.engineSettings = new QPushButton(tr("設定..."), this);
    engineRow->addWidget(m_ui.engineSettings);
    engineLayout->addLayout(engineRow);
    auto* engineNote = new QLabel(tr("最短手順を返す詰将棋エンジンを使用してください。"), this);
    engineNote->setWordWrap(true);
    auto* hintRow = new QHBoxLayout;
    hintRow->addWidget(engineNote, 1);
    m_ui.help = new QToolButton(this);
    m_ui.help->setObjectName(QStringLiteral("generatorHelp"));
    m_ui.help->setText(tr("エンジン設定・採択条件について"));
    m_ui.help->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_ui.help->setCheckable(true);
    m_ui.help->setArrowType(Qt::RightArrow);
    hintRow->addWidget(m_ui.help);
    engineLayout->addLayout(hintRow);
    m_ui.helpText = new QLabel(
        tr("※ エンジンは最短手順を返す設定で使用してください（KomoringHeights の場合: PostSearchLevel = MinLength）")
        + QStringLiteral("\n")
        + tr("主手順の攻手が一意な局面だけを出力します（成・不成も区別）。玉方が早く詰む変化での別の詰め方は許容し、判定不能の局面は出力しません。")
        + QStringLiteral("\n")
        + tr("「詰み局面から逆算・編集」方式では詰み形または種局面から逆算・変異・余詰修復で長手数局面を育て、全1枚除去の厳密証明と途中保存・再開に対応します。"), this);
    m_ui.helpText->setWordWrap(true);
    m_ui.helpText->hide();
    engineLayout->addWidget(m_ui.helpText);
    mainLayout->addWidget(engineGroup);
}

void TsumeshogiGeneratorSettingsPanel::buildConditionsGroup(QVBoxLayout* mainLayout)
{
    auto* settingsGroup = new QGroupBox(tr("生成設定"), this);
    auto* settingsLayout = new QVBoxLayout(settingsGroup);
    settingsLayout->setSpacing(4);

    auto* modeRow = new QHBoxLayout;
    auto* modeLabel = new QLabel(tr("生成方式:"), this);
    m_ui.mode = new QComboBox(this);
    m_ui.mode->setObjectName(QStringLiteral("generatorMode"));
    m_ui.mode->addItem(tr("ランダム配置"));
    m_ui.mode->addItem(tr("詰み局面から逆算・編集（長手数向け）"));
    m_ui.mode->setToolTip(
        tr("短手数向けは「ランダム配置」、15手以上の長手数詰将棋や途中保存・再開には「詰み局面から逆算・編集」を選んでください。"));
    modeLabel->setBuddy(m_ui.mode);
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(m_ui.mode, 1);
    settingsLayout->addLayout(modeRow);

    auto* columns = new QHBoxLayout;
    columns->setSpacing(24);
    m_ui.leftForm = new QFormLayout;
    m_ui.rightForm = new QFormLayout;
    m_ui.leftForm->setVerticalSpacing(6);
    m_ui.rightForm->setVerticalSpacing(6);
    m_ui.leftForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_ui.rightForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    columns->addLayout(m_ui.leftForm, 1);
    columns->addLayout(m_ui.rightForm, 1);
    settingsLayout->addLayout(columns);

    buildConditionRows();

    auto* options = new QHBoxLayout;
    m_ui.allowFinalAlternatives = new QCheckBox(tr("最終手の複数解を許容する"), this);
    m_ui.allowFinalAlternatives->setObjectName(QStringLiteral("generatorAllowFinalAlternatives"));
    m_ui.allowFinalAlternatives->setToolTip(
        tr("オンにすると、主手順の最終手に複数の詰手があっても採択します（1手詰の初手は除く）。オフにすると最終手も一意な局面だけを出力します。"));
    options->addWidget(m_ui.allowFinalAlternatives);
    options->addStretch();
    m_ui.restoreDefaults = new QPushButton(tr("既定値に戻す"), this);
    m_ui.restoreDefaults->setObjectName(QStringLiteral("generatorRestoreDefaults"));
    m_ui.restoreDefaults->setToolTip(tr("生成設定を既定値に戻します。"));
    options->addWidget(m_ui.restoreDefaults);
    settingsLayout->addLayout(options);

    settingsLayout->addWidget(buildRetrogradeExtras());

    mainLayout->addWidget(settingsGroup);
}

void TsumeshogiGeneratorSettingsPanel::buildConditionRows()
{
    m_ui.targetMoves = new OddSpinBox(this);
    m_ui.targetMoves->setObjectName(QStringLiteral("generatorTargetMoves"));
    m_ui.targetMoves->setRange(TsumeshogiSettings::tsumeshogiGeneratorTargetMoves.minimum(),
                                       TsumeshogiSettings::tsumeshogiGeneratorTargetMoves.maximum());
    m_ui.targetMoves->setSingleStep(2);
    m_ui.targetMoves->setSuffix(tr(" 手詰"));
    m_ui.leftForm->addRow(tr("目標手数:"), m_ui.targetMoves);

    m_ui.maxAttack = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_ui.maxAttack->setRange(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces.minimum(),
                                     TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces.maximum());
    m_ui.maxAttack->setToolTip(tr("攻方の盤上の駒と持駒の合計枚数の上限です。"));
    m_ui.leftForm->addRow(tr("攻め駒上限:"), m_ui.maxAttack);

    m_ui.maxDefend = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_ui.maxDefend->setRange(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces.minimum(),
                                     TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces.maximum());
    m_ui.maxDefend->setToolTip(tr("玉を除く、玉方の盤上の駒の枚数の上限です。"));
    m_ui.leftForm->addRow(tr("守り駒上限:"), m_ui.maxDefend);

    m_ui.retroMaxAttack = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_ui.retroMaxAttack->setObjectName(QStringLiteral("generatorRetroMaxAttack"));
    m_ui.retroMaxAttack->setRange(TsumeshogiSettings::tsumeshogiGeneratorRetroMaxAttackPieces.minimum(),
                                          TsumeshogiSettings::tsumeshogiGeneratorRetroMaxAttackPieces.maximum());
    m_ui.retroMaxAttack->setToolTip(tr("逆算・編集モードでの攻方駒（盤上＋持駒）の上限です。"));
    m_ui.leftForm->addRow(tr("攻め駒上限:"), m_ui.retroMaxAttack);

    m_ui.retroMaxDefend = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_ui.retroMaxDefend->setObjectName(QStringLiteral("generatorRetroMaxDefend"));
    m_ui.retroMaxDefend->setRange(TsumeshogiSettings::tsumeshogiGeneratorRetroMaxDefendPieces.minimum(),
                                          TsumeshogiSettings::tsumeshogiGeneratorRetroMaxDefendPieces.maximum());
    m_ui.retroMaxDefend->setToolTip(tr("逆算・編集モードでの玉方盤上駒（玉除く）の上限です。"));
    m_ui.leftForm->addRow(tr("守り駒上限:"), m_ui.retroMaxDefend);

    m_ui.beamWidth = new QSpinBox(this);
    m_ui.beamWidth->setObjectName(QStringLiteral("generatorBeamWidth"));
    m_ui.beamWidth->setRange(TsumeshogiSettings::tsumeshogiGeneratorBeamWidth.minimum(),
                                     TsumeshogiSettings::tsumeshogiGeneratorBeamWidth.maximum());
    m_ui.beamWidth->setToolTip(tr("各手数層に保持する有望局面の最大数です。大きいほど多様性が増します。"));
    m_ui.leftForm->addRow(tr("ビーム幅:"), m_ui.beamWidth);

    m_ui.maxPositions = new CountSpinBox(tr(" 局面", "1 のとき"), tr(" 局面"), this);
    m_ui.maxPositions->setObjectName(QStringLiteral("generatorMaxPositions"));
    m_ui.maxPositions->setRange(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions.minimum(),
                                        TsumeshogiSettings::tsumeshogiGeneratorMaxPositions.maximum());
    m_ui.maxPositions->setSpecialValueText(tr("無制限"));
    m_ui.maxPositions->setToolTip(tr("採択した局面がこの件数に達すると終了します。0 は停止するまで生成を続けます。"));
    m_ui.rightForm->addRow(tr("生成上限:"), m_ui.maxPositions);

    m_ui.timeoutLabel = new QLabel(tr("探索時間/局面:"), this);
    m_ui.timeout = new QSpinBox(this);
    m_ui.timeout->setRange(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec.minimum(),
                                   TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec.maximum());
    m_ui.timeout->setSuffix(tr(" 秒"));
    m_ui.timeoutLabel->setBuddy(m_ui.timeout);
    m_ui.rightForm->addRow(m_ui.timeoutLabel, m_ui.timeout);

    m_ui.attackRange = new CountSpinBox(tr(" マス（玉中心）", "1 のとき"), tr(" マス（玉中心）"), this);
    m_ui.attackRange->setRange(TsumeshogiSettings::tsumeshogiGeneratorAttackRange.minimum(),
                                       TsumeshogiSettings::tsumeshogiGeneratorAttackRange.maximum());
    m_ui.rightForm->addRow(tr("配置範囲:"), m_ui.attackRange);

    m_ui.branchFactor = new QSpinBox(this);
    m_ui.branchFactor->setObjectName(QStringLiteral("generatorBranchFactor"));
    m_ui.branchFactor->setRange(TsumeshogiSettings::tsumeshogiGeneratorBranchFactor.minimum(),
                                        TsumeshogiSettings::tsumeshogiGeneratorBranchFactor.maximum());
    m_ui.branchFactor->setToolTip(tr("1回の展開で逆算・盤面変異から作る候補局面数です。"));
    m_ui.rightForm->addRow(tr("分岐数:"), m_ui.branchFactor);

    m_ui.verifyBudget = new QSpinBox(this);
    m_ui.verifyBudget->setObjectName(QStringLiteral("generatorVerifyBudget"));
    m_ui.verifyBudget->setRange(TsumeshogiSettings::tsumeshogiGeneratorVerifyBudgetSec.minimum(),
                                        TsumeshogiSettings::tsumeshogiGeneratorVerifyBudgetSec.maximum());
    m_ui.verifyBudget->setSuffix(tr(" 秒"));
    m_ui.verifyBudget->setToolTip(tr("1候補の厳密証明と全1枚除去トリミングに使える最大時間です。"));
    m_ui.rightForm->addRow(tr("検証予算:"), m_ui.verifyBudget);
}

QWidget* TsumeshogiGeneratorSettingsPanel::buildRetrogradeExtras()
{
    // 逆算・編集モード専用の追加条件・ファイル設定（駒余り禁止・類似作除外・種局面・途中保存チェックポイント）
    m_ui.retroFilesWidget = new QWidget(this);
    m_ui.retroFilesWidget->setObjectName(QStringLiteral("generatorRetroFiles"));
    auto* retroExtraLayout = new QVBoxLayout(m_ui.retroFilesWidget);
    retroExtraLayout->setContentsMargins(0, 2, 0, 0);
    retroExtraLayout->setSpacing(6);

    auto* retroOptions = new QHBoxLayout;
    m_ui.requireNoSurplus = new QCheckBox(tr("駒余りを禁止する"), this);
    m_ui.requireNoSurplus->setObjectName(QStringLiteral("generatorRequireNoSurplus"));
    m_ui.requireNoSurplus->setToolTip(
        tr("オンにすると、全最長応手および最終手で攻方持駒が0枚になる局面だけを採択します。"));
    retroOptions->addWidget(m_ui.requireNoSurplus);
    m_ui.excludeSimilar = new QCheckBox(tr("類似作を除外する"), this);
    m_ui.excludeSimilar->setObjectName(QStringLiteral("generatorExcludeSimilar"));
    m_ui.excludeSimilar->setToolTip(
        tr("オンにすると、平行移動・左右反転・中間手順1手違いの類似作を除外します。"));
    retroOptions->addWidget(m_ui.excludeSimilar);
    retroOptions->addStretch();
    retroExtraLayout->addLayout(retroOptions);

    auto* filesForm = new QFormLayout;
    filesForm->setContentsMargins(0, 0, 0, 0);
    filesForm->setVerticalSpacing(6);
    filesForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto* seedRow = new QHBoxLayout;
    m_ui.seedFile = new QLineEdit(this);
    m_ui.seedFile->setObjectName(QStringLiteral("generatorSeedFile"));
    m_ui.seedFile->setPlaceholderText(tr("未指定（0手詰み局面から自動生成）"));
    m_ui.browseSeed = new QPushButton(tr("参照..."), this);
    m_ui.browseSeed->setObjectName(QStringLiteral("generatorBrowseSeed"));
    m_ui.clearSeed = new QPushButton(tr("クリア"), this);
    m_ui.clearSeed->setObjectName(QStringLiteral("generatorClearSeed"));
    seedRow->addWidget(m_ui.seedFile, 1);
    seedRow->addWidget(m_ui.browseSeed);
    seedRow->addWidget(m_ui.clearSeed);
    filesForm->addRow(tr("種局面ファイル（任意）:"), seedRow);

    auto* excludeRow = new QHBoxLayout;
    m_ui.excludeFile = new QLineEdit(this);
    m_ui.excludeFile->setObjectName(QStringLiteral("generatorExcludeFile"));
    m_ui.excludeFile->setPlaceholderText(tr("未指定（既出の除外ファイルを使わない）"));
    m_ui.excludeFile->setToolTip(
        tr("既存の詰将棋問題集などを指定すると、収録済みの局面や類似手順を除外して新作を生成します。"));
    m_ui.browseExclude = new QPushButton(tr("参照..."), this);
    m_ui.browseExclude->setObjectName(QStringLiteral("generatorBrowseExclude"));
    m_ui.clearExclude = new QPushButton(tr("クリア"), this);
    m_ui.clearExclude->setObjectName(QStringLiteral("generatorClearExclude"));
    excludeRow->addWidget(m_ui.excludeFile, 1);
    excludeRow->addWidget(m_ui.browseExclude);
    excludeRow->addWidget(m_ui.clearExclude);
    filesForm->addRow(tr("除外ファイル（任意）:"), excludeRow);

    auto* cpRow = new QHBoxLayout;
    m_ui.checkpointFile = new QLineEdit(this);
    m_ui.checkpointFile->setObjectName(QStringLiteral("generatorCheckpointFile"));
    m_ui.checkpointFile->setPlaceholderText(tr("未指定（途中保存しない）"));
    m_ui.browseCheckpoint = new QPushButton(tr("参照..."), this);
    m_ui.browseCheckpoint->setObjectName(QStringLiteral("generatorBrowseCheckpoint"));
    m_ui.clearCheckpoint = new QPushButton(tr("クリア"), this);
    m_ui.clearCheckpoint->setObjectName(QStringLiteral("generatorClearCheckpoint"));
    m_ui.resumeCheckpoint = new QCheckBox(tr("途中保存から再開"), this);
    m_ui.resumeCheckpoint->setObjectName(QStringLiteral("generatorResumeCheckpoint"));
    cpRow->addWidget(m_ui.checkpointFile, 1);
    cpRow->addWidget(m_ui.browseCheckpoint);
    cpRow->addWidget(m_ui.clearCheckpoint);
    cpRow->addWidget(m_ui.resumeCheckpoint);
    filesForm->addRow(tr("チェックポイント（任意）:"), cpRow);
    retroExtraLayout->addLayout(filesForm);
    return m_ui.retroFilesWidget;
}

void TsumeshogiGeneratorSettingsPanel::connectSignals()
{
    connect(m_ui.restoreDefaults, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onRestoreDefaults);
    connect(m_ui.maxPositions, &QSpinBox::valueChanged, this, &TsumeshogiGeneratorSettingsPanel::maxPositionsChanged);
    connect(m_ui.help, &QToolButton::toggled, this, &TsumeshogiGeneratorSettingsPanel::toggleHelp);
    connect(m_ui.mode, &QComboBox::currentIndexChanged, this, &TsumeshogiGeneratorSettingsPanel::onModeChanged);
    connect(m_ui.browseSeed, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onBrowseSeedFile);
    connect(m_ui.clearSeed, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onClearSeedFile);
    connect(m_ui.browseExclude, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onBrowseExcludeFile);
    connect(m_ui.clearExclude, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onClearExcludeFile);
    connect(m_ui.browseCheckpoint, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onBrowseCheckpointFile);
    connect(m_ui.clearCheckpoint, &QPushButton::clicked, this, &TsumeshogiGeneratorSettingsPanel::onClearCheckpointFile);
    connect(m_ui.resumeCheckpoint, &QCheckBox::toggled, this, &TsumeshogiGeneratorSettingsPanel::onResumeCheckpointToggled);
}

// ============================================================
// 設定の保存と復元
// ============================================================

void TsumeshogiGeneratorSettingsPanel::bindSettings()
{
    // 生成の条件（「既定値に戻す」の対象）と、表示の設定
    m_conditionsForm.add(m_ui.mode, TsumeshogiSettings::tsumeshogiGeneratorMode);
    m_conditionsForm.add(m_ui.targetMoves, TsumeshogiSettings::tsumeshogiGeneratorTargetMoves);
    m_conditionsForm.add(m_ui.maxAttack, TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces);
    m_conditionsForm.add(m_ui.maxDefend, TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces);
    m_conditionsForm.add(m_ui.attackRange, TsumeshogiSettings::tsumeshogiGeneratorAttackRange);
    m_conditionsForm.add(m_ui.retroMaxAttack, TsumeshogiSettings::tsumeshogiGeneratorRetroMaxAttackPieces);
    m_conditionsForm.add(m_ui.retroMaxDefend, TsumeshogiSettings::tsumeshogiGeneratorRetroMaxDefendPieces);
    m_conditionsForm.add(m_ui.beamWidth, TsumeshogiSettings::tsumeshogiGeneratorBeamWidth);
    m_conditionsForm.add(m_ui.branchFactor, TsumeshogiSettings::tsumeshogiGeneratorBranchFactor);
    m_conditionsForm.add(m_ui.verifyBudget, TsumeshogiSettings::tsumeshogiGeneratorVerifyBudgetSec);
    m_conditionsForm.add(m_ui.timeout, TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec);
    m_conditionsForm.add(m_ui.maxPositions, TsumeshogiSettings::tsumeshogiGeneratorMaxPositions);
    m_conditionsForm.add(m_ui.allowFinalAlternatives,
                         TsumeshogiSettings::tsumeshogiGeneratorAllowFinalMoveAlternatives);
    m_conditionsForm.add(m_ui.requireNoSurplus, TsumeshogiSettings::tsumeshogiGeneratorRequireNoSurplus);
    m_conditionsForm.add(m_ui.excludeSimilar, TsumeshogiSettings::tsumeshogiGeneratorExcludeSimilar);
    m_conditionsForm.add(m_ui.seedFile, TsumeshogiSettings::tsumeshogiGeneratorSeedFile);
    m_conditionsForm.add(m_ui.excludeFile, TsumeshogiSettings::tsumeshogiGeneratorExcludeFile);
    m_conditionsForm.add(m_ui.checkpointFile, TsumeshogiSettings::tsumeshogiGeneratorCheckpointFile);
    m_conditionsForm.add(m_ui.resumeCheckpoint, TsumeshogiSettings::tsumeshogiGeneratorResumeCheckpoint);
    m_displayForm.add(m_ui.help, TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded);
}

void TsumeshogiGeneratorSettingsPanel::loadSettings()
{
    // 以前の版が保存した偶数の目標手数は、設定の側で奇数に直してから読む
    TsumeshogiSettings::upgradeLegacyGeneratorTargetMoves();
    m_conditionsForm.load();
    m_displayForm.load();
    applyModeUi();
}

void TsumeshogiGeneratorSettingsPanel::saveSettings()
{
    m_engineSelector->saveSelection();
    m_conditionsForm.save();
    m_displayForm.save();
}

// ============================================================
// 欄の値
// ============================================================

TsumeshogiGeneratorSettingsPanel::Mode TsumeshogiGeneratorSettingsPanel::mode() const
{
    return m_ui.mode->currentIndex() == static_cast<int>(Mode::Retrograde) ? Mode::Retrograde : Mode::Random;
}

bool TsumeshogiGeneratorSettingsPanel::hasEngines() const
{
    return m_engineSelector->hasEngines();
}

std::optional<EngineEntry> TsumeshogiGeneratorSettingsPanel::selectedEngine() const
{
    return m_engineSelector->selected();
}

int TsumeshogiGeneratorSettingsPanel::maxPositions() const
{
    return m_ui.maxPositions->value();
}

bool TsumeshogiGeneratorSettingsPanel::isExpanded() const
{
    return m_ui.help->isChecked() || mode() == Mode::Retrograde;
}

TsumeshogiGenerator::Settings TsumeshogiGeneratorSettingsPanel::randomSettings(const EngineEntry& engine) const
{
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = engine.path;
    settings.engineName = engine.name;
    settings.targetMoves = m_ui.targetMoves->value();
    settings.timeoutMs = m_ui.timeout->value() * 1000;
    settings.maxPositionsToFind = m_ui.maxPositions->value();
    settings.allowFinalMoveAlternatives = m_ui.allowFinalAlternatives->isChecked();
    settings.posGenSettings.maxAttackPieces = m_ui.maxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_ui.maxDefend->value();
    settings.posGenSettings.attackRange = m_ui.attackRange->value();
    return settings;
}

std::optional<TsumeRetrogradeGenerator::Settings> TsumeshogiGeneratorSettingsPanel::retrogradeSettings(
    const EngineEntry& engine, QString* errorMessage) const
{
    const QString checkpointPath = m_ui.checkpointFile->text().trimmed();
    const bool resume = m_ui.resumeCheckpoint->isChecked();
    if (resume && checkpointPath.isEmpty()) {
        *errorMessage = tr("途中保存から再開するにはチェックポイントファイルを指定してください。");
        return std::nullopt;
    }

    // 種局面は、途中保存から再開するときは使わない（探索の状態は途中保存から戻す）
    QStringList seedLines;
    const QString seedPath = m_ui.seedFile->text().trimmed();
    if (!resume && !seedPath.isEmpty()) {
        QString fileError;
        const auto lines = TsumeRetrogradeGenerator::readInputLines(seedPath, &fileError);
        if (!lines) {
            *errorMessage = tr("種局面ファイルを開けませんでした: %1").arg(fileError);
            return std::nullopt;
        }
        seedLines = *lines;
    }

    QStringList excludeLines;
    const QString excludePath = m_ui.excludeFile->text().trimmed();
    if (!excludePath.isEmpty()) {
        QString fileError;
        const auto lines = TsumeRetrogradeGenerator::readInputLines(excludePath, &fileError);
        if (!lines) {
            *errorMessage = tr("除外ファイルを開けませんでした: %1").arg(fileError);
            return std::nullopt;
        }
        excludeLines = *lines;
    }

    TsumeRetrogradeGenerator::Settings settings;
    settings.enginePath = engine.path;
    settings.engineName = engine.name;
    settings.targetMoves = m_ui.targetMoves->value();
    settings.maxPositionsToFind = m_ui.maxPositions->value();
    settings.maxAttackPieces = m_ui.retroMaxAttack->value();
    settings.maxDefendPieces = m_ui.retroMaxDefend->value();
    settings.beamWidth = m_ui.beamWidth->value();
    settings.branchFactor = m_ui.branchFactor->value();
    settings.stepTimeoutMs = m_ui.timeout->value() * 1000;
    settings.verifyQueryTimeoutMs = qBound(5000, m_ui.timeout->value() * 3000, 60000);
    settings.verifyBudgetMs = static_cast<qint64>(m_ui.verifyBudget->value()) * 1000LL;
    settings.allowFinalMoveAlternatives = m_ui.allowFinalAlternatives->isChecked();
    settings.requireNoSurplus = m_ui.requireNoSurplus->isChecked();
    settings.excludeSimilar = m_ui.excludeSimilar->isChecked();
    settings.strictMinLength = true;
    settings.seedSfens = seedLines;
    settings.excludeLines = excludeLines;
    settings.checkpointPath = checkpointPath;
    settings.resumeFromCheckpoint = resume;
    return settings;
}

// ============================================================
// 欄の状態
// ============================================================

void TsumeshogiGeneratorSettingsPanel::setRunning(bool running)
{
    m_running = running;
    m_engineSelector->setEnabled(!running);
    m_ui.mode->setEnabled(!running);
    m_ui.targetMoves->setEnabled(!running);
    m_ui.maxAttack->setEnabled(!running);
    m_ui.maxDefend->setEnabled(!running);
    m_ui.attackRange->setEnabled(!running);
    m_ui.retroMaxAttack->setEnabled(!running);
    m_ui.retroMaxDefend->setEnabled(!running);
    m_ui.beamWidth->setEnabled(!running);
    m_ui.branchFactor->setEnabled(!running);
    m_ui.verifyBudget->setEnabled(!running);
    m_ui.timeout->setEnabled(!running);
    m_ui.maxPositions->setEnabled(!running);
    m_ui.allowFinalAlternatives->setEnabled(!running);
    m_ui.requireNoSurplus->setEnabled(!running);
    m_ui.excludeSimilar->setEnabled(!running);
    m_ui.excludeFile->setEnabled(!running);
    m_ui.browseExclude->setEnabled(!running);
    m_ui.clearExclude->setEnabled(!running);
    m_ui.checkpointFile->setEnabled(!running);
    m_ui.browseCheckpoint->setEnabled(!running);
    m_ui.clearCheckpoint->setEnabled(!running);
    m_ui.resumeCheckpoint->setEnabled(!running);
    onResumeCheckpointToggled(m_ui.resumeCheckpoint->isChecked());
    m_ui.restoreDefaults->setEnabled(!running);
}

void TsumeshogiGeneratorSettingsPanel::refreshLayout()
{
    // 説明や逆算の設定の開閉で変わった高さを、欄の中のレイアウトに計算し直させる
    for (QWidget* w = m_ui.helpText->parentWidget(); w; w = w->parentWidget()) {
        if (QLayout* l = w->layout()) l->invalidate();
        w->updateGeometry();
        if (w == this) break;
    }
}

void TsumeshogiGeneratorSettingsPanel::applyModeUi()
{
    const bool retro = mode() == Mode::Retrograde;
    m_ui.leftForm->setRowVisible(m_ui.maxAttack, !retro);
    m_ui.leftForm->setRowVisible(m_ui.maxDefend, !retro);
    m_ui.rightForm->setRowVisible(m_ui.attackRange, !retro);
    m_ui.leftForm->setRowVisible(m_ui.retroMaxAttack, retro);
    m_ui.leftForm->setRowVisible(m_ui.retroMaxDefend, retro);
    m_ui.leftForm->setRowVisible(m_ui.beamWidth, retro);
    m_ui.rightForm->setRowVisible(m_ui.branchFactor, retro);
    m_ui.rightForm->setRowVisible(m_ui.verifyBudget, retro);
    m_ui.requireNoSurplus->setVisible(retro);
    m_ui.excludeSimilar->setVisible(retro);
    m_ui.retroFilesWidget->setVisible(retro);
    m_ui.timeoutLabel->setText(retro ? tr("探索時間/手:") : tr("探索時間/局面:"));
    m_ui.timeout->setToolTip(retro
        ? tr("逆算・成長フェーズで1局面を調べる上限時間です（厳密検証では最大3倍まで自動延長）。")
        : tr("候補・駒除去後の詰み探索と、それぞれの余詰検査全体に使う時間です。検査時間を超えた局面は採択しません。"));
    onResumeCheckpointToggled(m_ui.resumeCheckpoint->isChecked());
}

void TsumeshogiGeneratorSettingsPanel::onModeChanged(int index)
{
    Q_UNUSED(index)
    applyModeUi();
    emit contentHeightChanged();
}

void TsumeshogiGeneratorSettingsPanel::onRestoreDefaults()
{
    if (m_running) return;
    // 既定値は設定の宣言の表のもの
    m_conditionsForm.reset();
    applyModeUi();
    emit contentHeightChanged();
}

void TsumeshogiGeneratorSettingsPanel::toggleHelp(bool visible)
{
    m_ui.helpText->setVisible(visible);
    m_ui.help->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
    emit contentHeightChanged();
}

void TsumeshogiGeneratorSettingsPanel::showEngineSettingsDialog(const EngineEntry& engine)
{
    ChangeEngineSettingsDialog::showFor(engine, window());
}

// ============================================================
// 逆算・編集のファイルの欄
// ============================================================

void TsumeshogiGeneratorSettingsPanel::onBrowseSeedFile()
{
    const QString initialDir = m_ui.seedFile->text().trimmed().isEmpty()
        ? TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory()
        : QFileInfo(m_ui.seedFile->text().trimmed()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("種局面ファイルを選択"),
        initialDir,
        tr("テキストファイル (*.txt *.sfen);;すべてのファイル (*)"));
    if (!path.isEmpty()) m_ui.seedFile->setText(path);
}

void TsumeshogiGeneratorSettingsPanel::onClearSeedFile()
{
    m_ui.seedFile->clear();
}

void TsumeshogiGeneratorSettingsPanel::onBrowseExcludeFile()
{
    const QString initialDir = m_ui.excludeFile->text().trimmed().isEmpty()
        ? TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory()
        : QFileInfo(m_ui.excludeFile->text().trimmed()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("除外ファイルを選択"),
        initialDir,
        tr("テキストファイル (*.txt *.sfen);;すべてのファイル (*)"));
    if (!path.isEmpty()) m_ui.excludeFile->setText(path);
}

void TsumeshogiGeneratorSettingsPanel::onClearExcludeFile()
{
    m_ui.excludeFile->clear();
}

void TsumeshogiGeneratorSettingsPanel::onBrowseCheckpointFile()
{
    const QString initialDir = m_ui.checkpointFile->text().trimmed().isEmpty()
        ? TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory()
        : QFileInfo(m_ui.checkpointFile->text().trimmed()).absolutePath();
    const bool resume = m_ui.resumeCheckpoint->isChecked();
    const QString path = resume
        ? QFileDialog::getOpenFileName(this, tr("再開するチェックポイントファイルを選択"), initialDir,
                                       tr("JSON ファイル (*.json);;すべてのファイル (*)"))
        : QFileDialog::getSaveFileName(this, tr("チェックポイントの保存先を選択"), initialDir,
                                       tr("JSON ファイル (*.json);;すべてのファイル (*)"));
    if (!path.isEmpty()) m_ui.checkpointFile->setText(path);
}

void TsumeshogiGeneratorSettingsPanel::onClearCheckpointFile()
{
    m_ui.checkpointFile->clear();
    m_ui.resumeCheckpoint->setChecked(false);
}

void TsumeshogiGeneratorSettingsPanel::onResumeCheckpointToggled(bool checked)
{
    const bool seedEditable = !m_running && !checked;
    m_ui.seedFile->setEnabled(seedEditable);
    m_ui.browseSeed->setEnabled(seedEditable);
    m_ui.clearSeed->setEnabled(seedEditable);
}
