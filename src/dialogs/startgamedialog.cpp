#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include <QSettings>
#include <QMessageBox>
#include <QDir>

using namespace EngineSettingsConstants;

// コンストラクタ
StartGameDialog::StartGameDialog(QWidget *parent) : QDialog(parent), ui(new Ui::StartGameDialog)
{
    // UIをセットアップする。
    ui->setupUi(this);

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    loadEngineConfigurations();

    // GUIにエンジン設定を反映する。
    populateUIWithEngines();

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
    settings.setValue("isHuman1", ui->radioButtonHuman1->isChecked());
    settings.setValue("isEngine1", ui->radioButtonEngine1->isChecked());
    settings.setValue("engineName1", ui->comboBoxEngine1->currentText());
    settings.setValue("humanName1", ui->lineEditHumanName1->text());
    settings.setValue("engineNumber1", ui->comboBoxEngine1->currentIndex());
    settings.setValue("basicTimeHour1", ui->basicTimeHour1->value());
    settings.setValue("basicTimeMinutes1", ui->basicTimeMinutes1->value());
    settings.setValue("byoyomiSec1", ui->byoyomiSec1->value());
    settings.setValue("addEachMoveSec1", ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存する。
    settings.setValue("isGroupBoxSecondPlayerTimeSettingsChecked", ui->groupBoxSecondPlayerTimeSettings->isChecked());

    settings.setValue("isHuman2", ui->radioButtonHuman2->isChecked());
    settings.setValue("isEngine2", ui->radioButtonEngine2->isChecked());
    settings.setValue("engineName2", ui->comboBoxEngine2->currentText());
    settings.setValue("humanName2", ui->lineEditHumanName2->text());
    settings.setValue("engineNumber2", ui->comboBoxEngine2->currentIndex());
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

    // 時間切れを負けにするかどうかのフラグを保存する。
    settings.setValue("isLoseOnTimeout", ui->checkBoxLoseOnTimeOut->isChecked());

    // 1局ごとに手番を入れ替えるかどうかのフラグを保存する。
    settings.setValue("isSwitchTurnEachGame", ui->checkBoxSwitchTurnEachGame->isChecked());

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
    ui->radioButtonHuman1->setChecked(settings.value("isHuman1", true).toBool());
    ui->radioButtonEngine1->setChecked(settings.value("isEngine1", false).toBool());
    ui->comboBoxEngine1->setCurrentText(settings.value("engineName1", "").toString());
    ui->lineEditHumanName1->setText(settings.value("humanName1", tr("You")).toString());
    ui->comboBoxEngine1->setCurrentIndex(settings.value("engineNumber1", 0).toInt());
    ui->basicTimeHour1->setValue(settings.value("basicTimeHour1", 0).toInt());
    ui->basicTimeMinutes1->setValue(settings.value("basicTimeMinutes1", 0).toInt());
    ui->byoyomiSec1->setValue(settings.value("byoyomiSec1", 0).toInt());
    ui->addEachMoveSec1->setValue(settings.value("addEachMoveSec1", 0).toInt());

    // 後手／上手の設定を読み込む。
    bool isChecked = settings.value("isGroupBoxSecondPlayerTimeSettingsChecked", false).toBool();
    ui->groupBoxSecondPlayerTimeSettings->setChecked(isChecked);

    ui->radioButtonHuman2->setChecked(settings.value("isHuman2", false).toBool());
    ui->radioButtonEngine2->setChecked(settings.value("isEngine2", true).toBool());
    ui->comboBoxEngine2->setCurrentText(settings.value("engineName2", "").toString());
    ui->lineEditHumanName2->setText(settings.value("humanName2", tr("You")).toString());
    ui->comboBoxEngine2->setCurrentIndex(settings.value("engineNumber2", 0).toInt());
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

    // 時間切れを負けにするかどうかのフラグを読み込む。
    ui->checkBoxLoseOnTimeOut->setChecked(settings.value("isLoseOnTimeout", true).toBool());

    // 1局ごとに手番を入れ替えるかどうかのフラグを読み込む。
    ui->checkBoxSwitchTurnEachGame->setChecked(settings.value("isSwitchTurnEachGame", false).toBool());

    // 設定の読み込みを終了する。
    settings.endGroup();
}

// 設定を初期値にリセットする。
void StartGameDialog::resetSettingsToDefault()
{
    // 先手／下手の設定を初期値に戻す。
    // 人間をデフォルトに設定する。
    ui->radioButtonHuman1->setChecked(true);
    ui->radioButtonEngine1->setChecked(false);

    // エンジン1のデフォルト値を設定する。
    ui->comboBoxEngine1->setCurrentIndex(0);

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
    // エンジン2をデフォルトに設定する。
    ui->radioButtonHuman2->setChecked(false);
    ui->radioButtonEngine2->setChecked(true);

    // エンジン2のデフォルト値を設定する。
    ui->comboBoxEngine2->setCurrentIndex(0);

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

    // 時間切れを負けにする。
    ui->checkBoxLoseOnTimeOut->setChecked(true);

     // 手番の入れ替えはデフォルトでオフにする。
    ui->checkBoxSwitchTurnEachGame->setChecked(false);
}

// 先後入れ替えボタンが押された場合、先後を入れ替える。
void StartGameDialog::swapSides()
{
    // 対局者のラジオボタンを入れ替える。
    bool isHumanTemp = ui->radioButtonHuman2->isChecked();
    bool isEngineTemp = ui->radioButtonEngine2->isChecked();
    ui->radioButtonHuman2->setChecked(ui->radioButtonHuman1->isChecked());
    ui->radioButtonEngine2->setChecked(ui->radioButtonEngine1->isChecked());
    ui->radioButtonHuman1->setChecked(isHumanTemp);
    ui->radioButtonEngine1->setChecked(isEngineTemp);

    // 人間の名前を入れ替える。
    QString temp = ui->lineEditHumanName1->text();
    ui->lineEditHumanName1->setText(ui->lineEditHumanName2->text());
    ui->lineEditHumanName2->setText(temp);

    // エンジン名のインデックスを入れ替える。
    int index = ui->comboBoxEngine1->currentIndex();
    ui->comboBoxEngine1->setCurrentIndex(ui->comboBoxEngine2->currentIndex());
    ui->comboBoxEngine2->setCurrentIndex(index);

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

// GUIにエンジン設定を反映する。
void StartGameDialog::populateUIWithEngines() const
{
    // コンボボックスをクリアする。
    ui->comboBoxEngine1->clear();
    ui->comboBoxEngine2->clear();

    // エンジンリストの各エンジンをコンボボックスに追加する。
    foreach (const Engine& engine, engineList) {
        ui->comboBoxEngine1->addItem(engine.name);
        ui->comboBoxEngine2->addItem(engine.name);
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

// OKボタンが押された場合、対局ダイアログ内の各パラメータを取得する。
void StartGameDialog::updateGameSettingsFromDialog()
{
    // 先手／下手が人間である場合
    if (ui->radioButtonHuman1->isChecked()) {
        m_isHuman1 = true;
        m_isEngine1 = false;
    }
    // 先手／下手がエンジンである場合
    else if (ui->radioButtonEngine1->isChecked()) {
        m_isHuman1 = false;
        m_isEngine1 = true;
    }

    m_humanName1 = ui->lineEditHumanName1->text();
    m_engineName1 = ui->comboBoxEngine1->currentText();
    m_engineNumber1 = ui->comboBoxEngine1->currentIndex();

    // 後手／上手が人間である場合
    if (ui->radioButtonHuman2->isChecked()) {
        m_isHuman2 = true;
        m_isEngine2 = false;
    }
    // 後手／上手がエンジンである場合
    else if (ui->radioButtonEngine2->isChecked()) {
        m_isHuman2 = false;
        m_isEngine2 = true;

    }

    m_humanName2 = ui->lineEditHumanName2->text();
    m_engineName2 = ui->comboBoxEngine2->currentText();
    m_engineNumber2 = ui->comboBoxEngine2->currentIndex();

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

    // 時間切れを負けにするかどうかのフラグを取得する。
    m_isLoseOnTimeout = ui->checkBoxLoseOnTimeOut->isChecked();

    // 1局ごとに手番を入れ替えるかどうかのフラグを取得する。
    m_isSwitchTurnEachGame = ui->checkBoxSwitchTurnEachGame->isChecked();
}

// エンジン設定ダイアログの共通処理を行う。
void StartGameDialog::showEngineSettingsDialog(QComboBox* comboBox)
{
    // エンジン番号を取得する。
    int engineNumber = comboBox->currentIndex();

    // エンジン名を取得する。
    QString engineName = comboBox->currentText();

    // エンジン名が空の場合
    if (engineName.isEmpty()) {
        QMessageBox::critical(this, "エラー", "将棋エンジンが選択されていません。");
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
    showEngineSettingsDialog(ui->comboBoxEngine1);
}

 // 後手／上手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void StartGameDialog::onSecondPlayerSettingsClicked()
{
    showEngineSettingsDialog(ui->comboBoxEngine2);
}
