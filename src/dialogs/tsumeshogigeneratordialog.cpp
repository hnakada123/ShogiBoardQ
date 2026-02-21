/// @file tsumeshogigeneratordialog.cpp
/// @brief 詰将棋局面生成ダイアログクラスの実装

#include "tsumeshogigeneratordialog.h"
#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include "settingsservice.h"
#include "tsumeshogigenerator.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace EngineSettingsConstants;

TsumeshogiGeneratorDialog::TsumeshogiGeneratorDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontSize(SettingsService::tsumeshogiGeneratorFontSize())
{
    setWindowTitle(tr("詰将棋局面生成"));
    setupUi();
    applyFontSize();
    readEngineNameAndDir();
    loadSettings();

    // 初期状態
    setRunningState(false);
}

TsumeshogiGeneratorDialog::~TsumeshogiGeneratorDialog()
{
    if (m_generator && m_generator->isRunning()) {
        m_generator->stop();
    }
    saveSettings();
}

void TsumeshogiGeneratorDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- エンジン設定セクション ---
    auto* engineLabel = new QLabel(tr("エンジン設定"), this);
    engineLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(engineLabel);

    auto* engineLayout = new QHBoxLayout;
    engineLayout->addWidget(new QLabel(tr("エンジン:"), this));
    m_comboEngine = new QComboBox(this);
    m_comboEngine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    engineLayout->addWidget(m_comboEngine);
    m_btnEngineSetting = new QPushButton(tr("設定..."), this);
    engineLayout->addWidget(m_btnEngineSetting);
    mainLayout->addLayout(engineLayout);

    // --- 生成設定セクション ---
    auto* settingsLabel = new QLabel(tr("生成設定"), this);
    settingsLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(settingsLabel);

    auto* formLayout = new QFormLayout;

    m_spinTargetMoves = new QSpinBox(this);
    m_spinTargetMoves->setRange(1, 99);
    m_spinTargetMoves->setSingleStep(2);
    m_spinTargetMoves->setValue(3);
    m_spinTargetMoves->setSuffix(tr(" 手詰"));
    formLayout->addRow(tr("目標手数:"), m_spinTargetMoves);

    m_spinMaxAttack = new QSpinBox(this);
    m_spinMaxAttack->setRange(1, 10);
    m_spinMaxAttack->setValue(4);
    m_spinMaxAttack->setSuffix(tr(" 枚"));
    formLayout->addRow(tr("攻め駒上限:"), m_spinMaxAttack);

    m_spinMaxDefend = new QSpinBox(this);
    m_spinMaxDefend->setRange(0, 5);
    m_spinMaxDefend->setValue(1);
    m_spinMaxDefend->setSuffix(tr(" 枚"));
    formLayout->addRow(tr("守り駒上限:"), m_spinMaxDefend);

    m_spinAttackRange = new QSpinBox(this);
    m_spinAttackRange->setRange(1, 8);
    m_spinAttackRange->setValue(3);
    m_spinAttackRange->setSuffix(tr(" マス（玉中心）"));
    formLayout->addRow(tr("配置範囲:"), m_spinAttackRange);

    m_spinTimeout = new QSpinBox(this);
    m_spinTimeout->setRange(1, 300);
    m_spinTimeout->setValue(5);
    m_spinTimeout->setSuffix(tr(" 秒"));
    formLayout->addRow(tr("探索時間/局面:"), m_spinTimeout);

    m_spinMaxPositions = new QSpinBox(this);
    m_spinMaxPositions->setRange(0, 10000);
    m_spinMaxPositions->setValue(10);
    m_spinMaxPositions->setSpecialValueText(tr("無制限"));
    formLayout->addRow(tr("生成上限:"), m_spinMaxPositions);

    mainLayout->addLayout(formLayout);

    // --- 制御ボタン ---
    auto* controlLayout = new QHBoxLayout;
    m_btnStart = new QPushButton(tr("開始"), this);
    m_btnStart->setStyleSheet(ButtonStyles::primaryAction());
    m_btnStop = new QPushButton(tr("停止"), this);
    m_btnStop->setStyleSheet(ButtonStyles::dangerStop());
    controlLayout->addWidget(m_btnStart);
    controlLayout->addWidget(m_btnStop);
    controlLayout->addStretch();
    mainLayout->addLayout(controlLayout);

    // --- プログレス表示 ---
    m_labelProgress = new QLabel(tr("探索済み: 0 局面 / 発見: 0 局面"), this);
    mainLayout->addWidget(m_labelProgress);
    m_labelElapsed = new QLabel(tr("経過時間: 00:00:00"), this);
    mainLayout->addWidget(m_labelElapsed);

    // --- 結果テーブル ---
    auto* resultLabel = new QLabel(tr("結果一覧"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(resultLabel);

    m_tableResults = new QTableWidget(0, 3, this);
    m_tableResults->setHorizontalHeaderLabels({tr("#"), tr("SFEN"), tr("手数")});
    m_tableResults->horizontalHeader()->setStretchLastSection(false);
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_tableResults);

    // --- 下部ボタン行 ---
    auto* bottomLayout = new QHBoxLayout;

    m_btnFontDecrease = new QToolButton(this);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontDecrease);

    m_btnFontIncrease = new QToolButton(this);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontIncrease);

    bottomLayout->addStretch();

    m_btnCopySelected = new QPushButton(tr("選択コピー"), this);
    m_btnCopySelected->setStyleSheet(ButtonStyles::editOperation());
    bottomLayout->addWidget(m_btnCopySelected);

    m_btnCopyAll = new QPushButton(tr("全コピー"), this);
    m_btnCopyAll->setStyleSheet(ButtonStyles::editOperation());
    bottomLayout->addWidget(m_btnCopyAll);

    m_btnClose = new QPushButton(tr("閉じる"), this);
    m_btnClose->setStyleSheet(ButtonStyles::secondaryNeutral());
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);

    // --- シグナル接続 ---
    connect(m_btnStart, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_btnCopySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_btnCopyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_btnClose, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_btnFontIncrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontIncrease);
    connect(m_btnFontDecrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontDecrease);
    connect(m_btnEngineSetting, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::showEngineSettingsDialog);

    // ウィンドウサイズ復元
    const QSize savedSize = SettingsService::tsumeshogiGeneratorDialogSize();
    if (savedSize.isValid()) {
        resize(savedSize);
    } else {
        resize(600, 550);
    }
}

void TsumeshogiGeneratorDialog::readEngineNameAndDir()
{
    QSettings settings(SettingsService::settingsFilePath(), QSettings::IniFormat);

    int size = settings.beginReadArray(EnginesGroupName);
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        Engine engine;
        engine.name = settings.value(EngineNameKey).toString();
        engine.path = settings.value(EnginePathKey).toString();
        m_comboEngine->addItem(engine.name);
        m_engineList.append(engine);
    }
    settings.endArray();
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    int engineIndex = SettingsService::tsumeshogiGeneratorEngineIndex();
    if (engineIndex >= 0 && engineIndex < m_comboEngine->count()) {
        m_comboEngine->setCurrentIndex(engineIndex);
    }

    m_spinTargetMoves->setValue(SettingsService::tsumeshogiGeneratorTargetMoves());
    m_spinMaxAttack->setValue(SettingsService::tsumeshogiGeneratorMaxAttackPieces());
    m_spinMaxDefend->setValue(SettingsService::tsumeshogiGeneratorMaxDefendPieces());
    m_spinAttackRange->setValue(SettingsService::tsumeshogiGeneratorAttackRange());
    m_spinTimeout->setValue(SettingsService::tsumeshogiGeneratorTimeoutSec());
    m_spinMaxPositions->setValue(SettingsService::tsumeshogiGeneratorMaxPositions());
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    SettingsService::setTsumeshogiGeneratorDialogSize(size());
    SettingsService::setTsumeshogiGeneratorFontSize(m_fontSize);
    SettingsService::setTsumeshogiGeneratorEngineIndex(m_comboEngine->currentIndex());
    SettingsService::setTsumeshogiGeneratorTargetMoves(m_spinTargetMoves->value());
    SettingsService::setTsumeshogiGeneratorMaxAttackPieces(m_spinMaxAttack->value());
    SettingsService::setTsumeshogiGeneratorMaxDefendPieces(m_spinMaxDefend->value());
    SettingsService::setTsumeshogiGeneratorAttackRange(m_spinAttackRange->value());
    SettingsService::setTsumeshogiGeneratorTimeoutSec(m_spinTimeout->value());
    SettingsService::setTsumeshogiGeneratorMaxPositions(m_spinMaxPositions->value());
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    // ジェネレータを作成
    m_generator = std::make_unique<TsumeshogiGenerator>(this);
    connect(m_generator.get(), &TsumeshogiGenerator::positionFound,
            this, &TsumeshogiGeneratorDialog::onPositionFound);
    connect(m_generator.get(), &TsumeshogiGenerator::progressUpdated,
            this, &TsumeshogiGeneratorDialog::onProgressUpdated);
    connect(m_generator.get(), &TsumeshogiGenerator::finished,
            this, &TsumeshogiGeneratorDialog::onGeneratorFinished);
    connect(m_generator.get(), &TsumeshogiGenerator::errorOccurred,
            this, &TsumeshogiGeneratorDialog::onGeneratorError);

    // 設定を構築
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = m_engineList.at(engineIndex).path;
    settings.engineName = m_engineList.at(engineIndex).name;
    settings.targetMoves = m_spinTargetMoves->value();
    settings.timeoutMs = m_spinTimeout->value() * 1000;
    settings.maxPositionsToFind = m_spinMaxPositions->value();
    settings.posGenSettings.maxAttackPieces = m_spinMaxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_spinMaxDefend->value();
    settings.posGenSettings.attackRange = m_spinAttackRange->value();

    // 結果テーブルをクリア
    m_tableResults->setRowCount(0);

    setRunningState(true);
    m_generator->start(settings);
}

void TsumeshogiGeneratorDialog::onStopClicked()
{
    if (m_generator) {
        m_generator->stop();
    }
}

void TsumeshogiGeneratorDialog::onPositionFound(const QString& sfen, const QStringList& pv)
{
    const int row = m_tableResults->rowCount();
    m_tableResults->insertRow(row);

    auto* itemNum = new QTableWidgetItem(QString::number(row + 1));
    itemNum->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 0, itemNum);

    m_tableResults->setItem(row, 1, new QTableWidgetItem(sfen));

    auto* itemMoves = new QTableWidgetItem(QString::number(pv.size()));
    itemMoves->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 2, itemMoves);

    // 最新行にスクロール
    m_tableResults->scrollToBottom();
}

void TsumeshogiGeneratorDialog::onProgressUpdated(int tried, int found, qint64 elapsedMs)
{
    m_labelProgress->setText(tr("探索済み: %1 局面 / 発見: %2 局面").arg(tried).arg(found));
    m_labelElapsed->setText(tr("経過時間: %1").arg(formatElapsedTime(elapsedMs)));
}

void TsumeshogiGeneratorDialog::onGeneratorFinished()
{
    setRunningState(false);
}

void TsumeshogiGeneratorDialog::onGeneratorError(const QString& message)
{
    QMessageBox::warning(this, tr("エラー"), message);
    setRunningState(false);
}

void TsumeshogiGeneratorDialog::onCopySelected()
{
    const auto selectedRows = m_tableResults->selectionModel()->selectedRows(1);
    if (selectedRows.isEmpty()) return;

    QStringList sfenList;
    for (const auto& index : std::as_const(selectedRows)) {
        sfenList.append(index.data().toString());
    }
    QApplication::clipboard()->setText(sfenList.join('\n'));
}

void TsumeshogiGeneratorDialog::onCopyAll()
{
    if (m_tableResults->rowCount() == 0) return;

    QStringList sfenList;
    for (int row = 0; row < m_tableResults->rowCount(); ++row) {
        const auto* item = m_tableResults->item(row, 1);
        if (item) {
            sfenList.append(item->text());
        }
    }
    QApplication::clipboard()->setText(sfenList.join('\n'));
}

void TsumeshogiGeneratorDialog::onFontIncrease()
{
    updateFontSize(1);
}

void TsumeshogiGeneratorDialog::onFontDecrease()
{
    updateFontSize(-1);
}

void TsumeshogiGeneratorDialog::showEngineSettingsDialog()
{
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    ChangeEngineSettingsDialog dialog(this);
    dialog.setEngineNumber(engineIndex);
    dialog.setEngineName(m_engineList.at(engineIndex).name);
    dialog.setupEngineOptionsDialog();
    dialog.exec();
}

void TsumeshogiGeneratorDialog::updateFontSize(int delta)
{
    m_fontSize += delta;
    m_fontSize = qBound(8, m_fontSize, 24);
    applyFontSize();
    SettingsService::setTsumeshogiGeneratorFontSize(m_fontSize);
}

void TsumeshogiGeneratorDialog::applyFontSize()
{
    m_fontSize = qBound(8, m_fontSize, 24);

    QFont f = font();
    f.setPointSize(m_fontSize);
    setFont(f);

    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget) {
            widget->setFont(f);
        }
    }

    const QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : std::as_const(comboBoxes)) {
        if (comboBox && comboBox->view()) {
            comboBox->view()->setFont(f);
        }
    }
}

void TsumeshogiGeneratorDialog::setRunningState(bool running)
{
    m_btnStart->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_comboEngine->setEnabled(!running);
    m_btnEngineSetting->setEnabled(!running);
    m_spinTargetMoves->setEnabled(!running);
    m_spinMaxAttack->setEnabled(!running);
    m_spinMaxDefend->setEnabled(!running);
    m_spinAttackRange->setEnabled(!running);
    m_spinTimeout->setEnabled(!running);
    m_spinMaxPositions->setEnabled(!running);
}

QString TsumeshogiGeneratorDialog::formatElapsedTime(qint64 ms) const
{
    const int totalSec = static_cast<int>(ms / 1000);
    const int hours = totalSec / 3600;
    const int minutes = (totalSec % 3600) / 60;
    const int seconds = totalSec % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
