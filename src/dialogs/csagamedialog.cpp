/// @file csagamedialog.cpp
/// @brief CSA対局ダイアログクラスの実装

#include "csagamedialog.h"
#include "ui_csagamedialog.h"
#include "changeenginesettingsdialog.h"
#include "enginelistsettings.h"
#include "networksettings.h"
#include "dialogutils.h"
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>

namespace {
/// shogi-server のゲーム名（名前-持時間-秒読み、末尾の F はフィッシャー）
const QRegularExpression& gameNamePattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9A-Za-z_@.\\-]+-\\d+-\\d+F?$"));
    return pattern;
}
} // namespace

CsaGameDialog::CsaGameDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::CsaGameDialog>())
    , m_fontHelper(NetworkSettings::csaGameDialogFontSize, 8, 24, 1)
{
    ui->setupUi(this);
    populateAutoSaveFormats();

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
    loadAdvancedSettings();

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
    DialogUtils::saveDialogSize(this, NetworkSettings::csaGameDialogSize);
}

// シグナル・スロットの接続を行う
void CsaGameDialog::connectSignalsAndSlots()
{
    connect(ui->radioButtonEngine, &QRadioButton::toggled,
            this, &CsaGameDialog::updateFormState);
    connect(ui->comboBoxEngine, &QComboBox::currentIndexChanged,
            this, &CsaGameDialog::updateFormState);
    for (auto* input : {ui->lineEditHost, ui->lineEditId, ui->lineEditPassword,
                        ui->lineEditAutoSaveDir, ui->lineEditGameName}) {
        connect(input, &QLineEdit::textChanged, this, &CsaGameDialog::updateFormState);
    }
    for (QAbstractButton* toggle : std::initializer_list<QAbstractButton*>{
             ui->checkBoxKeepAlive, ui->checkBoxAutoSave, ui->radioButtonGamesCount}) {
        connect(toggle, &QAbstractButton::toggled, this, &CsaGameDialog::updateFormState);
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

    // 自動保存先の参照
    connect(ui->pushButtonAutoSaveDir, &QPushButton::clicked,
            this, &CsaGameDialog::onAutoSaveDirBrowse);

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

void CsaGameDialog::populateAutoSaveFormats()
{
    ui->comboBoxAutoSaveFormat->clear();
    ui->comboBoxAutoSaveFormat->addItem(tr("KIF (Shift_JIS, .kif)"), QStringLiteral("kif"));
    ui->comboBoxAutoSaveFormat->addItem(tr("KIF (UTF-8, .kifu)"), QStringLiteral("kifu"));
    ui->comboBoxAutoSaveFormat->addItem(tr("KI2 (.ki2)"), QStringLiteral("ki2"));
    ui->comboBoxAutoSaveFormat->addItem(tr("CSA (.csa)"), QStringLiteral("csa"));
    ui->comboBoxAutoSaveFormat->addItem(tr("JKF (.jkf)"), QStringLiteral("jkf"));
    ui->comboBoxAutoSaveFormat->addItem(tr("USI (.usi)"), QStringLiteral("usi"));
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
    // CSAサーバープロトコル 3.1: ID は英数字・「_」・「-」で32バイト以内、
    // パスワードは空白を含まない32バイト以内の7ビットASCII
    static const QRegularExpression idPattern(QStringLiteral("^[0-9A-Za-z_-]{1,32}$"));
    if (!idPattern.match(id).hasMatch()) {
        return tr("IDは英数字・「_」・「-」のみを使い、32文字以内で入力してください。");
    }
    static const QRegularExpression passwordPattern(QStringLiteral("^[\\x21-\\x7e]{1,32}$"));
    if (!passwordPattern.match(password).hasMatch()) {
        return tr("パスワードは半角の英数字・記号のみを使い、32文字以内で入力してください。");
    }
    const QString gameName = ui->lineEditGameName->text().trimmed();
    if (!gameName.isEmpty() && !gameNamePattern().match(gameName).hasMatch()) {
        return tr("ゲーム名は「名前-持時間-秒読み」の形式で入力してください（例: floodgate-300-10F）。");
    }
    if (ui->checkBoxAutoSave->isChecked() && ui->lineEditAutoSaveDir->text().trimmed().isEmpty()) {
        return tr("棋譜の自動保存先フォルダを指定してください。");
    }
    return {};
}

void CsaGameDialog::updateFormState()
{
    const bool engine = ui->radioButtonEngine->isChecked();
    ui->comboBoxEngine->setEnabled(engine && ui->comboBoxEngine->count() > 0);
    ui->pushButtonEngineSettings->setEnabled(engine && ui->comboBoxEngine->currentIndex() >= 0);
    ui->spinBoxThinkingMargin->setEnabled(engine);
    ui->labelThinkingMargin->setEnabled(engine);
    ui->spinBoxKeepAliveInterval->setEnabled(ui->checkBoxKeepAlive->isChecked());
    ui->labelKeepAliveInterval->setEnabled(ui->checkBoxKeepAlive->isChecked());
    ui->spinBoxGameCount->setEnabled(ui->radioButtonGamesCount->isChecked());
    const bool autoSave = ui->checkBoxAutoSave->isChecked();
    for (QWidget* w : std::initializer_list<QWidget*>{ui->labelAutoSaveDir, ui->lineEditAutoSaveDir,
                                                     ui->pushButtonAutoSaveDir, ui->labelAutoSaveFormat,
                                                     ui->comboBoxAutoSaveFormat}) {
        w->setEnabled(autoSave);
    }
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

    m_keepAliveEnabled = ui->checkBoxKeepAlive->isChecked();
    m_keepAliveIntervalSec = ui->spinBoxKeepAliveInterval->value();
    m_tcpKeepAliveEnabled = ui->checkBoxTcpKeepAlive->isChecked();
    m_thinkingMarginSec = ui->spinBoxThinkingMargin->value();
    m_gameCountMode = ui->radioButtonGamesUnlimited->isChecked() ? GameCountMode::Unlimited
                    : ui->radioButtonGamesCount->isChecked() ? GameCountMode::Count
                    : GameCountMode::Single;
    m_gameCount = ui->spinBoxGameCount->value();
    m_reconnectAfterGame = ui->checkBoxReconnect->isChecked();
    m_autoSaveEnabled = ui->checkBoxAutoSave->isChecked();
    m_autoSaveDir = ui->lineEditAutoSaveDir->text().trimmed();
    m_autoSaveFormat = ui->comboBoxAutoSaveFormat->currentData().toString();
    m_autoFlipBoard = ui->checkBoxAutoFlip->isChecked();
    m_gameName = ui->lineEditGameName->text().trimmed();
    m_turnPreference = static_cast<TurnPreference>(ui->comboBoxTurnPreference->currentIndex());

    // 設定を保存
    saveGameSettings();
    saveAdvancedSettings();
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

void CsaGameDialog::onAutoSaveDirBrowse()
{
    const QString current = ui->lineEditAutoSaveDir->text().trimmed();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("棋譜の自動保存先"), current.isEmpty() ? QDir::homePath() : current);
    if (!dir.isEmpty()) ui->lineEditAutoSaveDir->setText(QDir::toNativeSeparators(dir));
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

QString CsaGameDialog::loginPassword() const
{
    if (m_gameName.isEmpty()) return m_password;
    // shogi-server: 「ゲーム名[-b|-w],パスワード」をパスワード欄で受け付ける
    QString token = m_gameName;
    if (m_turnPreference == TurnPreference::Black) token += QStringLiteral("-b");
    else if (m_turnPreference == TurnPreference::White) token += QStringLiteral("-w");
    return token + QLatin1Char(',') + m_password;
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

int CsaGameDialog::gameLimit() const
{
    switch (m_gameCountMode) {
    case GameCountMode::Count: return qMax(1, m_gameCount);
    case GameCountMode::Unlimited: return 0;
    case GameCountMode::Single: break;
    }
    return 1;
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
