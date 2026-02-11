/// @file startgamedialog.cpp
/// @brief 対局開始ダイアログクラスの実装

#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include "settingsservice.h"
#include <QSettings>
#include <QMessageBox>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

using namespace EngineSettingsConstants;

// ============================================================
// 初期化
// ============================================================

StartGameDialog::StartGameDialog(QWidget *parent) : QDialog(parent), ui(new Ui::StartGameDialog)
{
    ui->setupUi(this);

    // グリッドレイアウトの列ストレッチを設定（ラベル列は伸びず、コントロール列が伸びる）
    ui->gridLayoutPlayer1->setColumnStretch(0, 0);
    ui->gridLayoutPlayer1->setColumnStretch(1, 1);
    ui->gridLayoutPlayer2->setColumnStretch(0, 0);
    ui->gridLayoutPlayer2->setColumnStretch(1, 1);

    loadFontSizeSettings();
    loadEngineConfigurations();
    populatePlayerComboBoxes();
    loadGameSettings();
    connectSignalsAndSlots();

    // ウィンドウサイズを復元
    QSize savedSize = SettingsService::startGameDialogSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    }
}

StartGameDialog::~StartGameDialog()
{
    SettingsService::setStartGameDialogSize(size());
    delete ui;
}

// ============================================================
// シグナル接続
// ============================================================

void StartGameDialog::connectSignalsAndSlots() const
{
    // エンジン設定ボタン・先後入れ替え・リセット・保存
    connect(ui->pushButtonEngineSettings1, &QPushButton::clicked, this,
            &StartGameDialog::onFirstPlayerSettingsClicked);
    connect(ui->pushButtonEngineSettings2, &QPushButton::clicked, this,
            &StartGameDialog::onSecondPlayerSettingsClicked);
    connect(ui->pushButtonSwapSides, &QPushButton::clicked, this, &StartGameDialog::swapSides);
    connect(ui->pushButtonResetToDefault, &QPushButton::clicked, this, &StartGameDialog::resetSettingsToDefault);
    connect(ui->pushButtonSaveSettingsOnly, &QPushButton::clicked, this, &StartGameDialog::saveGameSettings);

    // OK/キャンセル: OKは設定保存→パラメータ取得→ダイアログ閉じの順で実行される
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::saveGameSettings);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::updateGameSettingsFromDialog);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &StartGameDialog::reject);

    // 秒読みと加算時間は排他制約（一方に値が入ると他方を0にする）
    connect(ui->byoyomiSec1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleByoyomiSecChanged);
    connect(ui->addEachMoveSec1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleAddEachMoveSecChanged);
    connect(ui->byoyomiSec2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleByoyomiSecChanged);
    connect(ui->addEachMoveSec2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleAddEachMoveSecChanged);

    // フォント拡大縮小
    connect(ui->pushButtonFontSizeUp, &QPushButton::clicked, this, &StartGameDialog::increaseFontSize);
    connect(ui->pushButtonFontSizeDown, &QPushButton::clicked, this, &StartGameDialog::decreaseFontSize);

    // 対局者種別（人間/エンジン）変更時にUIを切り替える
    connect(ui->comboBoxPlayer1, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer1SelectionChanged);
    connect(ui->comboBoxPlayer2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer2SelectionChanged);

    // 棋譜保存先ディレクトリ選択
    connect(ui->pushButtonSelectKifuDir, &QPushButton::clicked, this, &StartGameDialog::onSelectKifuDirClicked);
}

// ============================================================
// UIハンドラ
// ============================================================

void StartGameDialog::handleByoyomiSecChanged(int value)
{
    // 秒読みと加算時間は排他: 秒読みが入ったら加算時間を0に
    if (value > 0) {
        ui->addEachMoveSec1->setValue(0);
        ui->addEachMoveSec2->setValue(0);
    }
}

void StartGameDialog::handleAddEachMoveSecChanged(int value)
{
    // 秒読みと加算時間は排他: 加算時間が入ったら秒読みを0に
    if (value > 0) {
        ui->byoyomiSec1->setValue(0);
        ui->byoyomiSec2->setValue(0);
    }
}

// ============================================================
// 設定保存・読込
// ============================================================

void StartGameDialog::saveGameSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を保存
    // インデックス0は「人間」、1以上はエンジン
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    bool isHuman1 = (player1Index == 0);
    settings.setValue("isHuman1", isHuman1);
    settings.setValue("isEngine1", !isHuman1);
    settings.setValue("humanName1", ui->lineEditHumanName1->text());
    if (!isHuman1) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber1", player1Index - 1);
        settings.setValue("engineName1", ui->comboBoxPlayer1->currentText());
    }
    settings.setValue("basicTimeHour1", ui->basicTimeHour1->value());
    settings.setValue("basicTimeMinutes1", ui->basicTimeMinutes1->value());
    settings.setValue("byoyomiSec1", ui->byoyomiSec1->value());
    settings.setValue("addEachMoveSec1", ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存
    settings.setValue("isGroupBoxSecondPlayerTimeSettingsChecked", ui->groupBoxSecondPlayerTimeSettings->isChecked());

    int player2Index = ui->comboBoxPlayer2->currentIndex();
    bool isHuman2 = (player2Index == 0);
    settings.setValue("isHuman2", isHuman2);
    settings.setValue("isEngine2", !isHuman2);
    settings.setValue("humanName2", ui->lineEditHumanName2->text());
    if (!isHuman2) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber2", player2Index - 1);
        settings.setValue("engineName2", ui->comboBoxPlayer2->currentText());
    }
    settings.setValue("basicTimeHour2", ui->basicTimeHour2->value());
    settings.setValue("basicTimeMinutes2", ui->basicTimeMinutes2->value());
    settings.setValue("byoyomiSec2", ui->byoyomiSec2->value());
    settings.setValue("addEachMoveSec2", ui->addEachMoveSec2->value());

    settings.setValue("startingPositionName", ui->comboBoxStartingPosition->currentText());
    settings.setValue("startingPositionNumber", ui->comboBoxStartingPosition->currentIndex());
    settings.setValue("maxMoves", ui->spinBoxMaxMoves->value());
    settings.setValue("consecutiveGames", ui->spinBoxConsecutiveGames->value());
    settings.setValue("isShowHumanInFront", ui->checkBoxShowHumanInFront->isChecked());
    settings.setValue("isAutoSaveKifu", ui->checkBoxAutoSaveKifu->isChecked());
    settings.setValue("kifuSaveDir", ui->lineEditKifuSaveDir->text());
    settings.setValue("isLoseOnTimeout", ui->checkBoxLoseOnTimeOut->isChecked());
    settings.setValue("isSwitchTurnEachGame", ui->checkBoxSwitchTurnEachGame->isChecked());
    settings.setValue("jishogiRule", ui->comboBoxJishogi->currentIndex());

    settings.endGroup();
}

void StartGameDialog::loadGameSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を読み込む
    bool isHuman1 = settings.value("isHuman1", true).toBool();
    ui->lineEditHumanName1->setText(settings.value("humanName1", tr("You")).toString());
    if (isHuman1) {
        ui->comboBoxPlayer1->setCurrentIndex(0);
    } else {
        // エンジン番号+1がコンボボックスのインデックスになる
        int engineNumber = settings.value("engineNumber1", 0).toInt();
        ui->comboBoxPlayer1->setCurrentIndex(engineNumber + 1);
    }
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(settings.value("basicTimeHour1", 0).toInt());
    ui->basicTimeMinutes1->setValue(settings.value("basicTimeMinutes1", 0).toInt());
    ui->byoyomiSec1->setValue(settings.value("byoyomiSec1", 0).toInt());
    ui->addEachMoveSec1->setValue(settings.value("addEachMoveSec1", 0).toInt());

    // 後手／上手の設定を読み込む
    bool isChecked = settings.value("isGroupBoxSecondPlayerTimeSettingsChecked", false).toBool();
    ui->groupBoxSecondPlayerTimeSettings->setChecked(isChecked);

    bool isHuman2 = settings.value("isHuman2", false).toBool();
    ui->lineEditHumanName2->setText(settings.value("humanName2", tr("You")).toString());
    if (isHuman2) {
        ui->comboBoxPlayer2->setCurrentIndex(0);
    } else {
        // エンジン番号+1がコンボボックスのインデックスになる
        int engineNumber = settings.value("engineNumber2", 0).toInt();
        ui->comboBoxPlayer2->setCurrentIndex(engineNumber + 1);
    }
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(settings.value("basicTimeHour2", 0).toInt());
    ui->basicTimeMinutes2->setValue(settings.value("basicTimeMinutes2", 0).toInt());
    ui->byoyomiSec2->setValue(settings.value("byoyomiSec2", 0).toInt());
    ui->addEachMoveSec2->setValue(settings.value("addEachMoveSec2", 0).toInt());

    ui->comboBoxStartingPosition->setCurrentText(settings.value("startingPositionName", "平手").toString());
    ui->comboBoxStartingPosition->setCurrentIndex(settings.value("startingPositionNumber", 1).toInt());
    ui->spinBoxMaxMoves->setValue(settings.value("maxMoves", 1000).toInt());
    ui->spinBoxConsecutiveGames->setValue(settings.value("consecutiveGames", 1).toInt());
    ui->checkBoxShowHumanInFront->setChecked(settings.value("isShowHumanInFront", true).toBool());
    ui->checkBoxAutoSaveKifu->setChecked(settings.value("isAutoSaveKifu", false).toBool());

    // デフォルトの棋譜保存先はドキュメントフォルダ
    QString defaultKifuDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    ui->lineEditKifuSaveDir->setText(settings.value("kifuSaveDir", defaultKifuDir).toString());

    ui->checkBoxLoseOnTimeOut->setChecked(settings.value("isLoseOnTimeout", true).toBool());
    ui->checkBoxSwitchTurnEachGame->setChecked(settings.value("isSwitchTurnEachGame", false).toBool());
    ui->comboBoxJishogi->setCurrentIndex(settings.value("jishogiRule", 0).toInt());

    settings.endGroup();

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    // 先手: 人間をデフォルトに
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);
    ui->lineEditHumanName1->setText(tr("You"));
    ui->basicTimeHour1->setValue(0);
    ui->basicTimeMinutes1->setValue(30);
    ui->byoyomiSec1->setValue(30);
    ui->addEachMoveSec1->setValue(0);

    // 後手: 最初のエンジンをデフォルトに（なければ人間）
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }
    ui->lineEditHumanName2->setText(tr("You"));
    ui->basicTimeHour2->setValue(0);
    ui->basicTimeMinutes2->setValue(30);
    ui->byoyomiSec2->setValue(30);
    ui->addEachMoveSec2->setValue(0);

    // 開始局面: 平手
    ui->comboBoxStartingPosition->setCurrentIndex(1);
    ui->spinBoxMaxMoves->setValue(1000);
    ui->spinBoxConsecutiveGames->setValue(1);
    ui->checkBoxShowHumanInFront->setChecked(true);
    ui->checkBoxAutoSaveKifu->setChecked(false);
    ui->lineEditKifuSaveDir->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    ui->checkBoxLoseOnTimeOut->setChecked(true);
    ui->checkBoxSwitchTurnEachGame->setChecked(false);
    ui->comboBoxJishogi->setCurrentIndex(0);
}

// ============================================================
// 先後入れ替え
// ============================================================

void StartGameDialog::swapSides()
{
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    int player2Index = ui->comboBoxPlayer2->currentIndex();
    ui->comboBoxPlayer1->setCurrentIndex(player2Index);
    ui->comboBoxPlayer2->setCurrentIndex(player1Index);

    updatePlayerUI(1, player2Index);
    updatePlayerUI(2, player1Index);

    QString temp = ui->lineEditHumanName1->text();
    ui->lineEditHumanName1->setText(ui->lineEditHumanName2->text());
    ui->lineEditHumanName2->setText(temp);

    // 「後手に異なる時間を設定」が有効な場合のみ持ち時間も入れ替え
    if (ui->groupBoxSecondPlayerTimeSettings->isChecked()) {
        int tempInt = ui->basicTimeHour1->value();
        ui->basicTimeHour1->setValue(ui->basicTimeHour2->value());
        ui->basicTimeHour2->setValue(tempInt);

        tempInt = ui->basicTimeMinutes1->value();
        ui->basicTimeMinutes1->setValue(ui->basicTimeMinutes2->value());
        ui->basicTimeMinutes2->setValue(tempInt);

        tempInt = ui->byoyomiSec1->value();
        ui->byoyomiSec1->setValue(ui->byoyomiSec2->value());
        ui->byoyomiSec2->setValue(tempInt);

        tempInt = ui->addEachMoveSec1->value();
        ui->addEachMoveSec1->setValue(ui->addEachMoveSec2->value());
        ui->addEachMoveSec2->setValue(tempInt);
    }
}

// ============================================================
// エンジン設定読み込み
// ============================================================

void StartGameDialog::loadEngineConfigurations()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    int size = settings.beginReadArray("Engines");
    engineList.clear();

    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Engine engine;
        engine.name = settings.value("name").toString();
        engine.path = settings.value("path").toString();
        engineList.append(engine);
    }

    settings.endArray();
}

void StartGameDialog::populatePlayerComboBoxes()
{
    ui->comboBoxPlayer1->clear();
    ui->comboBoxPlayer2->clear();

    // インデックス0は「人間」
    ui->comboBoxPlayer1->addItem(tr("人間"));
    ui->comboBoxPlayer2->addItem(tr("人間"));

    // インデックス1以降にエンジンを追加
    foreach (const Engine& engine, engineList) {
        ui->comboBoxPlayer1->addItem(engine.name);
        ui->comboBoxPlayer2->addItem(engine.name);
    }
}

// ============================================================
// Getter
// ============================================================

bool StartGameDialog::isHuman1() const { return m_isHuman1; }
bool StartGameDialog::isHuman2() const { return m_isHuman2; }
bool StartGameDialog::isEngine1() const { return m_isEngine1; }
bool StartGameDialog::isEngine2() const { return m_isEngine2; }
const QString& StartGameDialog::engineName1() const { return m_engineName1; }
const QString& StartGameDialog::engineName2() const { return m_engineName2; }
const QString& StartGameDialog::humanName1() const { return m_humanName1; }
const QString& StartGameDialog::humanName2() const { return m_humanName2; }
int StartGameDialog::engineNumber1() const { return m_engineNumber1; }
int StartGameDialog::engineNumber2() const { return m_engineNumber2; }
int StartGameDialog::basicTimeHour1() const { return m_basicTimeHour1; }
int StartGameDialog::basicTimeMinutes1() const { return m_basicTimeMinutes1; }
int StartGameDialog::addEachMoveSec1() const { return m_addEachMoveSec1; }
int StartGameDialog::byoyomiSec1() const { return m_byoyomiSec1; }
int StartGameDialog::basicTimeHour2() const { return m_basicTimeHour2; }
int StartGameDialog::basicTimeMinutes2() const { return m_basicTimeMinutes2; }
int StartGameDialog::addEachMoveSec2() const { return m_addEachMoveSec2; }
int StartGameDialog::byoyomiSec2() const { return m_byoyomiSec2; }
const QString &StartGameDialog::startingPositionName() const { return m_startingPositionName; }
int StartGameDialog::startingPositionNumber() const { return m_startingPositionNumber; }
const QList<StartGameDialog::Engine> &StartGameDialog::getEngineList() const { return engineList; }
int StartGameDialog::maxMoves() const { return m_maxMoves; }
int StartGameDialog::consecutiveGames() const { return m_consecutiveGames; }
bool StartGameDialog::isShowHumanInFront() const { return m_isShowHumanInFront; }
bool StartGameDialog::isAutoSaveKifu() const { return m_isAutoSaveKifu; }
const QString& StartGameDialog::kifuSaveDir() const { return m_kifuSaveDir; }
bool StartGameDialog::isLoseOnTimeout() const { return m_isLoseOnTimeout; }
bool StartGameDialog::isSwitchTurnEachGame() const { return m_isSwitchTurnEachGame; }
int StartGameDialog::jishogiRule() const { return m_jishogiRule; }

// ============================================================
// パラメータ取得（OK押下時）
// ============================================================

void StartGameDialog::updateGameSettingsFromDialog()
{
    // 先手の設定を取得
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    if (player1Index == 0) {
        m_isHuman1 = true;
        m_isEngine1 = false;
        m_engineNumber1 = -1;
        m_engineName1 = "";
    } else {
        m_isHuman1 = false;
        m_isEngine1 = true;
        // インデックスから1を引いてエンジン番号を算出
        m_engineNumber1 = player1Index - 1;
        m_engineName1 = ui->comboBoxPlayer1->currentText();
    }
    m_humanName1 = ui->lineEditHumanName1->text();

    // 後手の設定を取得
    int player2Index = ui->comboBoxPlayer2->currentIndex();
    if (player2Index == 0) {
        m_isHuman2 = true;
        m_isEngine2 = false;
        m_engineNumber2 = -1;
        m_engineName2 = "";
    } else {
        m_isHuman2 = false;
        m_isEngine2 = true;
        m_engineNumber2 = player2Index - 1;
        m_engineName2 = ui->comboBoxPlayer2->currentText();
    }
    m_humanName2 = ui->lineEditHumanName2->text();

    m_basicTimeHour1 = ui->basicTimeHour1->text().toInt();
    m_basicTimeMinutes1 = ui->basicTimeMinutes1->text().toInt();
    m_byoyomiSec1 = ui->byoyomiSec1->text().toInt();
    m_addEachMoveSec1 = ui->addEachMoveSec1->text().toInt();

    // 「後手に異なる時間を設定」の有無で分岐
    if (ui->groupBoxSecondPlayerTimeSettings->isChecked()) {
        m_basicTimeHour2 = ui->basicTimeHour2->text().toInt();
        m_basicTimeMinutes2 = ui->basicTimeMinutes2->text().toInt();
        m_byoyomiSec2 = ui->byoyomiSec2->text().toInt();
        m_addEachMoveSec2 = ui->addEachMoveSec2->text().toInt();
    }
    else {
        // 先手の値をコピー
        m_basicTimeHour2 = m_basicTimeHour1;
        m_basicTimeMinutes2 = m_basicTimeMinutes1;
        m_byoyomiSec2 = m_byoyomiSec1;
        m_addEachMoveSec2 = m_addEachMoveSec1;
    }

    m_startingPositionName = ui->comboBoxStartingPosition->currentText();
    m_startingPositionNumber = ui->comboBoxStartingPosition->currentIndex();
    m_maxMoves = ui->spinBoxMaxMoves->value();
    m_consecutiveGames = ui->spinBoxConsecutiveGames->value();
    m_isShowHumanInFront = ui->checkBoxShowHumanInFront->isChecked();
    m_isAutoSaveKifu = ui->checkBoxAutoSaveKifu->isChecked();
    m_kifuSaveDir = ui->lineEditKifuSaveDir->text();
    m_isLoseOnTimeout = ui->checkBoxLoseOnTimeOut->isChecked();
    m_isSwitchTurnEachGame = ui->checkBoxSwitchTurnEachGame->isChecked();
    m_jishogiRule = ui->comboBoxJishogi->currentIndex();
}

// ============================================================
// エンジン設定ダイアログ
// ============================================================

void StartGameDialog::showEngineSettingsDialog(int playerNumber)
{
    QComboBox* comboBox = (playerNumber == 1) ? ui->comboBoxPlayer1 : ui->comboBoxPlayer2;
    int currentIndex = comboBox->currentIndex();

    // 人間が選択されている場合（インデックス0）は何もしない
    if (currentIndex == 0) {
        QMessageBox::information(this, tr("情報"), tr("人間が選択されています。"));
        return;
    }

    // インデックスから1を引いてエンジン番号を算出
    int engineNumber = currentIndex - 1;
    QString engineName = comboBox->currentText();

    if (engineName.isEmpty()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
    }
    else {
        ChangeEngineSettingsDialog dialog(this);
        dialog.setEngineNumber(engineNumber);
        dialog.setEngineName(engineName);
        dialog.setupEngineOptionsDialog();
        if (dialog.exec() == QDialog::Rejected) {
            return;
        }
    }
}

void StartGameDialog::onFirstPlayerSettingsClicked()
{
    showEngineSettingsDialog(1);
}

void StartGameDialog::onSecondPlayerSettingsClicked()
{
    showEngineSettingsDialog(2);
}

// ============================================================
// フォント設定
// ============================================================

void StartGameDialog::increaseFontSize()
{
    if (m_fontSize < MaxFontSize) {
        m_fontSize++;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

void StartGameDialog::decreaseFontSize()
{
    if (m_fontSize > MinFontSize) {
        m_fontSize--;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

void StartGameDialog::applyFontSize(int size)
{
    QFont font = this->font();
    font.setPointSize(size);
    this->setFont(font);
}

void StartGameDialog::loadFontSizeSettings()
{
    m_fontSize = SettingsService::startGameDialogFontSize();

    // 範囲内に収める
    if (m_fontSize < MinFontSize) m_fontSize = MinFontSize;
    if (m_fontSize > MaxFontSize) m_fontSize = MaxFontSize;

    applyFontSize(m_fontSize);
}

void StartGameDialog::saveFontSizeSettings()
{
    SettingsService::setStartGameDialogFontSize(m_fontSize);
}

// ============================================================
// 対局者選択UI更新
// ============================================================

void StartGameDialog::updatePlayerUI(int playerNumber, int index)
{
    // QStackedWidgetのページ: 0=人間名入力欄, 1=エンジン設定ボタン
    if (playerNumber == 1) {
        const int pageIndex = (index == 0) ? 0 : 1;
        ui->stackedWidgetPlayer1->setCurrentIndex(pageIndex);
        ui->stackedWidgetLabel1->setCurrentIndex(pageIndex);
    } else {
        const int pageIndex = (index == 0) ? 0 : 1;
        ui->stackedWidgetPlayer2->setCurrentIndex(pageIndex);
        ui->stackedWidgetLabel2->setCurrentIndex(pageIndex);
    }
}

void StartGameDialog::onPlayer1SelectionChanged(int index)
{
    updatePlayerUI(1, index);
    updateConsecutiveGamesEnabled();
}

void StartGameDialog::onPlayer2SelectionChanged(int index)
{
    updatePlayerUI(2, index);
    updateConsecutiveGamesEnabled();
}

void StartGameDialog::updateConsecutiveGamesEnabled()
{
    // 両方がエンジンの場合のみ連続対局を有効にする
    bool isEngine1 = (ui->comboBoxPlayer1->currentIndex() > 0);
    bool isEngine2 = (ui->comboBoxPlayer2->currentIndex() > 0);
    bool enableConsecutive = (isEngine1 && isEngine2);

    ui->spinBoxConsecutiveGames->setEnabled(enableConsecutive);

    // 人間が含まれる場合は連続対局数を1に固定
    if (!enableConsecutive) {
        ui->spinBoxConsecutiveGames->setValue(1);
    }
}

void StartGameDialog::onSelectKifuDirClicked()
{
    // 空の場合はドキュメントフォルダをデフォルトに
    QString currentDir = ui->lineEditKifuSaveDir->text();
    if (currentDir.isEmpty()) {
        currentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        tr("棋譜保存先を選択"),
        currentDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!selectedDir.isEmpty()) {
        ui->lineEditKifuSaveDir->setText(selectedDir);
    }
}
