/// @file tsumeshogigeneratordialog.cpp
/// @brief 詰将棋局面生成ダイアログの設定・生成制御

#include "tsumeshogigeneratordialog.h"
#include "tsumeshogisettings.h"
#include "dialogutils.h"
#include "enginelistsettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

TsumeshogiGeneratorDialog::TsumeshogiGeneratorDialog(QWidget* parent)
    : QDialog(parent)
    , m_generator(new TsumeshogiGenerator(this))
    , m_fontHelper(TsumeshogiSettings::tsumeshogiGeneratorFontSize, 8, 24, 1)
{
    setWindowTitle(tr("詰将棋局面生成"));
    setupUi();
    applyFontSize();
    readEngineNameAndDir();
    loadSettings();
    DialogUtils::restoreDialogSize(this, TsumeshogiSettings::tsumeshogiGeneratorDialogSize());

    // 初期状態
    setRunningState(false);
    setStatusText(m_engineList.isEmpty()
        ? tr("設定メニューで詰将棋エンジンを登録してください。") : tr("待機中"));
}

TsumeshogiGeneratorDialog::~TsumeshogiGeneratorDialog()
{
    // ウィジェット破棄前に停止する。トリミング未完了の局面は出力しない。Idle なら何もしない
    m_generator->stop();
    saveSettings();
}

void TsumeshogiGeneratorDialog::done(int result)
{
    onStopClicked();
    saveSettings();
    QDialog::done(result);
}

void TsumeshogiGeneratorDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // 説明を開いたまま閉じた場合も、次に開いたとき設定欄を隠さない（配置が決まってから調整する）
    if (m_settingsUi.help->isChecked()) QTimer::singleShot(0, this, &TsumeshogiGeneratorDialog::fitSettingsArea);
}

void TsumeshogiGeneratorDialog::connectDialogSignals()
{
    connect(m_progressUi.start, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_progressUi.stop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_resultsUi.save, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onSaveToFile);
    connect(m_resultsUi.copySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_resultsUi.copyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_footerUi.close, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_footerUi.fontIncrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontIncrease);
    connect(m_footerUi.fontDecrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontDecrease);
    connect(m_settingsUi.restoreDefaults, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onRestoreDefaults);
    connect(m_settingsUi.engineSettings, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::showEngineSettingsDialog);
    connect(m_resultsUi.table, &QTableWidget::clicked, this, &TsumeshogiGeneratorDialog::onResultTableClicked);
    connect(m_resultsUi.table, &QTableWidget::activated, this, &TsumeshogiGeneratorDialog::onResultTableActivated);
    connect(m_resultsUi.table, &QTableWidget::itemSelectionChanged, this, &TsumeshogiGeneratorDialog::updateResultActions);
    connect(m_settingsUi.maxPositions, &QSpinBox::valueChanged, this, &TsumeshogiGeneratorDialog::updateProgressBar);
    connect(m_settingsUi.help, &QToolButton::toggled, this, &TsumeshogiGeneratorDialog::toggleHelp);

    connect(m_generator, &TsumeshogiGenerator::positionFound,
            this, &TsumeshogiGeneratorDialog::onPositionFound);
    connect(m_generator, &TsumeshogiGenerator::progressUpdated,
            this, &TsumeshogiGeneratorDialog::onProgressUpdated);
    connect(m_generator, &TsumeshogiGenerator::finished,
            this, &TsumeshogiGeneratorDialog::onGeneratorFinished);
    connect(m_generator, &TsumeshogiGenerator::errorOccurred,
            this, &TsumeshogiGeneratorDialog::onGeneratorError);
    connect(m_generator, &TsumeshogiGenerator::searchPhaseStarted,
            this, &TsumeshogiGeneratorDialog::onSearchPhaseStarted);
    connect(m_generator, &TsumeshogiGenerator::trimmingProgress,
            this, &TsumeshogiGeneratorDialog::onTrimmingProgress);
    connect(m_generator, &TsumeshogiGenerator::verificationProgress,
            this, &TsumeshogiGeneratorDialog::onVerificationProgress);
    connect(m_generator, &TsumeshogiGenerator::verificationStatsUpdated,
            this, &TsumeshogiGeneratorDialog::onVerificationStatsUpdated);
}

void TsumeshogiGeneratorDialog::readEngineNameAndDir()
{
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (const auto& entry : engines) {
        Engine engine;
        engine.name = entry.name;
        engine.path = entry.path;
        engine.author = entry.author;
        m_settingsUi.engine->addItem(engine.name);
        m_engineList.append(engine);
    }
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    const int engineIndex =
        EngineListSettings::restoreSelection(EngineListSettings::tsumeshogiGeneratorSelection(), m_engineList);
    if (engineIndex >= 0) {
        m_settingsUi.engine->setCurrentIndex(engineIndex);
    }

    // 旧バージョンで偶数が保存されていた場合に備えて奇数に補正する
    int targetMoves = TsumeshogiSettings::tsumeshogiGeneratorTargetMoves();
    if (targetMoves % 2 == 0) {
        targetMoves = qMax(1, targetMoves - 1);
    }
    m_settingsUi.targetMoves->setValue(targetMoves);
    m_settingsUi.maxAttack->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces());
    m_settingsUi.maxDefend->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces());
    m_settingsUi.attackRange->setValue(TsumeshogiSettings::tsumeshogiGeneratorAttackRange());
    m_settingsUi.timeout->setValue(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec());
    m_settingsUi.maxPositions->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions());
    m_resultsUi.includePv->setChecked(TsumeshogiSettings::tsumeshogiGeneratorIncludePv());
    m_settingsUi.allowFinalAlternatives->setChecked(TsumeshogiSettings::tsumeshogiGeneratorAllowFinalMoveAlternatives());
    m_settingsUi.help->setChecked(TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded());
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    DialogUtils::saveDialogSize(this, TsumeshogiSettings::tsumeshogiGeneratorDialogSize);
    TsumeshogiSettings::tsumeshogiGeneratorFontSize.set(m_fontHelper.fontSize());
    const int engineIndex = m_settingsUi.engine->currentIndex();
    if (engineIndex >= 0 && engineIndex < m_engineList.size()) {
        EngineListSettings::saveSelection(EngineListSettings::tsumeshogiGeneratorSelection(),
                                          m_engineList.at(engineIndex).name);
    }
    TsumeshogiSettings::tsumeshogiGeneratorTargetMoves.set(m_settingsUi.targetMoves->value());
    TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces.set(m_settingsUi.maxAttack->value());
    TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces.set(m_settingsUi.maxDefend->value());
    TsumeshogiSettings::tsumeshogiGeneratorAttackRange.set(m_settingsUi.attackRange->value());
    TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec.set(m_settingsUi.timeout->value());
    TsumeshogiSettings::tsumeshogiGeneratorMaxPositions.set(m_settingsUi.maxPositions->value());
    TsumeshogiSettings::tsumeshogiGeneratorIncludePv.set(m_resultsUi.includePv->isChecked());
    TsumeshogiSettings::tsumeshogiGeneratorAllowFinalMoveAlternatives.set(m_settingsUi.allowFinalAlternatives->isChecked());
    TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded.set(m_settingsUi.help->isChecked());
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    if (m_running) return;
    const int engineIndex = m_settingsUi.engine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    // 設定を構築
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = m_engineList.at(engineIndex).path;
    settings.engineName = m_engineList.at(engineIndex).name;
    settings.targetMoves = m_settingsUi.targetMoves->value();
    settings.timeoutMs = m_settingsUi.timeout->value() * 1000;
    settings.maxPositionsToFind = m_settingsUi.maxPositions->value();
    settings.allowFinalMoveAlternatives = m_settingsUi.allowFinalAlternatives->isChecked();
    settings.posGenSettings.maxAttackPieces = m_settingsUi.maxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_settingsUi.maxDefend->value();
    settings.posGenSettings.attackRange = m_settingsUi.attackRange->value();

    // 結果テーブルをクリア
    m_resultsUi.table->setRowCount(0);

    // ファイル保存時のコメントヘッダ用に、この実行の設定と開始日時を控える
    m_lastRunSettings = settings;
    m_lastRunStartedAt = QDateTime::currentDateTime();
    m_stopRequested = false;
    m_runFailed = false;

    setRunningState(true);
    onProgressUpdated(0, 0, 0);
    onVerificationStatsUpdated(0, 0);
    setStatusText(tr("エンジンを起動中"));
    m_generator->start(settings);
}

void TsumeshogiGeneratorDialog::onStopClicked()
{
    if (!m_running) return;
    m_stopRequested = true;
    m_generator->stop();
}
