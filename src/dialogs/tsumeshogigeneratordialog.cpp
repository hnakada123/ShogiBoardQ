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
#include <QToolButton>

TsumeshogiGeneratorDialog::TsumeshogiGeneratorDialog(QWidget* parent)
    : QDialog(parent)
    , m_generator(new TsumeshogiGenerator(this))
    , m_fontHelper({TsumeshogiSettings::tsumeshogiGeneratorFontSize(), 8, 24, 1,
                    TsumeshogiSettings::setTsumeshogiGeneratorFontSize})
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

void TsumeshogiGeneratorDialog::connectDialogSignals()
{
    connect(m_btnStart, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_btnSaveToFile, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onSaveToFile);
    connect(m_btnCopySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_btnCopyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_btnClose, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_btnFontIncrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontIncrease);
    connect(m_btnFontDecrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontDecrease);
    connect(m_btnRestoreDefaults, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onRestoreDefaults);
    connect(m_btnEngineSetting, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::showEngineSettingsDialog);
    connect(m_tableResults, &QTableWidget::clicked, this, &TsumeshogiGeneratorDialog::onResultTableClicked);
    connect(m_tableResults, &QTableWidget::activated, this, &TsumeshogiGeneratorDialog::onResultTableActivated);
    connect(m_tableResults, &QTableWidget::itemSelectionChanged, this, &TsumeshogiGeneratorDialog::updateResultActions);
    connect(m_spinMaxPositions, &QSpinBox::valueChanged, this, &TsumeshogiGeneratorDialog::updateProgressBar);
    connect(m_btnHelp, &QToolButton::toggled, this, &TsumeshogiGeneratorDialog::toggleHelp);

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
        m_comboEngine->addItem(engine.name);
        m_engineList.append(engine);
    }
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    int engineIndex = TsumeshogiSettings::tsumeshogiGeneratorEngineIndex();
    if (engineIndex >= 0 && engineIndex < m_comboEngine->count()) {
        m_comboEngine->setCurrentIndex(engineIndex);
    }

    // 旧バージョンで偶数が保存されていた場合に備えて奇数に補正する
    int targetMoves = TsumeshogiSettings::tsumeshogiGeneratorTargetMoves();
    if (targetMoves % 2 == 0) {
        targetMoves = qMax(1, targetMoves - 1);
    }
    m_spinTargetMoves->setValue(targetMoves);
    m_spinMaxAttack->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces());
    m_spinMaxDefend->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces());
    m_spinAttackRange->setValue(TsumeshogiSettings::tsumeshogiGeneratorAttackRange());
    m_spinTimeout->setValue(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec());
    m_spinMaxPositions->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions());
    m_checkIncludePv->setChecked(TsumeshogiSettings::tsumeshogiGeneratorIncludePv());
    m_checkAllowFinalAlternatives->setChecked(TsumeshogiSettings::tsumeshogiGeneratorAllowFinalMoveAlternatives());
    m_btnHelp->setChecked(TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded());
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    DialogUtils::saveDialogSize(this, TsumeshogiSettings::setTsumeshogiGeneratorDialogSize);
    TsumeshogiSettings::setTsumeshogiGeneratorFontSize(m_fontHelper.fontSize());
    TsumeshogiSettings::setTsumeshogiGeneratorEngineIndex(m_comboEngine->currentIndex());
    TsumeshogiSettings::setTsumeshogiGeneratorTargetMoves(m_spinTargetMoves->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxAttackPieces(m_spinMaxAttack->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxDefendPieces(m_spinMaxDefend->value());
    TsumeshogiSettings::setTsumeshogiGeneratorAttackRange(m_spinAttackRange->value());
    TsumeshogiSettings::setTsumeshogiGeneratorTimeoutSec(m_spinTimeout->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxPositions(m_spinMaxPositions->value());
    TsumeshogiSettings::setTsumeshogiGeneratorIncludePv(m_checkIncludePv->isChecked());
    TsumeshogiSettings::setTsumeshogiGeneratorAllowFinalMoveAlternatives(m_checkAllowFinalAlternatives->isChecked());
    TsumeshogiSettings::setTsumeshogiGeneratorHelpExpanded(m_btnHelp->isChecked());
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    if (m_running) return;
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    // 設定を構築
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = m_engineList.at(engineIndex).path;
    settings.engineName = m_engineList.at(engineIndex).name;
    settings.targetMoves = m_spinTargetMoves->value();
    settings.timeoutMs = m_spinTimeout->value() * 1000;
    settings.maxPositionsToFind = m_spinMaxPositions->value();
    settings.allowFinalMoveAlternatives = m_checkAllowFinalAlternatives->isChecked();
    settings.posGenSettings.maxAttackPieces = m_spinMaxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_spinMaxDefend->value();
    settings.posGenSettings.attackRange = m_spinAttackRange->value();

    // 結果テーブルをクリア
    m_tableResults->setRowCount(0);

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
