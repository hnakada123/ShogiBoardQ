/// @file considerationdialog.cpp
/// @brief 検討ダイアログクラスの実装

#include "considerationdialog.h"
#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "ui_considerationdialog.h"
#include "settingscommon.h"
#include "analysissettings.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QPushButton>

// 検討ダイアログを表示する。
ConsiderationDialog::ConsiderationDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ConsiderationDialog>())
    , m_fontSize(AnalysisSettings::considerationDialogFontSize())
{
    // UIをセットアップする。
    ui->setupUi(this);

    // フォントサイズを適用
    applyFontSize();

    // フォントサイズボタンにスタイルを適用
    ui->toolButtonFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    ui->toolButtonFontIncrease->setStyleSheet(ButtonStyles::fontButton());

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    readEngineNameAndDir();

    // 保存された設定を復元する
    loadSettings();

    // エンジン設定ボタンが押されたときの処理
    connect(ui->engineSetting, &QPushButton::clicked, this, &ConsiderationDialog::showEngineSettingsDialog);

    // OKボタンが押された場合、ダイアログを受け入れたとして閉じる動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConsiderationDialog::accept);

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConsiderationDialog::processEngineSettings);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ConsiderationDialog::reject);

    // フォントサイズボタン
    connect(ui->toolButtonFontIncrease, &QPushButton::clicked, this, &ConsiderationDialog::onFontIncrease);
    connect(ui->toolButtonFontDecrease, &QPushButton::clicked, this, &ConsiderationDialog::onFontDecrease);
}

ConsiderationDialog::~ConsiderationDialog() = default;

// エンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void ConsiderationDialog::showEngineSettingsDialog()
{
    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // エンジン名が空の場合
    if (m_engineName.isEmpty()) {
        // エラーメッセージを表示する。
        QString errorText = "将棋エンジンが選択されていません。";
        QMessageBox::critical(this, "エラー", errorText);
    }
    // エンジン名が空でない場合
    else {
        // エンジン設定ダイアログを表示する。
        ChangeEngineSettingsDialog dialog(this);

        // エンジン名、エンジン番号を設定する。
        dialog.setEngineNumber(m_engineNumber);
        dialog.setEngineName(m_engineName);

        // エンジン設定ダイアログを作成する。
        dialog.setupEngineOptionsDialog();

        // ダイアログがキャンセルされた場合、何もしない。
        if (dialog.exec() == QDialog::Rejected) return;
    }
}

// エンジン名を取得する。
const QString& ConsiderationDialog::getEngineName() const
{
    return m_engineName;
}

// エンジン番号を取得する。
int ConsiderationDialog::getEngineNumber() const
{
    return m_engineNumber;
}

// 1手あたりの思考時間（秒数）を取得する。
int ConsiderationDialog::getByoyomiSec() const
{
    return m_byoyomiSec;
}

// 候補手の数（MultiPV）を取得する。
int ConsiderationDialog::getMultiPV() const
{
    return m_multiPV;
}

// エンジン名、エンジン番号、時間無制限フラグ、検討時間フラグ、検討時間を取得する。
void ConsiderationDialog::processEngineSettings()
{
    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // "時間無制限"にチェックが入っている場合
    if (ui->unlimitedTimeRadioButton->isChecked()) {
        m_unlimitedTimeFlag = true;
    }
    // "検討時間"にチェックが入っている場合
    else if (ui->considerationTimeRadioButton->isChecked()) {
        m_unlimitedTimeFlag = false;
        m_byoyomiSec = ui->byoyomiSec->text().toInt();
    }

    // 候補手の数を取得する。
    m_multiPV = ui->spinBoxMultiPV->value();

    // 設定を保存する
    saveSettings();
}

// 設定ファイルからエンジンの名前とディレクトリを読み込む。
void ConsiderationDialog::readEngineNameAndDir()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    // エンジンの数を取得する。
    int size = settings.beginReadArray("Engines");

    // エンジンの数だけループする。
    for (int i = 0; i < size; i++) {
        // 現在のインデックスで配列の要素を設定する。
        settings.setArrayIndex(i);

        // エンジン名とディレクトリを取得する。
        Engine engine;
        engine.name = settings.value("name").toString();
        engine.path = settings.value("path").toString();

        // 棋譜解析ダイアログのエンジン選択リストにエンジン名を追加する。
        ui->comboBoxEngine1->addItem(engine.name);

        // エンジンリストにエンジンを追加する。
        engineList.append(engine);
    }

    // エンジン名のグループの配列の読み込みを終了する。
    settings.endArray();
}

// エンジンの名前とディレクトリを格納するリストを取得する。
const QList<ConsiderationDialog::Engine>& ConsiderationDialog::getEngineList() const
{
    return engineList;
}

// フォントサイズを大きくする
void ConsiderationDialog::onFontIncrease()
{
    updateFontSize(1);
}

// フォントサイズを小さくする
void ConsiderationDialog::onFontDecrease()
{
    updateFontSize(-1);
}

// フォントサイズを更新する
void ConsiderationDialog::updateFontSize(int delta)
{
    m_fontSize += delta;
    if (m_fontSize < 8) m_fontSize = 8;
    if (m_fontSize > 24) m_fontSize = 24;

    applyFontSize();

    // SettingsServiceに保存
    AnalysisSettings::setConsiderationDialogFontSize(m_fontSize);
}

// ダイアログ全体にフォントサイズを適用する
void ConsiderationDialog::applyFontSize()
{
    // 念のため範囲外値を抑止（設定ファイル汚損時の安全策）
    m_fontSize = qBound(8, m_fontSize, 24);

    QFont f = font();
    f.setPointSize(m_fontSize);
    setFont(f);

    // コンストラクタ中は setFont() による子ウィジェットへのフォント伝播が
    // 遅延するため、全子ウィジェットに明示的にフォントを設定する
    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget) {
            widget->setFont(f);
        }
    }

    // コンボボックスのポップアップリストにも反映する
    const QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : std::as_const(comboBoxes)) {
        if (comboBox && comboBox->view()) {
            comboBox->view()->setFont(f);
        }
    }
}

// 保存された設定を読み込む
void ConsiderationDialog::loadSettings()
{
    // 最後に選択したエンジン番号を復元
    int engineIndex = AnalysisSettings::considerationEngineIndex();
    if (engineIndex >= 0 && engineIndex < ui->comboBoxEngine1->count()) {
        ui->comboBoxEngine1->setCurrentIndex(engineIndex);
    }

    // 時間設定を復元
    bool unlimitedTime = AnalysisSettings::considerationUnlimitedTime();
    if (unlimitedTime) {
        ui->unlimitedTimeRadioButton->setChecked(true);
    } else {
        ui->considerationTimeRadioButton->setChecked(true);
    }

    // 検討時間（秒）を復元
    int byoyomiSec = AnalysisSettings::considerationByoyomiSec();
    ui->byoyomiSec->setValue(byoyomiSec);

    // 候補手の数を復元
    int multiPV = AnalysisSettings::considerationMultiPV();
    ui->spinBoxMultiPV->setValue(multiPV);
}

// 設定を保存する
void ConsiderationDialog::saveSettings()
{
    // エンジン番号を保存
    AnalysisSettings::setConsiderationEngineIndex(m_engineNumber);

    // 時間無制限フラグを保存
    AnalysisSettings::setConsiderationUnlimitedTime(m_unlimitedTimeFlag);

    // 検討時間（秒）を保存
    AnalysisSettings::setConsiderationByoyomiSec(ui->byoyomiSec->value());

    // 候補手の数を保存
    AnalysisSettings::setConsiderationMultiPV(m_multiPV);
}
