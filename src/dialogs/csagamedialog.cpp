/// @file csagamedialog.cpp
/// @brief CSA対局ダイアログクラスの実装

#include "csagamedialog.h"
#include "dialogfontscale.h"
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
#include "settingscommon.h"
#include <QSettings>

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
{
    ui->setupUi(this);
    // 機密性は表示モードから独立させる。GUIで表示しても自動化APIへは公開しない。
    ui->lineEditPassword->setProperty("automationSensitive", true);
    ui->checkBoxShowPassword->setProperty("automationSensitive", true);
    populateAutoSaveFormats();

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

    // 文字の大きさ（A-/A+ は .ui に置いたボタン）
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->toolButtonFontDecrease;
    fontOptions.increase = ui->toolButtonFontIncrease;
    DialogFontScale::install(this, NetworkSettings::csaGameDialogFontSize, fontOptions);

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
    connect(ui->comboBoxTransport, &QComboBox::currentIndexChanged,
            this, &CsaGameDialog::updateFormState);
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
    ui->labelPlaintextWarning->setVisible(!useTls());
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
    ui->comboBoxTransport->setCurrentIndex(history.useTls ? 1 : 0);
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

// ========== 選んだ値 ==========

CsaGameDialog::Selection CsaGameDialog::selection() const
{
    Selection selection;
    CsaGameCoordinator::StartOptions& start = selection.start;
    start.host = ui->lineEditHost->text().trimmed();
    start.port = ui->spinBoxPort->value();
    start.transport = useTls() ? CsaClient::Transport::Tls : CsaClient::Transport::Plain;
    start.username = ui->lineEditId->text().trimmed();
    // shogi-server のゲーム名と手番の希望は「ゲーム名[-b|-w],パスワード」の形でパスワード欄に載せる
    const QString password = ui->lineEditPassword->text();
    const QString gameName = ui->lineEditGameName->text().trimmed();
    if (gameName.isEmpty()) {
        start.password = password;
    } else {
        QString token = gameName;
        const auto turn = static_cast<TurnPreference>(ui->comboBoxTurnPreference->currentIndex());
        if (turn == TurnPreference::Black) token += QStringLiteral("-b");
        else if (turn == TurnPreference::White) token += QStringLiteral("-w");
        start.password = token + QLatin1Char(',') + password;
    }
    if (ui->radioButtonHuman->isChecked()) {
        start.playerType = CsaGameCoordinator::PlayerType::Human;
    } else {
        start.playerType = CsaGameCoordinator::PlayerType::Engine;
        start.engineName = ui->comboBoxEngine->currentText();
        start.enginePath = m_engineList.value(ui->comboBoxEngine->currentIndex()).path;
    }
    start.keepAliveEnabled = ui->checkBoxKeepAlive->isChecked();
    start.keepAliveIntervalMs = ui->spinBoxKeepAliveInterval->value() * 1000;
    start.tcpKeepAliveEnabled = ui->checkBoxTcpKeepAlive->isChecked();
    start.thinkingMarginMs = ui->spinBoxThinkingMargin->value() * 1000;
    switch (gameCountMode()) {
    case GameCountMode::Count: start.gameLimit = qMax(1, ui->spinBoxGameCount->value()); break;
    case GameCountMode::Unlimited: start.gameLimit = 0; break;
    case GameCountMode::Single: start.gameLimit = 1; break;
    }
    start.reconnectAfterGame = ui->checkBoxReconnect->isChecked();

    selection.start.autoSave.enabled = ui->checkBoxAutoSave->isChecked();
    selection.start.autoSave.dir = ui->lineEditAutoSaveDir->text().trimmed();
    const QString format = ui->comboBoxAutoSaveFormat->currentData().toString();
    if (!format.isEmpty()) selection.start.autoSave.extension = format;
    selection.autoFlipBoard = ui->checkBoxAutoFlip->isChecked();
    return selection;
}

bool CsaGameDialog::useTls() const
{
    return ui->comboBoxTransport->currentIndex() == 1;
}

CsaGameDialog::GameCountMode CsaGameDialog::gameCountMode() const
{
    if (ui->radioButtonGamesUnlimited->isChecked()) return GameCountMode::Unlimited;
    if (ui->radioButtonGamesCount->isChecked()) return GameCountMode::Count;
    return GameCountMode::Single;
}

// ============================================================
// CSA対局ダイアログ - 接続履歴と設定の読み書き
// ============================================================

namespace {
// 接続履歴は配列で保存するため、宣言の表ではなくここでキーを扱う。
constexpr auto kCsaServerHistoryArray = "CsaServerHistory";
constexpr auto kKeyHost = "host";
constexpr auto kKeyPort = "port";
constexpr auto kKeyId = "id";
constexpr auto kKeyUseTls = "useTls";
} // namespace

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
        history.useTls = settings.value(kKeyUseTls, false).toBool();
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
    current.useTls = useTls();
    current.id = ui->lineEditId->text().trimmed();

    // 空の場合は保存しない
    if (current.host.isEmpty() || current.id.isEmpty()) {
        return;
    }

    // 同じ設定が既にあるか確認し、あれば削除（最新を先頭に移動するため）
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        const ServerHistory& h = m_serverHistory.at(i);
        if (h.host == current.host && h.port == current.port && h.id == current.id && h.useTls == current.useTls) {
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
        settings.setValue(kKeyUseTls, h.useTls);
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
    // 対局者設定
    const bool isHuman = NetworkSettings::csaPlayerIsHuman();
    ui->radioButtonHuman->setChecked(isHuman);
    ui->radioButtonEngine->setChecked(!isHuman);

    int engineIndex = ui->comboBoxEngine->findText(NetworkSettings::csaEngineName());
    if (engineIndex < 0) engineIndex = NetworkSettings::csaEngineNumber();
    if (engineIndex >= 0 && engineIndex < ui->comboBoxEngine->count()) {
        ui->comboBoxEngine->setCurrentIndex(engineIndex);
    }

    // サーバー設定
    ui->lineEditHost->setText(NetworkSettings::csaHost());
    ui->spinBoxPort->setValue(NetworkSettings::csaPort());
    ui->comboBoxTransport->setCurrentIndex(NetworkSettings::csaUseTls() ? 1 : 0);
    ui->lineEditId->setText(NetworkSettings::csaId());
    ui->lineEditPassword->clear();
    NetworkSettings::csaLegacyPassword.remove();
}

// 対局設定を設定ファイルに保存する
void CsaGameDialog::saveGameSettings()
{
    // 対局者設定
    NetworkSettings::csaPlayerIsHuman.set(ui->radioButtonHuman->isChecked());
    NetworkSettings::csaEngineName.set(ui->comboBoxEngine->currentText());
    NetworkSettings::csaEngineNumber.set(ui->comboBoxEngine->currentIndex());

    // サーバー設定
    NetworkSettings::csaHost.set(ui->lineEditHost->text().trimmed());
    NetworkSettings::csaPort.set(ui->spinBoxPort->value());
    NetworkSettings::csaUseTls.set(useTls());
    NetworkSettings::csaId.set(ui->lineEditId->text().trimmed());
    NetworkSettings::csaLegacyPassword.remove();
}

void CsaGameDialog::loadAdvancedSettings()
{
    ui->checkBoxKeepAlive->setChecked(NetworkSettings::csaKeepAliveEnabled());
    ui->spinBoxKeepAliveInterval->setValue(NetworkSettings::csaKeepAliveIntervalSec());
    ui->checkBoxTcpKeepAlive->setChecked(NetworkSettings::csaTcpKeepAliveEnabled());
    ui->spinBoxThinkingMargin->setValue(NetworkSettings::csaThinkingMarginSec());

    const int mode = NetworkSettings::csaGameCountMode();
    ui->radioButtonGamesSingle->setChecked(mode == static_cast<int>(GameCountMode::Single));
    ui->radioButtonGamesCount->setChecked(mode == static_cast<int>(GameCountMode::Count));
    ui->radioButtonGamesUnlimited->setChecked(mode == static_cast<int>(GameCountMode::Unlimited));
    ui->spinBoxGameCount->setValue(NetworkSettings::csaGameCount());
    ui->checkBoxReconnect->setChecked(NetworkSettings::csaReconnectAfterGame());
    ui->checkBoxAutoFlip->setChecked(NetworkSettings::csaAutoFlipBoard());

    ui->checkBoxAutoSave->setChecked(NetworkSettings::csaAutoSaveEnabled());
    ui->lineEditAutoSaveDir->setText(NetworkSettings::csaAutoSaveDir());
    const int formatIndex = ui->comboBoxAutoSaveFormat->findData(NetworkSettings::csaAutoSaveFormat());
    ui->comboBoxAutoSaveFormat->setCurrentIndex(qMax(0, formatIndex));

    ui->lineEditGameName->setText(NetworkSettings::csaGameName());
    ui->comboBoxTurnPreference->setCurrentIndex(qBound(0, NetworkSettings::csaTurnPreference(), 2));
}

void CsaGameDialog::saveAdvancedSettings()
{
    NetworkSettings::csaKeepAliveEnabled.set(ui->checkBoxKeepAlive->isChecked());
    NetworkSettings::csaKeepAliveIntervalSec.set(ui->spinBoxKeepAliveInterval->value());
    NetworkSettings::csaTcpKeepAliveEnabled.set(ui->checkBoxTcpKeepAlive->isChecked());
    NetworkSettings::csaThinkingMarginSec.set(ui->spinBoxThinkingMargin->value());
    NetworkSettings::csaGameCountMode.set(static_cast<int>(gameCountMode()));
    NetworkSettings::csaGameCount.set(ui->spinBoxGameCount->value());
    NetworkSettings::csaReconnectAfterGame.set(ui->checkBoxReconnect->isChecked());
    NetworkSettings::csaAutoFlipBoard.set(ui->checkBoxAutoFlip->isChecked());
    NetworkSettings::csaAutoSaveEnabled.set(ui->checkBoxAutoSave->isChecked());
    NetworkSettings::csaAutoSaveDir.set(ui->lineEditAutoSaveDir->text().trimmed());
    NetworkSettings::csaAutoSaveFormat.set(ui->comboBoxAutoSaveFormat->currentData().toString());
    NetworkSettings::csaGameName.set(ui->lineEditGameName->text().trimmed());
    NetworkSettings::csaTurnPreference.set(ui->comboBoxTurnPreference->currentIndex());
}
