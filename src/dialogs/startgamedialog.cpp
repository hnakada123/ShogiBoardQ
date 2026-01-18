#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include <QSettings>
#include <QMessageBox>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

using namespace EngineSettingsConstants;

// コンストラクタ
StartGameDialog::StartGameDialog(QWidget *parent) : QDialog(parent), ui(new Ui::StartGameDialog), m_fontSize(DefaultFontSize)
{
    // UIをセットアップする。
    ui->setupUi(this);

    // グリッドレイアウトの列ストレッチを設定（ラベル列は伸びず、コントロール列が伸びる）
    ui->gridLayoutPlayer1->setColumnStretch(0, 0);
    ui->gridLayoutPlayer1->setColumnStretch(1, 1);
    ui->gridLayoutPlayer2->setColumnStretch(0, 0);
    ui->gridLayoutPlayer2->setColumnStretch(1, 1);

    // 文字サイズ設定を読み込んで適用する。
    loadFontSizeSettings();

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    loadEngineConfigurations();

    // 対局者コンボボックスにエンジンリストを反映する。
    populatePlayerComboBoxes();

    // 設定ファイルから対局設定を読み込みGUIに反映する。
    loadGameSettings();

    // GUIのシグナルとスロットを接続する。
    connectSignalsAndSlots();
}

// デストラクタ
StartGameDialog::~StartGameDialog()
{
    // UIを削除する。
    delete ui;
}

// UIのシグナルとスロットを接続する。
void StartGameDialog::connectSignalsAndSlots() const
{
    // 先手／下手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    connect(ui->pushButtonEngineSettings1, &QPushButton::clicked, this,
            &StartGameDialog::onFirstPlayerSettingsClicked);

    // 後手／上手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    connect(ui->pushButtonEngineSettings2, &QPushButton::clicked, this,
            &StartGameDialog::onSecondPlayerSettingsClicked);

    // 先後入れ替えボタンが押された場合、先後を入れ替える。
    connect(ui->pushButtonSwapSides, &QPushButton::clicked, this, &StartGameDialog::swapSides);

    // 設定を初期値にリセットする。
    connect(ui->pushButtonResetToDefault, &QPushButton::clicked, this, &StartGameDialog::resetSettingsToDefault);

    // 設定を保存する。
    connect(ui->pushButtonSaveSettingsOnly, &QPushButton::clicked, this, &StartGameDialog::saveGameSettings);

    // OKボタンが押された場合、ダイアログを受け入れたとして閉じる動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::accept);

    // OKボタンが押された場合、 設定ファイルに対局設定を保存する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::saveGameSettings);

    // OKボタンが押された場合、対局ダイアログ内の各パラメータを取得する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::updateGameSettingsFromDialog);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &StartGameDialog::reject);

    // 先手／下手の秒読みの値が変更された場合、1手ごとの加算時間を0に設定する。
    connect(ui->byoyomiSec1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleByoyomiSecChanged);

    // 先手／下手の1手ごとの加算時間の値が変更された場合、秒読みを0に設定する。
    connect(ui->addEachMoveSec1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleAddEachMoveSecChanged);

    // 後手／上手の秒読みの値が変更された場合、1手ごとの加算時間を0に設定する。
    connect(ui->byoyomiSec2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleByoyomiSecChanged);

    // 後手／上手の1手ごとの加算時間の値が変更された場合、秒読みを0に設定する。
    connect(ui->addEachMoveSec2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::handleAddEachMoveSecChanged);

    // 文字サイズを大きくするボタンが押された場合、文字サイズを大きくする。
    connect(ui->pushButtonFontSizeUp, &QPushButton::clicked, this, &StartGameDialog::increaseFontSize);

    // 文字サイズを小さくするボタンが押された場合、文字サイズを小さくする。
    connect(ui->pushButtonFontSizeDown, &QPushButton::clicked, this, &StartGameDialog::decreaseFontSize);

    // 先手／下手の対局者選択が変更された場合、UIを更新する。
    connect(ui->comboBoxPlayer1, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer1SelectionChanged);

    // 後手／上手の対局者選択が変更された場合、UIを更新する。
    connect(ui->comboBoxPlayer2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StartGameDialog::onPlayer2SelectionChanged);

    // 棋譜保存ディレクトリ選択ボタンが押された場合、ディレクトリ選択ダイアログを表示する。
    connect(ui->pushButtonSelectKifuDir, &QPushButton::clicked, this, &StartGameDialog::onSelectKifuDirClicked);
}

// 秒読みの値が変更された場合、1手ごとの加算時間を0に設定する。
void StartGameDialog::handleByoyomiSecChanged(int value)
{
    // 秒読みの値が0より大きい場合
    if (value > 0) {
        // 両対局者の1手ごとの加算時間を0に設定する。
        ui->addEachMoveSec1->setValue(0);
        ui->addEachMoveSec2->setValue(0);
    }
}

// 1手ごとの加算時間の値が変更された場合、秒読みを0に設定する。
void StartGameDialog::handleAddEachMoveSecChanged(int value)
{
    // 1手ごとの加算時間の値が0より大きい場合
    if (value > 0) {
        // 両対局者の秒読みの値を0に設定する。
        ui->byoyomiSec1->setValue(0);
        ui->byoyomiSec2->setValue(0);
    }
}


// 設定ファイルに対局設定を保存する。
void StartGameDialog::saveGameSettings()
{
    // 設定ファイルを操作するためのQSettingsオブジェクトを作成する。ファイル名とフォーマットを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 設定の書き込みを開始する。
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を保存する。
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

    // 後手／上手の設定を保存する。
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

    // 開始局面の設定を保存する。
    settings.setValue("startingPositionName", ui->comboBoxStartingPosition->currentText());
    settings.setValue("startingPositionNumber", ui->comboBoxStartingPosition->currentIndex());

    // 最大手数を保存する。
    settings.setValue("maxMoves", ui->spinBoxMaxMoves->value());

    // 連続対局数を保存する。
    settings.setValue("consecutiveGames", ui->spinBoxConsecutiveGames->value());

    // 人を手前に表示するかどうかのフラグを保存する。
    settings.setValue("isShowHumanInFront", ui->checkBoxShowHumanInFront->isChecked());

    // 棋譜の自動保存フラグを保存する。
    settings.setValue("isAutoSaveKifu", ui->checkBoxAutoSaveKifu->isChecked());

    // 棋譜の保存ディレクトリを保存する。
    settings.setValue("kifuSaveDir", ui->lineEditKifuSaveDir->text());

    // 時間切れを負けにするかどうかのフラグを保存する。
    settings.setValue("isLoseOnTimeout", ui->checkBoxLoseOnTimeOut->isChecked());

    // 1局ごとに手番を入れ替えるかどうかのフラグを保存する。
    settings.setValue("isSwitchTurnEachGame", ui->checkBoxSwitchTurnEachGame->isChecked());

    // 持将棋ルールを保存する。
    settings.setValue("jishogiRule", ui->comboBoxJishogi->currentIndex());

    // 設定の書き込みを終了する。
    settings.endGroup();
}

// 設定ファイルから対局設定を読み込みGUIに反映する。
void StartGameDialog::loadGameSettings()
{
    // 設定ファイルを操作するためのQSettingsオブジェクトを作成する。ファイル名とフォーマットを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 設定の読み込みを開始する。
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を読み込む。
    bool isHuman1 = settings.value("isHuman1", true).toBool();
    ui->lineEditHumanName1->setText(settings.value("humanName1", tr("You")).toString());
    if (isHuman1) {
        // 人間の場合、インデックス0を選択
        ui->comboBoxPlayer1->setCurrentIndex(0);
    } else {
        // エンジンの場合、エンジン番号+1をインデックスとして選択
        int engineNumber = settings.value("engineNumber1", 0).toInt();
        ui->comboBoxPlayer1->setCurrentIndex(engineNumber + 1);
    }
    // UIを更新（人間名欄/エンジン設定ボタンの表示切替）
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(settings.value("basicTimeHour1", 0).toInt());
    ui->basicTimeMinutes1->setValue(settings.value("basicTimeMinutes1", 0).toInt());
    ui->byoyomiSec1->setValue(settings.value("byoyomiSec1", 0).toInt());
    ui->addEachMoveSec1->setValue(settings.value("addEachMoveSec1", 0).toInt());

    // 後手／上手の設定を読み込む。
    bool isChecked = settings.value("isGroupBoxSecondPlayerTimeSettingsChecked", false).toBool();
    ui->groupBoxSecondPlayerTimeSettings->setChecked(isChecked);

    bool isHuman2 = settings.value("isHuman2", false).toBool();
    ui->lineEditHumanName2->setText(settings.value("humanName2", tr("You")).toString());
    if (isHuman2) {
        // 人間の場合、インデックス0を選択
        ui->comboBoxPlayer2->setCurrentIndex(0);
    } else {
        // エンジンの場合、エンジン番号+1をインデックスとして選択
        int engineNumber = settings.value("engineNumber2", 0).toInt();
        ui->comboBoxPlayer2->setCurrentIndex(engineNumber + 1);
    }
    // UIを更新（人間名欄/エンジン設定ボタンの表示切替）
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(settings.value("basicTimeHour2", 0).toInt());
    ui->basicTimeMinutes2->setValue(settings.value("basicTimeMinutes2", 0).toInt());
    ui->byoyomiSec2->setValue(settings.value("byoyomiSec2", 0).toInt());
    ui->addEachMoveSec2->setValue(settings.value("addEachMoveSec2", 0).toInt());

    // 開始局面の設定を読み込む。
    ui->comboBoxStartingPosition->setCurrentText(settings.value("startingPositionName", "平手").toString());
    ui->comboBoxStartingPosition->setCurrentIndex(settings.value("startingPositionNumber", 1).toInt());

    // 最大手数を読み込む。
    ui->spinBoxMaxMoves->setValue(settings.value("maxMoves", 1000).toInt());

    // 連続対局数を読み込む。
    ui->spinBoxConsecutiveGames->setValue(settings.value("consecutiveGames", 1).toInt());

    // 人を手前に表示するかどうかのフラグを読み込む。
    ui->checkBoxShowHumanInFront->setChecked(settings.value("isShowHumanInFront", true).toBool());

    // 棋譜の自動保存フラグを読み込む。
    ui->checkBoxAutoSaveKifu->setChecked(settings.value("isAutoSaveKifu", false).toBool());

    // 棋譜の保存ディレクトリを読み込む。デフォルトはドキュメントフォルダ。
    QString defaultKifuDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    ui->lineEditKifuSaveDir->setText(settings.value("kifuSaveDir", defaultKifuDir).toString());

    // 時間切れを負けにするかどうかのフラグを読み込む。
    ui->checkBoxLoseOnTimeOut->setChecked(settings.value("isLoseOnTimeout", true).toBool());

    // 1局ごとに手番を入れ替えるかどうかのフラグを読み込む。
    ui->checkBoxSwitchTurnEachGame->setChecked(settings.value("isSwitchTurnEachGame", false).toBool());

    // 持将棋ルールを読み込む。
    ui->comboBoxJishogi->setCurrentIndex(settings.value("jishogiRule", 0).toInt());

    // 設定の読み込みを終了する。
    settings.endGroup();

    // 連続対局スピンボックスの有効/無効を更新する。
    updateConsecutiveGamesEnabled();
}

// 設定を初期値にリセットする。
void StartGameDialog::resetSettingsToDefault()
{
    // 先手／下手の設定を初期値に戻す。
    // 人間をデフォルトに設定する（インデックス0）。
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);

    // 人間の名前をデフォルトに設定する。
    ui->lineEditHumanName1->setText(tr("You"));

    // 持ち時間のデフォルト値を設定する。
    ui->basicTimeHour1->setValue(0);
    ui->basicTimeMinutes1->setValue(30);

    // 秒読みのデフォルト値を設定する。
    ui->byoyomiSec1->setValue(30);

    // 1手ごとの加算時間をデフォルトに設定する。
    ui->addEachMoveSec1->setValue(0);

    // 後手／上手の設定を初期値に戻す。
    // エンジン（最初のエンジン）をデフォルトに設定する（インデックス1）。
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        // エンジンが登録されていない場合は人間に設定
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }

    // 人間の名前をデフォルトに設定する。
    ui->lineEditHumanName2->setText(tr("You"));

    // 持ち時間のデフォルト値を設定する。
    ui->basicTimeHour2->setValue(0);
    ui->basicTimeMinutes2->setValue(30);

    // 秒読みのデフォルト値を設定する。
    ui->byoyomiSec2->setValue(30);

    // 1手ごとの加算時間をデフォルトに設定する。
    ui->addEachMoveSec2->setValue(0);

    // 開始局面の設定を初期値に戻す。
    // 平手をデフォルトに設定する。
    ui->comboBoxStartingPosition->setCurrentIndex(1);

    // 最大手数、連続対局数を初期値に戻す。
    ui->spinBoxMaxMoves->setValue(1000);
    ui->spinBoxConsecutiveGames->setValue(1);

    // その他のチェックボックスの初期値を設定する。
    // 人を手前に表示する。
    ui->checkBoxShowHumanInFront->setChecked(true);

    // 棋譜の自動保存はデフォルトでオフにする。
    ui->checkBoxAutoSaveKifu->setChecked(false);

    // 棋譜の保存ディレクトリをデフォルト（ドキュメントフォルダ）に設定する。
    ui->lineEditKifuSaveDir->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    // 時間切れを負けにする。
    ui->checkBoxLoseOnTimeOut->setChecked(true);

    // 手番の入れ替えはデフォルトでオフにする。
    ui->checkBoxSwitchTurnEachGame->setChecked(false);

    // 持将棋ルールのデフォルトは「なし」にする。
    ui->comboBoxJishogi->setCurrentIndex(0);
}

// 先後入れ替えボタンが押された場合、先後を入れ替える。
void StartGameDialog::swapSides()
{
    // 対局者コンボボックスのインデックスを入れ替える。
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    int player2Index = ui->comboBoxPlayer2->currentIndex();
    ui->comboBoxPlayer1->setCurrentIndex(player2Index);
    ui->comboBoxPlayer2->setCurrentIndex(player1Index);

    // UIを更新（人間名欄/エンジン設定ボタンの表示切替）
    updatePlayerUI(1, player2Index);
    updatePlayerUI(2, player1Index);

    // 人間の名前を入れ替える。
    QString temp = ui->lineEditHumanName1->text();
    ui->lineEditHumanName1->setText(ui->lineEditHumanName2->text());
    ui->lineEditHumanName2->setText(temp);

    // 「後手／上手に異なる時間を設定」にチェックが入っている場合
    if (ui->groupBoxSecondPlayerTimeSettings->isChecked()) {
        // 持ち時間を入れ替える。
        int tempInt = ui->basicTimeHour1->value();
        ui->basicTimeHour1->setValue(ui->basicTimeHour2->value());
        ui->basicTimeHour2->setValue(tempInt);

        tempInt = ui->basicTimeMinutes1->value();
        ui->basicTimeMinutes1->setValue(ui->basicTimeMinutes2->value());
        ui->basicTimeMinutes2->setValue(tempInt);

        // 秒読み時間を入れ替える。
        tempInt = ui->byoyomiSec1->value();
        ui->byoyomiSec1->setValue(ui->byoyomiSec2->value());
        ui->byoyomiSec2->setValue(tempInt);

        // 1手ごとの時間加算を入れ替える。
        tempInt = ui->addEachMoveSec1->value();
        ui->addEachMoveSec1->setValue(ui->addEachMoveSec2->value());
        ui->addEachMoveSec2->setValue(tempInt);
    }
}

// 設定ファイルからエンジンの名前とディレクトリを読み込む。
void StartGameDialog::loadEngineConfigurations()
{
    // 現在のディレクトリをアプリケーションのディレクトリに設定する
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    int size = settings.beginReadArray("Engines");
    engineList.clear(); // 現在のエンジンリストをクリア

    // エンジンの数だけループする。
    for (int i = 0; i < size; ++i) {
        // 現在のインデックスで配列の要素を設定する。
        settings.setArrayIndex(i);

        // エンジン名とディレクトリを取得する。
        Engine engine;
        engine.name = settings.value("name").toString();
        engine.path = settings.value("path").toString();

        // 対局ダイアログのエンジン選択リストにエンジン名を追加する。
        engineList.append(engine);
    }

    // エンジン名のグループの配列の読み込みを終了する。
    settings.endArray();
}

// UIに対局者リスト（人間＋エンジン）を反映する。
void StartGameDialog::populatePlayerComboBoxes()
{
    // コンボボックスをクリアする。
    ui->comboBoxPlayer1->clear();
    ui->comboBoxPlayer2->clear();

    // 最初に「人間」を追加する（インデックス0）。
    ui->comboBoxPlayer1->addItem(tr("人間"));
    ui->comboBoxPlayer2->addItem(tr("人間"));

    // エンジンリストの各エンジンをコンボボックスに追加する（インデックス1以降）。
    foreach (const Engine& engine, engineList) {
        ui->comboBoxPlayer1->addItem(engine.name);
        ui->comboBoxPlayer2->addItem(engine.name);
    }
}

// 先手／下手が人間かエンジンかを示すフラグを取得する。
bool StartGameDialog::isHuman1() const
{
    return m_isHuman1;
}

// 後手／上手が人間かエンジンかを示すフラグを取得する。
bool StartGameDialog::isHuman2() const
{
    return m_isHuman2;
}

// 先手／下手がエンジンかを示すフラグを取得する。
bool StartGameDialog::isEngine1() const
{
    return m_isEngine1;
}

// 後手／上手がエンジンかを示すフラグを取得する。
bool StartGameDialog::isEngine2() const
{
    return m_isEngine2;
}

// 先手／下手のエンジン名を取得する。
const QString& StartGameDialog::engineName1() const
{
    return m_engineName1;
}

// 後手／上手のエンジン名を取得する。
const QString& StartGameDialog::engineName2() const
{
    return m_engineName2;
}

// 先手／下手の人間名を取得する。
const QString& StartGameDialog::humanName1() const
{
    return m_humanName1;
}

// 後手／上手の人間名を取得する。
const QString& StartGameDialog::humanName2() const
{
    return m_humanName2;
}

// 先手／下手のエンジン番号を取得する。
int StartGameDialog::engineNumber1() const
{
    return m_engineNumber1;
}

// 後手／上手のエンジン番号を取得する。
int StartGameDialog::engineNumber2() const
{
    return m_engineNumber2;
}

// 対局者1の持ち時間（時間）を取得する。
int StartGameDialog::basicTimeHour1() const
{
    return m_basicTimeHour1;
}

// 対局者1の持ち時間（分）を取得する。
int StartGameDialog::basicTimeMinutes1() const
{
    return m_basicTimeMinutes1;
}

// 対局者1の1手ごとの加算（秒）を取得する。
int StartGameDialog::addEachMoveSec1() const
{
    return m_addEachMoveSec1;
}

// 対局者1の秒読みの時間（秒）を取得する。
int StartGameDialog::byoyomiSec1() const
{
    return m_byoyomiSec1;
}

// 対局者2の持ち時間（時間）を取得する。
int StartGameDialog::basicTimeHour2() const
{
    return m_basicTimeHour2;
}

// 対局者2の持ち時間（分）を取得する。
int StartGameDialog::basicTimeMinutes2() const
{
    return m_basicTimeMinutes2;
}

// 対局者2の1手ごとの加算（秒）を取得する。
int StartGameDialog::addEachMoveSec2() const
{
    return m_addEachMoveSec2;
}

// 対局者2の秒読みの時間（秒）を取得する。
int StartGameDialog::byoyomiSec2() const
{
    return m_byoyomiSec2;
}

// 対局の初期局面（平手、2枚落ちなど）を取得する。
const QString &StartGameDialog::startingPositionName() const
{
    return m_startingPositionName;
}

// 対局の初期局面番号を取得する。
int StartGameDialog::startingPositionNumber() const
{
    return m_startingPositionNumber;
}

// エンジンリストを取得する。
const QList<StartGameDialog::Engine> &StartGameDialog::getEngineList() const
{
    return engineList;
}

// 最大手数を取得する。
int StartGameDialog::maxMoves() const
{
    return m_maxMoves;
}

// 連続対局数を取得する。
int StartGameDialog::consecutiveGames() const
{
    return m_consecutiveGames;
}

// 人を手前に表示するかどうかのフラグを取得する。
bool StartGameDialog::isShowHumanInFront() const
{
    return m_isShowHumanInFront;
}

// 棋譜の自動保存フラグを取得する。
bool StartGameDialog::isAutoSaveKifu() const
{
    return m_isAutoSaveKifu;
}

// 棋譜の保存ディレクトリを取得する。
const QString& StartGameDialog::kifuSaveDir() const
{
    return m_kifuSaveDir;
}

// 時間切れを負けにするかどうかのフラグを取得する。
bool StartGameDialog::isLoseOnTimeout() const
{
    return m_isLoseOnTimeout;
}

// 1局ごとに手番を入れ替えるかどうかのフラグを取得する。
bool StartGameDialog::isSwitchTurnEachGame() const
{
    return m_isSwitchTurnEachGame;
}

// 持将棋ルールを取得する（0: なし, 1: 24点法, 2: 27点法）。
int StartGameDialog::jishogiRule() const
{
    return m_jishogiRule;
}

// OKボタンが押された場合、対局ダイアログ内の各パラメータを取得する。
void StartGameDialog::updateGameSettingsFromDialog()
{
    // 先手／下手の設定を取得する。
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    if (player1Index == 0) {
        // 人間の場合
        m_isHuman1 = true;
        m_isEngine1 = false;
        m_engineNumber1 = -1;
        m_engineName1 = "";
    } else {
        // エンジンの場合
        m_isHuman1 = false;
        m_isEngine1 = true;
        m_engineNumber1 = player1Index - 1;
        m_engineName1 = ui->comboBoxPlayer1->currentText();
    }
    m_humanName1 = ui->lineEditHumanName1->text();

    // 後手／上手の設定を取得する。
    int player2Index = ui->comboBoxPlayer2->currentIndex();
    if (player2Index == 0) {
        // 人間の場合
        m_isHuman2 = true;
        m_isEngine2 = false;
        m_engineNumber2 = -1;
        m_engineName2 = "";
    } else {
        // エンジンの場合
        m_isHuman2 = false;
        m_isEngine2 = true;
        m_engineNumber2 = player2Index - 1;
        m_engineName2 = ui->comboBoxPlayer2->currentText();
    }
    m_humanName2 = ui->lineEditHumanName2->text();

    // 対局者1の持ち時間を取得する。
    m_basicTimeHour1 = ui->basicTimeHour1->text().toInt();
    m_basicTimeMinutes1 = ui->basicTimeMinutes1->text().toInt();

    // 対局者1の秒読み時間（秒）を取得する。
    m_byoyomiSec1 = ui->byoyomiSec1->text().toInt();

    // 対局者1の1手ごとの時間加算（秒）を取得する。
    m_addEachMoveSec1 = ui->addEachMoveSec1->text().toInt();

    // 「後手／上手に異なる時間を設定」にチェックが入っている場合
    if (ui->groupBoxSecondPlayerTimeSettings->isChecked()) {
        // 対局者2の持ち時間を取得する。
        m_basicTimeHour2 = ui->basicTimeHour2->text().toInt();
        m_basicTimeMinutes2 = ui->basicTimeMinutes2->text().toInt();

        // 対局者2の秒読み時間（秒）を取得する。
        m_byoyomiSec2 = ui->byoyomiSec2->text().toInt();

        // 対局者2の1手ごとの時間加算（秒）を取得する。
        m_addEachMoveSec2 = ui->addEachMoveSec2->text().toInt();
    }
    // 「後手／上手に異なる時間を設定」にチェックが入っていない場合
    else {
        // 対局者1の持ち時間を対局者2の持ち時間にコピーする。
        m_basicTimeHour2 = m_basicTimeHour1;
        m_basicTimeMinutes2 = m_basicTimeMinutes1;

        // 対局者1の秒読み時間（秒）を対局者2の秒読み時間にコピーする。
        m_byoyomiSec2 = m_byoyomiSec1;

        // 対局者1の1手ごとの時間加算（秒）を対局者2の1手ごとの時間加算にコピーする。
        m_addEachMoveSec2 = m_addEachMoveSec1;
    }

    // 開始局面を取得する。
    m_startingPositionName = ui->comboBoxStartingPosition->currentText();
    m_startingPositionNumber = ui->comboBoxStartingPosition->currentIndex();

    // 最大手数を取得する。
    m_maxMoves = ui->spinBoxMaxMoves->value();

    // 連続対局数を取得する。
    m_consecutiveGames = ui->spinBoxConsecutiveGames->value();

    // 人を手前に表示するかどうかのフラグを取得する。
    m_isShowHumanInFront = ui->checkBoxShowHumanInFront->isChecked();

    // 棋譜の自動保存フラグを取得する。
    m_isAutoSaveKifu = ui->checkBoxAutoSaveKifu->isChecked();

    // 棋譜の保存ディレクトリを取得する。
    m_kifuSaveDir = ui->lineEditKifuSaveDir->text();

    // 時間切れを負けにするかどうかのフラグを取得する。
    m_isLoseOnTimeout = ui->checkBoxLoseOnTimeOut->isChecked();

    // 1局ごとに手番を入れ替えるかどうかのフラグを取得する。
    m_isSwitchTurnEachGame = ui->checkBoxSwitchTurnEachGame->isChecked();

    // 持将棋ルールを取得する。
    m_jishogiRule = ui->comboBoxJishogi->currentIndex();
}

// エンジン設定ダイアログの共通処理を行う。
void StartGameDialog::showEngineSettingsDialog(int playerNumber)
{
    // 対局者コンボボックスを取得する。
    QComboBox* comboBox = (playerNumber == 1) ? ui->comboBoxPlayer1 : ui->comboBoxPlayer2;

    // 現在のインデックスを取得する。
    int currentIndex = comboBox->currentIndex();

    // 人間が選択されている場合（インデックス0）は何もしない。
    if (currentIndex == 0) {
        QMessageBox::information(this, tr("情報"), tr("人間が選択されています。"));
        return;
    }

    // エンジン番号を計算する（インデックスから1を引く）。
    int engineNumber = currentIndex - 1;

    // エンジン名を取得する。
    QString engineName = comboBox->currentText();

    // エンジン名が空の場合
    if (engineName.isEmpty()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
    }
    // エンジン名が空でない場合
    else {
        // エンジン設定ダイアログを表示する。
        ChangeEngineSettingsDialog dialog(this);

        // エンジン名、エンジン番号を設定する。
        dialog.setEngineNumber(engineNumber);
        dialog.setEngineName(engineName);

        // エンジン設定ダイアログを作成する。
        dialog.setupEngineOptionsDialog();

        // ダイアログがキャンセルされた場合、何もしない。
        if (dialog.exec() == QDialog::Rejected) {
            return;
        }
    }
}

// 先手／下手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void StartGameDialog::onFirstPlayerSettingsClicked()
{
    showEngineSettingsDialog(1);
}

// 後手／上手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void StartGameDialog::onSecondPlayerSettingsClicked()
{
    showEngineSettingsDialog(2);
}

// 文字サイズを大きくする。
void StartGameDialog::increaseFontSize()
{
    if (m_fontSize < MaxFontSize) {
        m_fontSize++;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

// 文字サイズを小さくする。
void StartGameDialog::decreaseFontSize()
{
    if (m_fontSize > MinFontSize) {
        m_fontSize--;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

// 文字サイズを適用する。
void StartGameDialog::applyFontSize(int size)
{
    QFont font = this->font();
    font.setPointSize(size);
    this->setFont(font);
}

// 文字サイズ設定を読み込む。
void StartGameDialog::loadFontSizeSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);
    settings.beginGroup("GameSettings");
    m_fontSize = settings.value("dialogFontSize", DefaultFontSize).toInt();
    settings.endGroup();

    // 範囲内に収める
    if (m_fontSize < MinFontSize) m_fontSize = MinFontSize;
    if (m_fontSize > MaxFontSize) m_fontSize = MaxFontSize;

    applyFontSize(m_fontSize);
}

// 文字サイズ設定を保存する。
void StartGameDialog::saveFontSizeSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);
    settings.beginGroup("GameSettings");
    settings.setValue("dialogFontSize", m_fontSize);
    settings.endGroup();
}

// 対局者コンボボックスの選択に応じてUIを更新する。
void StartGameDialog::updatePlayerUI(int playerNumber, int index)
{
    // playerNumber: 1 = 先手／下手, 2 = 後手／上手
    // index: 0 = 人間, 1以上 = エンジン
    // QStackedWidgetのページ: 0 = 人間名入力欄, 1 = エンジン設定ボタン

    if (playerNumber == 1) {
        // 人間が選択された場合はページ0、エンジンの場合はページ1
        const int pageIndex = (index == 0) ? 0 : 1;
        ui->stackedWidgetPlayer1->setCurrentIndex(pageIndex);
        ui->stackedWidgetLabel1->setCurrentIndex(pageIndex);
    } else {
        // 人間が選択された場合はページ0、エンジンの場合はページ1
        const int pageIndex = (index == 0) ? 0 : 1;
        ui->stackedWidgetPlayer2->setCurrentIndex(pageIndex);
        ui->stackedWidgetLabel2->setCurrentIndex(pageIndex);
    }
}

// 先手／下手の対局者選択が変更された場合、UIを更新する。
void StartGameDialog::onPlayer1SelectionChanged(int index)
{
    updatePlayerUI(1, index);
    updateConsecutiveGamesEnabled();
}

// 後手／上手の対局者選択が変更された場合、UIを更新する。
void StartGameDialog::onPlayer2SelectionChanged(int index)
{
    updatePlayerUI(2, index);
    updateConsecutiveGamesEnabled();
}

// 連続対局スピンボックスの有効/無効を更新する。
void StartGameDialog::updateConsecutiveGamesEnabled()
{
    // 両方のプレイヤーがエンジン（インデックス > 0）の場合のみ連続対局を有効にする
    bool isEngine1 = (ui->comboBoxPlayer1->currentIndex() > 0);
    bool isEngine2 = (ui->comboBoxPlayer2->currentIndex() > 0);
    bool enableConsecutive = (isEngine1 && isEngine2);

    ui->spinBoxConsecutiveGames->setEnabled(enableConsecutive);

    // 人間が含まれる場合は連続対局数を1に固定する
    if (!enableConsecutive) {
        ui->spinBoxConsecutiveGames->setValue(1);
    }
}

// 棋譜保存ディレクトリ選択ボタンが押された場合、ディレクトリ選択ダイアログを表示する。
void StartGameDialog::onSelectKifuDirClicked()
{
    // 現在のディレクトリを取得（空の場合はドキュメントフォルダ）
    QString currentDir = ui->lineEditKifuSaveDir->text();
    if (currentDir.isEmpty()) {
        currentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    // ディレクトリ選択ダイアログを表示
    QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        tr("棋譜保存先を選択"),
        currentDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    // ディレクトリが選択された場合、テキストボックスに設定
    if (!selectedDir.isEmpty()) {
        ui->lineEditKifuSaveDir->setText(selectedDir);
    }
}
