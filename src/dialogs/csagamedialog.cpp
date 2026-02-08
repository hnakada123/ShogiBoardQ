/// @file csagamedialog.cpp
/// @brief CSA対局ダイアログクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "csagamedialog.h"
#include "ui_csagamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include "settingsservice.h"
#include <QSettings>
#include <QMessageBox>
#include <QDir>

using namespace EngineSettingsConstants;

CsaGameDialog::CsaGameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CsaGameDialog)
    , m_fontSize(SettingsService::csaGameDialogFontSize())
    , m_isHuman(true)
    , m_isEngine(false)
    , m_engineNumber(0)
    , m_port(4081)
{
    ui->setupUi(this);

    // フォントサイズを適用
    applyFontSize();

    // 設定ファイルからエンジン情報を読み込む
    loadEngineConfigurations();

    // UIにエンジン設定を反映する
    populateUIWithEngines();

    // 設定ファイルからサーバー接続履歴を読み込む
    loadServerHistory();

    // UIにサーバー履歴を反映する
    populateUIWithServerHistory();

    // 設定ファイルから対局設定を読み込む
    loadGameSettings();

    // シグナル・スロットの接続を行う
    connectSignalsAndSlots();
}

CsaGameDialog::~CsaGameDialog()
{
    delete ui;
}

// シグナル・スロットの接続を行う
void CsaGameDialog::connectSignalsAndSlots()
{
    // 対局開始ボタン
    connect(ui->pushButtonStart, &QPushButton::clicked,
            this, &CsaGameDialog::onAccepted);

    // キャンセルボタン
    connect(ui->pushButtonCancel, &QPushButton::clicked,
            this, &QDialog::reject);

    // エンジン設定ボタン
    connect(ui->pushButtonEngineSettings, &QPushButton::clicked,
            this, &CsaGameDialog::onEngineSettingsClicked);

    // サーバー履歴選択
    connect(ui->comboBoxHistory, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CsaGameDialog::onServerHistoryChanged);

    // パスワード表示チェックボックス
    connect(ui->checkBoxShowPassword, &QCheckBox::toggled,
            this, &CsaGameDialog::onShowPasswordToggled);

    // フォントサイズボタン
    connect(ui->toolButtonFontIncrease, &QToolButton::clicked,
            this, &CsaGameDialog::onFontIncrease);
    connect(ui->toolButtonFontDecrease, &QToolButton::clicked,
            this, &CsaGameDialog::onFontDecrease);
}

// 設定ファイルからエンジン情報を読み込む
void CsaGameDialog::loadEngineConfigurations()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // [Engines]グループ内のエンジン数を取得する
    int size = settings.beginReadArray("Engines");

    // エンジンリストをクリアする
    m_engineList.clear();

    // エンジンリストに追加する
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Engine engine;
        engine.name = settings.value("name").toString();
        engine.path = settings.value("path").toString();
        m_engineList.append(engine);
    }

    settings.endArray();
}

// UIにエンジン設定を反映する
void CsaGameDialog::populateUIWithEngines()
{
    // エンジンコンボボックスをクリアする
    ui->comboBoxEngine->clear();

    // エンジンリストからエンジン名をコンボボックスに追加する
    for (const Engine& engine : std::as_const(m_engineList)) {
        ui->comboBoxEngine->addItem(engine.name);
    }
}

// 設定ファイルからサーバー接続履歴を読み込む
void CsaGameDialog::loadServerHistory()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // [CsaServerHistory]グループ内の履歴数を取得する
    int size = settings.beginReadArray("CsaServerHistory");

    // 履歴リストをクリアする
    m_serverHistory.clear();

    // 履歴リストに追加する
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        ServerHistory history;
        history.host = settings.value("host").toString();
        history.port = settings.value("port", 4081).toInt();
        history.id = settings.value("id").toString();
        history.password = settings.value("password").toString();
        history.version = settings.value("version", "CSAプロトコル1.2.1 読み筋コメント出力あり").toString();
        m_serverHistory.append(history);
    }

    settings.endArray();
}

// 現在の設定をサーバー接続履歴として保存する
void CsaGameDialog::saveServerHistory()
{
    // 現在の設定を取得
    ServerHistory current;
    current.host = ui->lineEditHost->text().trimmed();
    current.port = ui->spinBoxPort->value();
    current.id = ui->lineEditId->text().trimmed();
    current.password = ui->lineEditPassword->text();
    current.version = ui->comboBoxVersion->currentText();

    // 空の場合は保存しない
    if (current.host.isEmpty() || current.id.isEmpty()) {
        return;
    }

    // 同じ設定が既にあるか確認し、あれば削除（最新を先頭に移動するため）
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        const ServerHistory& h = m_serverHistory.at(i);
        if (h.host == current.host && h.port == current.port && h.id == current.id) {
            m_serverHistory.removeAt(i);
            break;
        }
    }

    // 先頭に追加
    m_serverHistory.prepend(current);

    // 最大10件まで保持
    while (m_serverHistory.size() > 10) {
        m_serverHistory.removeLast();
    }

    // 設定ファイルに保存
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    settings.beginWriteArray("CsaServerHistory");
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        settings.setArrayIndex(i);
        const ServerHistory& h = m_serverHistory.at(i);
        settings.setValue("host", h.host);
        settings.setValue("port", h.port);
        settings.setValue("id", h.id);
        settings.setValue("password", h.password);
        settings.setValue("version", h.version);
    }
    settings.endArray();
}

// UIにサーバー履歴を反映する
void CsaGameDialog::populateUIWithServerHistory()
{
    // 履歴コンボボックスをクリアする
    ui->comboBoxHistory->clear();

    // 空の選択肢を追加
    ui->comboBoxHistory->addItem(QString());

    // 履歴リストからサーバー履歴をコンボボックスに追加する
    for (const ServerHistory& history : std::as_const(m_serverHistory)) {
        QString displayText = formatServerHistoryDisplay(history);
        ui->comboBoxHistory->addItem(displayText);
    }
}

// サーバー履歴の表示文字列を生成する
QString CsaGameDialog::formatServerHistoryDisplay(const ServerHistory& history) const
{
    return QString("%1:%2 %3").arg(history.host).arg(history.port).arg(history.id);
}

// 設定ファイルから対局設定を読み込む
void CsaGameDialog::loadGameSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    settings.beginGroup("CsaGameSettings");

    // 対局者設定
    bool isHuman = settings.value("isHuman", true).toBool();
    ui->radioButtonHuman->setChecked(isHuman);
    ui->radioButtonEngine->setChecked(!isHuman);

    int engineIndex = settings.value("engineNumber", 0).toInt();
    if (engineIndex >= 0 && engineIndex < ui->comboBoxEngine->count()) {
        ui->comboBoxEngine->setCurrentIndex(engineIndex);
    }

    // サーバー設定
    ui->lineEditHost->setText(settings.value("host", "").toString());
    ui->spinBoxPort->setValue(settings.value("port", 4081).toInt());
    ui->lineEditId->setText(settings.value("id", "").toString());
    ui->lineEditPassword->setText(settings.value("password", "").toString());

    int versionIndex = settings.value("versionIndex", 0).toInt();
    if (versionIndex >= 0 && versionIndex < ui->comboBoxVersion->count()) {
        ui->comboBoxVersion->setCurrentIndex(versionIndex);
    }

    settings.endGroup();
}

// 対局設定を設定ファイルに保存する
void CsaGameDialog::saveGameSettings()
{
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    settings.beginGroup("CsaGameSettings");

    // 対局者設定
    settings.setValue("isHuman", ui->radioButtonHuman->isChecked());
    settings.setValue("engineName", ui->comboBoxEngine->currentText());
    settings.setValue("engineNumber", ui->comboBoxEngine->currentIndex());

    // サーバー設定
    settings.setValue("host", ui->lineEditHost->text().trimmed());
    settings.setValue("port", ui->spinBoxPort->value());
    settings.setValue("id", ui->lineEditId->text().trimmed());
    settings.setValue("password", ui->lineEditPassword->text());
    settings.setValue("versionIndex", ui->comboBoxVersion->currentIndex());

    settings.endGroup();
}

// 対局開始ボタンが押された時の処理
void CsaGameDialog::onAccepted()
{
    // 入力値の検証
    QString host = ui->lineEditHost->text().trimmed();
    QString id = ui->lineEditId->text().trimmed();
    QString password = ui->lineEditPassword->text();

    if (host.isEmpty()) {
        QMessageBox::warning(this, tr("入力エラー"), tr("接続先ホストを入力してください。"));
        ui->lineEditHost->setFocus();
        return;
    }

    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("入力エラー"), tr("IDを入力してください。"));
        ui->lineEditId->setFocus();
        return;
    }

    if (password.isEmpty()) {
        QMessageBox::warning(this, tr("入力エラー"), tr("パスワードを入力してください。"));
        ui->lineEditPassword->setFocus();
        return;
    }

    // エンジンが選択されている場合、エンジンが存在するか確認
    if (ui->radioButtonEngine->isChecked() && ui->comboBoxEngine->count() == 0) {
        QMessageBox::warning(this, tr("エラー"), tr("将棋エンジンが登録されていません。\nツール→エンジン設定からエンジンを登録してください。"));
        return;
    }

    // 設定値をメンバー変数にキャッシュ
    m_isHuman = ui->radioButtonHuman->isChecked();
    m_isEngine = ui->radioButtonEngine->isChecked();
    m_engineName = ui->comboBoxEngine->currentText();
    m_engineNumber = ui->comboBoxEngine->currentIndex();
    m_host = host;
    m_port = ui->spinBoxPort->value();
    m_loginId = id;
    m_password = password;
    m_csaVersion = ui->comboBoxVersion->currentText();

    // 設定を保存
    saveGameSettings();
    saveServerHistory();

    // ダイアログを受け入れて閉じる
    accept();
}

// エンジン設定ボタンが押された時の処理
void CsaGameDialog::onEngineSettingsClicked()
{
    showEngineSettingsDialog(ui->comboBoxEngine);
}

// サーバー履歴が選択された時の処理
void CsaGameDialog::onServerHistoryChanged(int index)
{
    // インデックス0は空の選択肢なのでスキップ
    if (index <= 0 || index > m_serverHistory.size()) {
        return;
    }

    // 履歴から設定を復元（インデックスは1始まりなので-1する）
    const ServerHistory& history = m_serverHistory.at(index - 1);
    ui->lineEditHost->setText(history.host);
    ui->spinBoxPort->setValue(history.port);
    ui->lineEditId->setText(history.id);
    ui->lineEditPassword->setText(history.password);

    // バージョンを設定
    int versionIndex = ui->comboBoxVersion->findText(history.version);
    if (versionIndex >= 0) {
        ui->comboBoxVersion->setCurrentIndex(versionIndex);
    }
}

// パスワード表示チェックボックスの状態が変化した時の処理
void CsaGameDialog::onShowPasswordToggled(bool checked)
{
    if (checked) {
        ui->lineEditPassword->setEchoMode(QLineEdit::Normal);
    } else {
        ui->lineEditPassword->setEchoMode(QLineEdit::Password);
    }
}

// エンジン設定ダイアログを表示する
void CsaGameDialog::showEngineSettingsDialog(QComboBox* comboBox)
{
    int engineNumber = comboBox->currentIndex();
    QString engineName = comboBox->currentText();

    if (engineName.isEmpty()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    ChangeEngineSettingsDialog dialog(this);
    dialog.setEngineNumber(engineNumber);
    dialog.setEngineName(engineName);
    dialog.setupEngineOptionsDialog();

    if (dialog.exec() == QDialog::Rejected) {
        return;
    }
}

// ========== Getter implementations ==========

QString CsaGameDialog::host() const
{
    return m_host;
}

int CsaGameDialog::port() const
{
    return m_port;
}

QString CsaGameDialog::loginId() const
{
    return m_loginId;
}

QString CsaGameDialog::password() const
{
    return m_password;
}

QString CsaGameDialog::csaVersion() const
{
    return m_csaVersion;
}

bool CsaGameDialog::isHuman() const
{
    return m_isHuman;
}

bool CsaGameDialog::isEngine() const
{
    return m_isEngine;
}

QString CsaGameDialog::engineName() const
{
    return m_engineName;
}

int CsaGameDialog::engineNumber() const
{
    return m_engineNumber;
}

const QList<CsaGameDialog::Engine>& CsaGameDialog::engineList() const
{
    return m_engineList;
}

// フォントサイズを大きくする
void CsaGameDialog::onFontIncrease()
{
    updateFontSize(1);
}

// フォントサイズを小さくする
void CsaGameDialog::onFontDecrease()
{
    updateFontSize(-1);
}

// フォントサイズを更新する
void CsaGameDialog::updateFontSize(int delta)
{
    m_fontSize += delta;
    if (m_fontSize < 8) m_fontSize = 8;
    if (m_fontSize > 24) m_fontSize = 24;

    applyFontSize();

    // SettingsServiceに保存
    SettingsService::setCsaGameDialogFontSize(m_fontSize);
}

// ダイアログ全体にフォントサイズを適用する
void CsaGameDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    this->setFont(font);
}
