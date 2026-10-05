/// @file csagamedialog.cpp
/// @brief CSA対局ダイアログクラスの実装

#include "csagamedialog.h"
#include "ui_csagamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginelistsettings.h"
#include "settingscommon.h"
#include "networksettings.h"
#include "dialogutils.h"
#include <QSettings>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>

namespace {
constexpr auto kCsaServerHistoryArray = "CsaServerHistory";
constexpr auto kCsaGameSettingsGroup = "CsaGameSettings";
constexpr auto kKeyHost = "host";
constexpr auto kKeyPort = "port";
constexpr auto kKeyId = "id";
constexpr auto kKeyIsHuman = "isHuman";
constexpr auto kKeyEngineNumber = "engineNumber";
constexpr auto kKeyEngineName = "engineName";
constexpr auto kKeyPassword = "password";
} // namespace

CsaGameDialog::CsaGameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::CsaGameDialog>())
    , m_fontHelper({NetworkSettings::csaGameDialogFontSize(), 8, 24, 1,
                    NetworkSettings::setCsaGameDialogFontSize})
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
    updateFormState();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, NetworkSettings::csaGameDialogSize());
    if (ui->lineEditHost->text().isEmpty()) ui->lineEditHost->setFocus();
    else if (ui->lineEditId->text().isEmpty()) ui->lineEditId->setFocus();
    else ui->lineEditPassword->setFocus();
}

CsaGameDialog::~CsaGameDialog()
{
    DialogUtils::saveDialogSize(this, NetworkSettings::setCsaGameDialogSize);
}

// シグナル・スロットの接続を行う
void CsaGameDialog::connectSignalsAndSlots()
{
    connect(ui->radioButtonEngine, &QRadioButton::toggled,
            this, &CsaGameDialog::updateFormState);
    connect(ui->comboBoxEngine, &QComboBox::currentIndexChanged,
            this, &CsaGameDialog::updateFormState);
    for (auto* input : {ui->lineEditHost, ui->lineEditId, ui->lineEditPassword}) {
        connect(input, &QLineEdit::textChanged, this, &CsaGameDialog::updateFormState);
    }
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

void CsaGameDialog::refreshChoices()
{
    const QString engineName = ui->comboBoxEngine->currentText();
    {
        // 履歴の再設定で入力欄を書き換えないよう、選択変更の通知を止める
        const QSignalBlocker engineBlocker(ui->comboBoxEngine);
        const QSignalBlocker historyBlocker(ui->comboBoxHistory);
        loadEngineConfigurations();
        populateUIWithEngines();
        const int engineIndex = ui->comboBoxEngine->findText(engineName);
        if (engineIndex >= 0) ui->comboBoxEngine->setCurrentIndex(engineIndex);
        loadServerHistory();
        populateUIWithServerHistory();
    }
    updateFormState();
}

// 設定ファイルからエンジン情報を読み込む
void CsaGameDialog::loadEngineConfigurations()
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
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    // [CsaServerHistory]グループ内の履歴数を取得する
    int size = settings.beginReadArray(kCsaServerHistoryArray);

    // 履歴リストをクリアする
    m_serverHistory.clear();

    // 履歴リストに追加する
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        ServerHistory history;
        history.host = settings.value(kKeyHost).toString();
        history.port = settings.value(kKeyPort, 4081).toInt();
        history.id = settings.value(kKeyId).toString();
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
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginWriteArray(kCsaServerHistoryArray);
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        settings.setArrayIndex(i);
        const ServerHistory& h = m_serverHistory.at(i);
        settings.setValue(kKeyHost, h.host);
        settings.setValue(kKeyPort, h.port);
        settings.setValue(kKeyId, h.id);
    }
    settings.endArray();
}

// UIにサーバー履歴を反映する
void CsaGameDialog::populateUIWithServerHistory()
{
    // 履歴コンボボックスをクリアする
    ui->comboBoxHistory->clear();

    ui->comboBoxHistory->addItem(tr("新しい接続先"));
    ui->comboBoxHistory->setEnabled(!m_serverHistory.isEmpty());

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
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginGroup(kCsaGameSettingsGroup);

    // 対局者設定
    bool isHuman = settings.value(kKeyIsHuman, true).toBool();
    ui->radioButtonHuman->setChecked(isHuman);
    ui->radioButtonEngine->setChecked(!isHuman);

    int engineIndex = ui->comboBoxEngine->findText(settings.value(kKeyEngineName).toString());
    if (engineIndex < 0) engineIndex = settings.value(kKeyEngineNumber, 0).toInt();
    if (engineIndex >= 0 && engineIndex < ui->comboBoxEngine->count()) {
        ui->comboBoxEngine->setCurrentIndex(engineIndex);
    }

    // サーバー設定
    ui->lineEditHost->setText(settings.value(kKeyHost, "").toString());
    ui->spinBoxPort->setValue(settings.value(kKeyPort, 4081).toInt());
    ui->lineEditId->setText(settings.value(kKeyId, "").toString());
    ui->lineEditPassword->clear();
    settings.remove(kKeyPassword);

    settings.endGroup();
}

// 対局設定を設定ファイルに保存する
void CsaGameDialog::saveGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginGroup(kCsaGameSettingsGroup);

    // 対局者設定
    settings.setValue(kKeyIsHuman, ui->radioButtonHuman->isChecked());
    settings.setValue(kKeyEngineName, ui->comboBoxEngine->currentText());
    settings.setValue(kKeyEngineNumber, ui->comboBoxEngine->currentIndex());

    // サーバー設定
    settings.setValue(kKeyHost, ui->lineEditHost->text().trimmed());
    settings.setValue(kKeyPort, ui->spinBoxPort->value());
    settings.setValue(kKeyId, ui->lineEditId->text().trimmed());
    settings.remove(kKeyPassword);

    settings.endGroup();
}

QString CsaGameDialog::validationMessage() const
{
    if (ui->radioButtonEngine->isChecked() && ui->comboBoxEngine->currentIndex() < 0) {
        return tr("「設定」→「エンジン設定」からエンジンを登録してください。");
    }
    const QString host = ui->lineEditHost->text().trimmed();
    const QString id = ui->lineEditId->text().trimmed();
    const QString password = ui->lineEditPassword->text();
    if (host.isEmpty()) return tr("接続先ホストを入力してください。");
    if (id.isEmpty()) return tr("IDを入力してください。");
    if (password.isEmpty()) return tr("パスワードを入力してください。");
    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    if (host.contains(whitespace) || id.contains(whitespace) || password.contains(whitespace)) {
        return tr("接続先ホスト・ID・パスワードに空白や改行は使用できません。");
    }
    return {};
}

void CsaGameDialog::updateFormState()
{
    const bool engine = ui->radioButtonEngine->isChecked();
    ui->comboBoxEngine->setEnabled(engine && ui->comboBoxEngine->count() > 0);
    ui->pushButtonEngineSettings->setEnabled(engine && ui->comboBoxEngine->currentIndex() >= 0);
    const QString message = validationMessage();
    ui->labelValidation->setText(message.isEmpty() ? tr("接続後、対局相手を待ちます。") : message);
    ui->pushButtonStart->setEnabled(message.isEmpty());
}

// 接続ボタンが押された時の処理
void CsaGameDialog::onAccepted()
{
    if (!validationMessage().isEmpty()) {
        updateFormState();
        return;
    }

    // 設定値をメンバー変数にキャッシュ
    m_isHuman = ui->radioButtonHuman->isChecked();
    m_engineName = ui->comboBoxEngine->currentText();
    m_engineNumber = ui->comboBoxEngine->currentIndex();
    m_host = ui->lineEditHost->text().trimmed();
    m_port = ui->spinBoxPort->value();
    m_loginId = ui->lineEditId->text().trimmed();
    m_password = ui->lineEditPassword->text();

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
    // インデックス0は新しい接続先なのでスキップ
    if (index <= 0 || index > m_serverHistory.size()) {
        return;
    }

    // 履歴から設定を復元（インデックスは1始まりなので-1する）
    const ServerHistory& history = m_serverHistory.at(index - 1);
    ui->lineEditHost->setText(history.host);
    ui->spinBoxPort->setValue(history.port);
    ui->lineEditId->setText(history.id);
    ui->lineEditPassword->clear();
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
    QString engineName = comboBox->currentText();

    if (engineName.isEmpty()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    ChangeEngineSettingsDialog dialog(this);
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

bool CsaGameDialog::isHuman() const
{
    return m_isHuman;
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
    if (m_fontHelper.increase()) applyFontSize();
}

// フォントサイズを小さくする
void CsaGameDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

// ダイアログ全体にフォントサイズを適用する
void CsaGameDialog::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    QFont font = this->font();
    font.setPointSize(m_fontHelper.fontSize());
    DialogUtils::applyFontToAllChildren(this, font);
    DialogUtils::updateFontButtons(this, m_fontHelper.fontSize());
}
