/// @file csagamedialog.cpp
/// @brief CSA対局ダイアログクラスの実装

#include "csagamedialog.h"
#include "dialogfontscale.h"
#include "ui_csagamedialog.h"
#include "changeenginesettingsdialog.h"
#include "engineselector.h"
#include "networksettings.h"
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
{
    ui->setupUi(this);
    // 機密性は表示モードから独立させる。GUIで表示しても自動化APIへは公開しない。
    ui->lineEditPassword->setProperty("automationSensitive", true);
    ui->checkBoxShowPassword->setProperty("automationSensitive", true);
    populateAutoSaveFormats();

    // エンジンの一覧と前回選んだエンジン（名前で保存。以前の版の番号も読む）
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::csaGameSelection();
    m_engineSelector = new EngineSelector({ui->comboBoxEngine, ui->pushButtonEngineSettings, nullptr},
                                          engineOptions, this);

    // 設定ファイルからサーバー接続履歴を読み込む
    loadServerHistory();

    // UIにサーバー履歴を反映する
    populateUIWithServerHistory();

    // 設定ファイルから対局設定を読み込む
    loadGameSettings();

    // シグナル・スロットの接続を行う
    connectSignalsAndSlots();
    updateFormState();

    // 文字の大きさ（A-/A+ は .ui に置いたボタン）
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = ui->toolButtonFontDecrease;
    fontOptions.increase = ui->toolButtonFontIncrease;
    fontOptions.size = &NetworkSettings::csaGameDialogSize;
    DialogFontScale::install(this, NetworkSettings::csaGameDialogFontSize, fontOptions);
    if (ui->lineEditHost->text().isEmpty()) ui->lineEditHost->setFocus();
    else if (ui->lineEditId->text().isEmpty()) ui->lineEditId->setFocus();
    else ui->lineEditPassword->setFocus();
}

CsaGameDialog::~CsaGameDialog() = default;

// シグナル・スロットの接続を行う
void CsaGameDialog::connectSignalsAndSlots()
{
    connect(ui->radioButtonEngine, &QRadioButton::toggled,
            this, &CsaGameDialog::updateFormState);
    connect(m_engineSelector, &EngineSelector::selectionChanged, this, &CsaGameDialog::updateFormState);
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
    connect(m_engineSelector, &EngineSelector::settingsRequested, this, &CsaGameDialog::showEngineSettingsDialog);

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
    {
        // 履歴の再設定で入力欄を書き換えないよう、選択変更の通知を止める
        const QSignalBlocker engineBlocker(m_engineSelector);
        const QSignalBlocker historyBlocker(ui->comboBoxHistory);
        m_engineSelector->reload();
        loadServerHistory();
        populateUIWithServerHistory();
    }
    updateFormState();
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
    if (ui->radioButtonEngine->isChecked() && !m_engineSelector->selected()) {
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
    m_engineSelector->setEnabled(engine);
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
    saveServerHistory();

    // ダイアログを受け入れて閉じる
    accept();
}

// サーバー履歴が選択された時の処理
void CsaGameDialog::onServerHistoryChanged(int index)
{
    // インデックス0は新しい接続先なのでスキップ
    if (index <= 0 || index > m_serverHistory.size()) {
        return;
    }

    // 履歴から設定を復元（インデックスは1始まりなので-1する）
    const NetworkSettings::CsaServerHistoryEntry& history = m_serverHistory.at(index - 1);
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
void CsaGameDialog::showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine)
{
    ChangeEngineSettingsDialog dialog(engine, this);
    dialog.exec();
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
        const std::optional<Engine> engine = m_engineSelector->selected();
        start.engineName = engine ? engine->name : QString();
        start.enginePath = engine ? engine->path : QString();
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

void CsaGameDialog::loadServerHistory()
{
    m_serverHistory = NetworkSettings::csaServerHistory();
}

// 現在の設定をサーバー接続履歴の先頭に置いて保存する（同じ接続先は先頭へ移し、古いものから捨てる）
void CsaGameDialog::saveServerHistory()
{
    NetworkSettings::CsaServerHistoryEntry current;
    current.host = ui->lineEditHost->text().trimmed();
    current.port = ui->spinBoxPort->value();
    current.useTls = useTls();
    current.id = ui->lineEditId->text().trimmed();
    if (current.host.isEmpty() || current.id.isEmpty()) return;

    m_serverHistory.removeAll(current);
    m_serverHistory.prepend(current);
    while (m_serverHistory.size() > NetworkSettings::kCsaServerHistoryLimit) m_serverHistory.removeLast();
    NetworkSettings::csaServerHistory.set(m_serverHistory);
}

// UIにサーバー履歴を反映する
void CsaGameDialog::populateUIWithServerHistory()
{
    // 履歴コンボボックスをクリアする
    ui->comboBoxHistory->clear();

    ui->comboBoxHistory->addItem(tr("新しい接続先"));
    ui->comboBoxHistory->setEnabled(!m_serverHistory.isEmpty());

    // 履歴リストからサーバー履歴をコンボボックスに追加する
    for (const NetworkSettings::CsaServerHistoryEntry& history : std::as_const(m_serverHistory)) {
        QString displayText = formatServerHistoryDisplay(history);
        ui->comboBoxHistory->addItem(displayText);
    }
}

// サーバー履歴の表示文字列を生成する
QString CsaGameDialog::formatServerHistoryDisplay(const NetworkSettings::CsaServerHistoryEntry& history) const
{
    return QString("%1:%2 %3").arg(history.host).arg(history.port).arg(history.id);
}

// 設定ファイルから対局設定を読み込む
void CsaGameDialog::loadGameSettings()
{
    // 入力欄と設定の対応（読み込み・保存は同じ表で行う）
    m_form.add(ui->radioButtonEngine, ui->radioButtonHuman, NetworkSettings::csaPlayerIsHuman);
    m_form.add(ui->lineEditHost, NetworkSettings::csaHost);
    m_form.add(ui->spinBoxPort, NetworkSettings::csaPort);
    m_form.add(ui->lineEditId, NetworkSettings::csaId);
    m_form.add(ui->checkBoxKeepAlive, NetworkSettings::csaKeepAliveEnabled);
    m_form.add(ui->spinBoxKeepAliveInterval, NetworkSettings::csaKeepAliveIntervalSec);
    m_form.add(ui->checkBoxTcpKeepAlive, NetworkSettings::csaTcpKeepAliveEnabled);
    m_form.add(ui->spinBoxThinkingMargin, NetworkSettings::csaThinkingMarginSec);
    // 対局数の決め方（GameCountMode の順）
    m_form.add({ui->radioButtonGamesSingle, ui->radioButtonGamesCount, ui->radioButtonGamesUnlimited},
               NetworkSettings::csaGameCountMode);
    m_form.add(ui->spinBoxGameCount, NetworkSettings::csaGameCount);
    m_form.add(ui->checkBoxReconnect, NetworkSettings::csaReconnectAfterGame);
    m_form.add(ui->checkBoxAutoFlip, NetworkSettings::csaAutoFlipBoard);
    m_form.add(ui->checkBoxAutoSave, NetworkSettings::csaAutoSaveEnabled);
    m_form.add(ui->lineEditAutoSaveDir, NetworkSettings::csaAutoSaveDir);
    m_form.add(ui->lineEditGameName, NetworkSettings::csaGameName);
    m_form.add(ui->comboBoxTurnPreference, NetworkSettings::csaTurnPreference);
    m_form.load();

    // 表で表せない欄（接続方式は TLS かどうか、保存の形式は項目のデータ）
    ui->comboBoxTransport->setCurrentIndex(NetworkSettings::csaUseTls() ? 1 : 0);
    const int formatIndex = ui->comboBoxAutoSaveFormat->findData(NetworkSettings::csaAutoSaveFormat());
    ui->comboBoxAutoSaveFormat->setCurrentIndex(qMax(0, formatIndex));
    ui->lineEditPassword->clear();
    NetworkSettings::csaLegacyPassword.remove();
}

// 対局設定を設定ファイルに保存する
void CsaGameDialog::saveGameSettings()
{
    m_form.save();
    m_engineSelector->saveSelection();
    NetworkSettings::csaUseTls.set(useTls());
    NetworkSettings::csaAutoSaveFormat.set(ui->comboBoxAutoSaveFormat->currentData().toString());
    NetworkSettings::csaLegacyPassword.remove();
}
