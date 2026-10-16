/// @file startgamedialog.cpp
/// @brief 対局開始ダイアログクラスの実装

#include "startgamedialog.h"
#include "dialogfontscale.h"
#include "ui_startgamedialog.h"
#include "changeenginesettingsdialog.h"
#include "gamesettings.h"
#include "enginelistsettings.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include <QMessageBox>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QAbstractItemView>
#include <QTimer>
#include <utility>
#include "timecontrolvalidator.h"

// ============================================================
// 初期化
// ============================================================

StartGameDialog::StartGameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::StartGameDialog>())
{
    ui->setupUi(this);
    // スクロール位置にかかわらず文字サイズを戻せるよう、操作を固定フッターへ移す。
    ui->utilityLayout->removeWidget(ui->pushButtonFontSizeDown);
    ui->utilityLayout->removeWidget(ui->pushButtonFontSizeUp);
    ui->pushButtonFontSizeDown->setParent(this);
    ui->pushButtonFontSizeUp->setParent(this);
    ui->buttonLayout->insertWidget(0, ui->pushButtonFontSizeUp);
    ui->buttonLayout->insertWidget(0, ui->pushButtonFontSizeDown);

    // グリッドレイアウトの列ストレッチを設定（ラベル列は伸びず、コントロール列が伸びる）
    ui->gridLayoutPlayer1->setColumnStretch(0, 0);
    ui->gridLayoutPlayer1->setColumnStretch(1, 1);
    ui->gridLayoutPlayer2->setColumnStretch(0, 0);
    ui->gridLayoutPlayer2->setColumnStretch(1, 1);

    // ボタンの配色を設定
    ui->pushButtonEngineSettings1->setStyleSheet(ButtonStyles::secondaryNeutral());
    ui->pushButtonEngineSettings2->setStyleSheet(ButtonStyles::secondaryNeutral());
    ui->pushButtonSwapSides->setStyleSheet(ButtonStyles::secondaryNeutral());
    ui->pushButtonSaveSettingsOnly->setStyleSheet(ButtonStyles::secondaryNeutral());
    ui->pushButtonResetToDefault->setStyleSheet(ButtonStyles::dangerStop());
    ui->pushButtonSelectKifuDir->setStyleSheet(ButtonStyles::secondaryNeutral());

    // 補助操作でEnterの既定動作が変わらないよう、開始ボタンを明示する。
    const auto buttons = findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        button->setAutoDefault(false);
    }
    auto* startButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    startButton->setText(tr("対局開始"));
    startButton->setDefault(true);
    startButton->setToolTip(tr("設定を保存して対局を開始します。"));

    // 文字の大きさ（QScrollArea 配下を含めダイアログ内の全部品）
    DialogFontScale::Options fontOptions;
    fontOptions.minSize = MinFontSize;
    fontOptions.maxSize = MaxFontSize;
    fontOptions.decrease = ui->pushButtonFontSizeDown;
    fontOptions.increase = ui->pushButtonFontSizeUp;
    DialogFontScale::install(this, GameSettings::startGameDialogFontSize, fontOptions);

    loadEngineConfigurations();
    populatePlayerComboBoxes();
    loadGameSettings();
    connectSignalsAndSlots();
    updateTimeSettingsTitle();
    updateKifuSaveEnabled();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, GameSettings::startGameDialogSize());
}

StartGameDialog::~StartGameDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::startGameDialogSize);
}

// ============================================================
// シグナル接続
// ============================================================

void StartGameDialog::connectSignalsAndSlots()
{
    // エンジン設定ボタン・先後入れ替え・リセット・保存
    connect(ui->pushButtonEngineSettings1, &QPushButton::clicked, this,
            &StartGameDialog::onFirstPlayerSettingsClicked);
    connect(ui->pushButtonEngineSettings2, &QPushButton::clicked, this,
            &StartGameDialog::onSecondPlayerSettingsClicked);
    connect(ui->pushButtonSwapSides, &QPushButton::clicked, this, &StartGameDialog::swapSides);
    connect(ui->pushButtonResetToDefault, &QPushButton::clicked, this, &StartGameDialog::resetSettingsToDefault);
    connect(ui->pushButtonSaveSettingsOnly, &QPushButton::clicked, this, &StartGameDialog::saveSettingsOnly);

    // OK/キャンセル: OKはパラメータ取得→時間設定の検証→設定保存→ダイアログ閉じの順で実行される
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::onStartRequested);
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

    // 対局者種別（人間/エンジン）変更時にUIを切り替える
    connect(ui->comboBoxPlayer1, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer1SelectionChanged);
    connect(ui->comboBoxPlayer2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer2SelectionChanged);

    // 棋譜保存先ディレクトリ選択
    connect(ui->pushButtonSelectKifuDir, &QPushButton::clicked, this, &StartGameDialog::onSelectKifuDirClicked);
    connect(ui->checkBoxAutoSaveKifu, &QCheckBox::toggled, this, &StartGameDialog::updateKifuSaveEnabled);
    connect(ui->groupBoxSecondPlayerTimeSettings, &QGroupBox::toggled,
            this, &StartGameDialog::updateTimeSettingsTitle);
    connect(ui->spinBoxConsecutiveGames, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::updateConsecutiveGamesEnabled);
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
    m_engineList.clear();
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (const auto& entry : engines) {
        Engine engine;
        engine.name = entry.name;
        engine.path = entry.path;
        m_engineList.append(engine);
    }
}

void StartGameDialog::populatePlayerComboBoxes()
{
    ui->comboBoxPlayer1->clear();
    ui->comboBoxPlayer2->clear();

    // インデックス0は「人間」
    ui->comboBoxPlayer1->addItem(tr("人間"));
    ui->comboBoxPlayer2->addItem(tr("人間"));

    // インデックス1以降にエンジンを追加
    foreach (const Engine& engine, m_engineList) {
        ui->comboBoxPlayer1->addItem(engine.name);
        ui->comboBoxPlayer2->addItem(engine.name);
    }
}

// ============================================================
// 選んだ値
// ============================================================

void StartGameDialog::forceCurrentPositionSelection()
{
    ui->comboBoxStartingPosition->setCurrentIndex(0);
}

const QList<StartGameDialog::Engine> &StartGameDialog::engineList() const { return m_engineList; }

StartGameDialogData StartGameDialog::data() const
{
    StartGameDialogData data;
    // 対局者（コンボの 0 は人間、1 以降は登録エンジン）
    const int player1 = ui->comboBoxPlayer1->currentIndex();
    data.isHuman1 = player1 <= 0;
    data.isEngine1 = !data.isHuman1;
    data.engineNumber1 = data.isEngine1 ? player1 - 1 : -1;
    data.engineName1 = data.isEngine1 ? ui->comboBoxPlayer1->currentText() : QString();
    data.humanName1 = ui->lineEditHumanName1->text();
    const int player2 = ui->comboBoxPlayer2->currentIndex();
    data.isHuman2 = player2 <= 0;
    data.isEngine2 = !data.isHuman2;
    data.engineNumber2 = data.isEngine2 ? player2 - 1 : -1;
    data.engineName2 = data.isEngine2 ? ui->comboBoxPlayer2->currentText() : QString();
    data.humanName2 = ui->lineEditHumanName2->text();
    for (const Engine& engine : m_engineList) data.engineList.append({engine.path});

    // 持ち時間（「後手に異なる時間を設定」がなければ先手と同じ）
    data.basicTimeHour1 = ui->basicTimeHour1->value();
    data.basicTimeMinutes1 = ui->basicTimeMinutes1->value();
    data.byoyomiSec1 = ui->byoyomiSec1->value();
    data.addEachMoveSec1 = ui->addEachMoveSec1->value();
    const bool separate = ui->groupBoxSecondPlayerTimeSettings->isChecked();
    data.basicTimeHour2 = separate ? ui->basicTimeHour2->value() : data.basicTimeHour1;
    data.basicTimeMinutes2 = separate ? ui->basicTimeMinutes2->value() : data.basicTimeMinutes1;
    data.byoyomiSec2 = separate ? ui->byoyomiSec2->value() : data.byoyomiSec1;
    data.addEachMoveSec2 = separate ? ui->addEachMoveSec2->value() : data.addEachMoveSec1;
    data.isLoseOnTimeout = ui->checkBoxLoseOnTimeOut->isChecked();

    data.startingPositionNumber = ui->comboBoxStartingPosition->currentIndex();
    data.maxMoves = ui->spinBoxMaxMoves->value();
    data.autoSaveKifu = ui->checkBoxAutoSaveKifu->isChecked();
    data.kifuSaveDir = ui->lineEditKifuSaveDir->text();
    data.isShowHumanInFront = ui->checkBoxShowHumanInFront->isChecked();
    data.consecutiveGames = ui->spinBoxConsecutiveGames->value();
    data.isSwitchTurnEachGame = ui->checkBoxSwitchTurnEachGame->isEnabled()
                                && ui->checkBoxSwitchTurnEachGame->isChecked();
    return data;
}

// ============================================================
// パラメータ取得（OK押下時）
// ============================================================

void StartGameDialog::onStartRequested()
{
    // 開始できない時間設定なら、設定を保存せずにダイアログを開いたままにする。
    if (!confirmTimeSettings()) return;
    saveGameSettings();
    accept();
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

    QString engineName = comboBox->currentText();

    if (engineName.isEmpty()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
    }
    else {
        ChangeEngineSettingsDialog dialog(this);
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
    ui->label_19->setEnabled(enableConsecutive);

    // 人間が含まれる場合は連続対局数を1に固定
    if (!enableConsecutive) {
        ui->spinBoxConsecutiveGames->setValue(1);
    }
    ui->checkBoxSwitchTurnEachGame->setEnabled(
        enableConsecutive && ui->spinBoxConsecutiveGames->value() > 1);
}

void StartGameDialog::updateKifuSaveEnabled()
{
    const bool enabled = ui->checkBoxAutoSaveKifu->isChecked();
    ui->lineEditKifuSaveDir->setEnabled(enabled);
    ui->pushButtonSelectKifuDir->setEnabled(enabled);
}

void StartGameDialog::updateTimeSettingsTitle()
{
    ui->groupBox_7->setTitle(ui->groupBoxSecondPlayerTimeSettings->isChecked()
                                ? tr("先手／下手の時間設定") : tr("共通の時間設定"));
}

void StartGameDialog::saveSettingsOnly()
{
    saveGameSettings();
    ui->labelSaveStatus->setText(tr("設定を保存しました"));
    QTimer::singleShot(3000, ui->labelSaveStatus, &QLabel::clear);
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

// ============================================================
// 対局開始ダイアログ - 設定保存・読込・リセット
// ============================================================

namespace {
namespace Saved = GameSettings::StartGame;

QString defaultKifuSaveDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

int savedPlayerIndex(const Saved::Player& saved, const QList<StartGameDialog::Engine>& engines)
{
    if (saved.isHuman()) return 0;

    const int number = saved.engineNumber();
    const QString name = saved.engineName();
    const QString path = saved.enginePath();
    const auto matches = [&](const StartGameDialog::Engine& engine) {
        return !path.isEmpty() ? engine.path == path : engine.name == name;
    };
    if (number >= 0 && number < engines.size()
        && ((name.isEmpty() && path.isEmpty()) || matches(engines.at(number)))) {
        return number + 1;
    }
    // 登録順が変わっても同じエンジンを選び、削除済みなら人間へ戻す。
    for (qsizetype i = 0; i < engines.size(); ++i) {
        if (matches(engines.at(i))) return static_cast<int>(i) + 1;
    }
    return 0;
}

/// 対局者の選択を保存する。インデックス0は「人間」、1以上はエンジン
void savePlayer(const Saved::Player& saved, int playerIndex, const QString& humanName,
                const QString& engineName, const QList<StartGameDialog::Engine>& engines)
{
    const bool isHuman = (playerIndex <= 0);
    saved.isHuman.set(isHuman);
    saved.isEngine.set(!isHuman);
    saved.humanName.set(humanName);
    if (!isHuman) {
        saved.engineNumber.set(playerIndex - 1);
        saved.engineName.set(engineName);
        saved.enginePath.set(engines.at(playerIndex - 1).path);
    }
}
} // namespace

// ============================================================
// 設定保存・読込
// ============================================================

void StartGameDialog::saveGameSettings()
{
    // 先手／下手の設定を保存
    savePlayer(Saved::player1, ui->comboBoxPlayer1->currentIndex(), ui->lineEditHumanName1->text(),
               ui->comboBoxPlayer1->currentText(), m_engineList);
    Saved::player1.basicTimeHour.set(ui->basicTimeHour1->value());
    Saved::player1.basicTimeMinutes.set(ui->basicTimeMinutes1->value());
    Saved::player1.byoyomiSec.set(ui->byoyomiSec1->value());
    Saved::player1.addEachMoveSec.set(ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存
    Saved::separateSecondPlayerTime.set(ui->groupBoxSecondPlayerTimeSettings->isChecked());
    savePlayer(Saved::player2, ui->comboBoxPlayer2->currentIndex(), ui->lineEditHumanName2->text(),
               ui->comboBoxPlayer2->currentText(), m_engineList);
    Saved::player2.basicTimeHour.set(ui->basicTimeHour2->value());
    Saved::player2.basicTimeMinutes.set(ui->basicTimeMinutes2->value());
    Saved::player2.byoyomiSec.set(ui->byoyomiSec2->value());
    Saved::player2.addEachMoveSec.set(ui->addEachMoveSec2->value());

    Saved::startingPositionName.set(ui->comboBoxStartingPosition->currentText());
    Saved::startingPositionNumber.set(ui->comboBoxStartingPosition->currentIndex());
    Saved::maxMoves.set(ui->spinBoxMaxMoves->value());
    Saved::consecutiveGames.set(ui->spinBoxConsecutiveGames->value());
    Saved::showHumanInFront.set(ui->checkBoxShowHumanInFront->isChecked());
    Saved::autoSaveKifu.set(ui->checkBoxAutoSaveKifu->isChecked());
    Saved::kifuSaveDir.set(ui->lineEditKifuSaveDir->text());
    Saved::loseOnTimeout.set(ui->checkBoxLoseOnTimeOut->isChecked());
    Saved::switchTurnEachGame.set(ui->checkBoxSwitchTurnEachGame->isChecked());
    Saved::jishogiRule.set(ui->comboBoxJishogi->currentIndex());
}

void StartGameDialog::loadGameSettings()
{
    // 先手／下手の設定を読み込む
    ui->lineEditHumanName1->setText(Saved::player1.humanName.valueOr(tr("You")));
    ui->comboBoxPlayer1->setCurrentIndex(savedPlayerIndex(Saved::player1, m_engineList));
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(Saved::player1.basicTimeHour());
    ui->basicTimeMinutes1->setValue(Saved::player1.basicTimeMinutes());
    ui->byoyomiSec1->setValue(Saved::player1.byoyomiSec());
    ui->addEachMoveSec1->setValue(Saved::player1.addEachMoveSec());

    // 後手／上手の設定を読み込む
    ui->groupBoxSecondPlayerTimeSettings->setChecked(Saved::separateSecondPlayerTime());

    ui->lineEditHumanName2->setText(Saved::player2.humanName.valueOr(tr("You")));
    ui->comboBoxPlayer2->setCurrentIndex(savedPlayerIndex(Saved::player2, m_engineList));
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(Saved::player2.basicTimeHour());
    ui->basicTimeMinutes2->setValue(Saved::player2.basicTimeMinutes());
    ui->byoyomiSec2->setValue(Saved::player2.byoyomiSec());
    ui->addEachMoveSec2->setValue(Saved::player2.addEachMoveSec());

    const int savedStartingIndex = Saved::startingPositionNumber();
    if (savedStartingIndex >= 0 && savedStartingIndex < ui->comboBoxStartingPosition->count()) {
        ui->comboBoxStartingPosition->setCurrentIndex(savedStartingIndex);
    } else {
        ui->comboBoxStartingPosition->setCurrentText(Saved::startingPositionName());
    }
    ui->spinBoxMaxMoves->setValue(Saved::maxMoves());
    ui->spinBoxConsecutiveGames->setValue(Saved::consecutiveGames());
    ui->checkBoxShowHumanInFront->setChecked(Saved::showHumanInFront());
    ui->checkBoxAutoSaveKifu->setChecked(Saved::autoSaveKifu());
    // デフォルトの棋譜保存先はドキュメントフォルダ
    ui->lineEditKifuSaveDir->setText(Saved::kifuSaveDir.valueOr(defaultKifuSaveDir()));
    ui->checkBoxLoseOnTimeOut->setChecked(Saved::loseOnTimeout());
    ui->checkBoxSwitchTurnEachGame->setChecked(Saved::switchTurnEachGame());
    ui->comboBoxJishogi->setCurrentIndex(Saved::jishogiRule());

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    ui->groupBoxSecondPlayerTimeSettings->setChecked(Saved::separateSecondPlayerTime.defaultValue());
    // 先手: 人間をデフォルトに
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);
    ui->lineEditHumanName1->setText(tr("You"));
    ui->basicTimeHour1->setValue(Saved::player1.basicTimeHour.defaultValue());
    ui->basicTimeMinutes1->setValue(Saved::player1.basicTimeMinutes.defaultValue());
    ui->byoyomiSec1->setValue(Saved::player1.byoyomiSec.defaultValue());
    ui->addEachMoveSec1->setValue(Saved::player1.addEachMoveSec.defaultValue());

    // 後手: 最初のエンジンをデフォルトに（なければ人間）
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }
    ui->lineEditHumanName2->setText(tr("You"));
    ui->basicTimeHour2->setValue(Saved::player2.basicTimeHour.defaultValue());
    ui->basicTimeMinutes2->setValue(Saved::player2.basicTimeMinutes.defaultValue());
    ui->byoyomiSec2->setValue(Saved::player2.byoyomiSec.defaultValue());
    ui->addEachMoveSec2->setValue(Saved::player2.addEachMoveSec.defaultValue());

    ui->comboBoxStartingPosition->setCurrentIndex(Saved::startingPositionNumber.defaultValue());
    ui->spinBoxMaxMoves->setValue(Saved::maxMoves.defaultValue());
    ui->spinBoxConsecutiveGames->setValue(Saved::consecutiveGames.defaultValue());
    ui->checkBoxShowHumanInFront->setChecked(Saved::showHumanInFront.defaultValue());
    ui->checkBoxAutoSaveKifu->setChecked(Saved::autoSaveKifu.defaultValue());
    ui->lineEditKifuSaveDir->setText(defaultKifuSaveDir());
    ui->checkBoxLoseOnTimeOut->setChecked(Saved::loseOnTimeout.defaultValue());
    ui->checkBoxSwitchTurnEachGame->setChecked(Saved::switchTurnEachGame.defaultValue());
    ui->comboBoxJishogi->setCurrentIndex(Saved::jishogiRule.defaultValue());
}

// ============================================================
// 対局開始ダイアログ - 時間設定の確認
// ============================================================

bool StartGameDialog::confirmTimeSettings()
{
    using TimeControlValidator::Issue;
    const StartGameDialogData d = data();
    const TimeControlValidator::PlayerTime p1{d.basicTimeHour1, d.basicTimeMinutes1, d.byoyomiSec1, d.addEachMoveSec1};
    const TimeControlValidator::PlayerTime p2{d.basicTimeHour2, d.basicTimeMinutes2, d.byoyomiSec2, d.addEachMoveSec2};
    const bool hasEngine = d.isEngine1 || d.isEngine2;
    const Issue issue = TimeControlValidator::validate(p1, p2, hasEngine);

    QString message;
    switch (issue) {
    case Issue::None:
        return true;
    case Issue::Player1HasNoTime:
    case Issue::Player2HasNoTime: {
        const QString side = (issue == Issue::Player1HasNoTime) ? tr("先手／下手") : tr("後手／上手");
        message = hasEngine
            ? tr("%1の持ち時間・秒読み・加算がすべて0秒です。\n"
                 "片方の対局者だけを時間無制限にすることはできません。"
                 "両方の対局者に時間を設定してください。").arg(side)
            : tr("%1の持ち時間・秒読み・加算がすべて0秒です。\n"
                 "片方の対局者だけを時間無制限にすることはできません。"
                 "両方の対局者に時間を設定するか、時間無制限で対局する場合は両方とも0秒にしてください。").arg(side);
        break;
    }
    case Issue::EngineWithoutLimit:
        message = tr("エンジンが参加する対局は、時間無制限（持ち時間・秒読み・加算がすべて0秒）にできません。\n"
                     "エンジンは使える時間を0秒と受け取り、ほとんど考えずに指してしまいます。"
                     "持ち時間・秒読み・加算のいずれかを設定してください。");
        break;
    }
    QMessageBox::warning(this, tr("時間設定"), message);
    return false;
}
