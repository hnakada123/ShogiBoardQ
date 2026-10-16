/// @file startgamedialog.cpp
/// @brief 対局開始ダイアログクラスの実装

#include "startgamedialog.h"
#include "dialogfontscale.h"
#include "ui_startgamedialog.h"
#include "changeenginesettingsdialog.h"
#include "gamesettings.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "playersetupwidget.h"
#include "timecontroleditor.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <utility>
#include "timecontrolvalidator.h"

namespace {
namespace Saved = GameSettings::StartGame;

QString defaultKifuSaveDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

constexpr qint64 kSecondMs = 1000;

/// 保存した時間（時間・分・秒で保存している）
TimeControlSide savedTime(const Saved::Player& saved)
{
    TimeControlSide time;
    time.mainMs = (static_cast<qint64>(saved.basicTimeHour()) * 3600 + saved.basicTimeMinutes() * 60) * kSecondMs;
    time.byoyomiMs = saved.byoyomiSec() * kSecondMs;
    time.incrementMs = saved.addEachMoveSec() * kSecondMs;
    return time;
}

void saveTime(const Saved::Player& saved, const TimeControlSide& time)
{
    saved.basicTimeHour.set(static_cast<int>(time.mainMs / (3600 * kSecondMs)));
    saved.basicTimeMinutes.set(static_cast<int>(time.mainMs % (3600 * kSecondMs) / (60 * kSecondMs)));
    saved.byoyomiSec.set(static_cast<int>(time.byoyomiMs / kSecondMs));
    saved.addEachMoveSec.set(static_cast<int>(time.incrementMs / kSecondMs));
}

TimeControlSide defaultTime(const Saved::Player& saved)
{
    TimeControlSide time;
    time.mainMs = (static_cast<qint64>(saved.basicTimeHour.defaultValue()) * 3600
                   + saved.basicTimeMinutes.defaultValue() * 60) * kSecondMs;
    time.byoyomiMs = saved.byoyomiSec.defaultValue() * kSecondMs;
    time.incrementMs = saved.addEachMoveSec.defaultValue() * kSecondMs;
    return time;
}

/// 以前の版が保存した開始局面の番号（0=現在の局面、1..14 はこの並び）の手合割。範囲外なら平手
const Handicaps::Handicap* legacyStartingPosition(int number)
{
    static constexpr const char16_t* kLegacyOrder[] = {
        u"平手", u"香落ち", u"右香落ち", u"角落ち", u"飛車落ち", u"飛香落ち", u"二枚落ち",
        u"三枚落ち", u"四枚落ち", u"五枚落ち", u"左五枚落ち", u"六枚落ち", u"八枚落ち", u"十枚落ち",
    };
    if (number == 0) return nullptr;
    const int index = number >= 1 && number <= static_cast<int>(std::size(kLegacyOrder)) ? number - 1 : 0;
    return Handicaps::byLabel(QStringView(kLegacyOrder[index]));
}

/// 「現在の局面」の項目の値（手合割の項目は Handicaps::kAll の添字）
constexpr int kCurrentPosition = -1;
} // namespace

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

    buildPlayerSections();
    populateStartingPositions();
    bindSettings();

    // ボタンの配色を設定
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

void StartGameDialog::buildPlayerSections()
{
    const std::array<QGroupBox*, 2> playerGroups{ui->groupBox, ui->groupBox_2};
    const std::array<QGroupBox*, 2> timeGroups{ui->groupBox_7, ui->groupBoxSecondPlayerTimeSettings};
    const std::array<const Saved::Player*, 2> saved{&Saved::player1, &Saved::player2};
    for (std::size_t i = 0; i < 2; ++i) {
        const int side = static_cast<int>(i) + 1;
        m_players[i] = new PlayerSetupWidget(side, *saved[i], playerGroups[i]);
        playerGroups[i]->layout()->addWidget(m_players[i]);
        m_times[i] = new TimeControlEditor(side, timeGroups[i]);
        timeGroups[i]->layout()->addWidget(m_times[i]);
    }
    // 対局者・時間の欄は .ui の後の部品より前に Tab で回る
    QWidget::setTabOrder(m_players[1], m_times[0]);
    QWidget::setTabOrder(m_times[0], ui->groupBoxSecondPlayerTimeSettings);
    QWidget::setTabOrder(ui->groupBoxSecondPlayerTimeSettings, m_times[1]);
    QWidget::setTabOrder(m_times[1], ui->comboBoxStartingPosition);
}

void StartGameDialog::populateStartingPositions()
{
    // 手合割の表（Handicaps::kAll）から作る。表示は棋譜の手合割と同じ訳
    ui->comboBoxStartingPosition->addItem(tr("現在の局面"), kCurrentPosition);
    for (int i = 0; i < static_cast<int>(std::size(Handicaps::kAll)); ++i) {
        ui->comboBoxStartingPosition->addItem(
            QCoreApplication::translate("KifuPresentation", Handicaps::kAll[i].label), i);
    }
}

void StartGameDialog::bindSettings()
{
    m_form.add(ui->groupBoxSecondPlayerTimeSettings, Saved::separateSecondPlayerTime);
    m_form.add(ui->spinBoxMaxMoves, Saved::maxMoves);
    m_form.add(ui->spinBoxConsecutiveGames, Saved::consecutiveGames);
    m_form.add(ui->checkBoxShowHumanInFront, Saved::showHumanInFront);
    m_form.add(ui->checkBoxAutoSaveKifu, Saved::autoSaveKifu);
    m_form.add(ui->lineEditKifuSaveDir, Saved::kifuSaveDir, &defaultKifuSaveDir);
    m_form.add(ui->checkBoxLoseOnTimeOut, Saved::loseOnTimeout);
    m_form.add(ui->checkBoxSwitchTurnEachGame, Saved::switchTurnEachGame);
    m_form.add(ui->comboBoxJishogi, Saved::jishogiRule);
}

// ============================================================
// シグナル接続
// ============================================================

void StartGameDialog::connectSignalsAndSlots()
{
    // エンジン設定ボタン・先後入れ替え・リセット・保存
    for (PlayerSetupWidget* player : m_players) {
        connect(player, &PlayerSetupWidget::engineSettingsRequested, this, &StartGameDialog::showEngineSettingsDialog);
        connect(player, &PlayerSetupWidget::kindChanged, this, &StartGameDialog::updateConsecutiveGamesEnabled);
    }
    connect(ui->pushButtonSwapSides, &QPushButton::clicked, this, &StartGameDialog::swapSides);
    connect(ui->pushButtonResetToDefault, &QPushButton::clicked, this, &StartGameDialog::resetSettingsToDefault);
    connect(ui->pushButtonSaveSettingsOnly, &QPushButton::clicked, this, &StartGameDialog::saveSettingsOnly);

    // OK/キャンセル: OKはパラメータ取得→時間設定の検証→設定保存→ダイアログ閉じの順で実行される
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StartGameDialog::onStartRequested);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &StartGameDialog::reject);

    // 秒読みと加算は両者で同じ方式にする（一方に値が入ると、両者のもう一方を0にする）
    TimeControlEditor* firstTime = m_times.front();
    TimeControlEditor* secondTime = m_times.back();
    connect(firstTime, &TimeControlEditor::byoyomiEntered, secondTime, &TimeControlEditor::clearIncrement);
    connect(secondTime, &TimeControlEditor::byoyomiEntered, firstTime, &TimeControlEditor::clearIncrement);
    connect(firstTime, &TimeControlEditor::incrementEntered, secondTime, &TimeControlEditor::clearByoyomi);
    connect(secondTime, &TimeControlEditor::incrementEntered, firstTime, &TimeControlEditor::clearByoyomi);

    // 棋譜保存先ディレクトリ選択
    connect(ui->pushButtonSelectKifuDir, &QPushButton::clicked, this, &StartGameDialog::onSelectKifuDirClicked);
    connect(ui->checkBoxAutoSaveKifu, &QCheckBox::toggled, this, &StartGameDialog::updateKifuSaveEnabled);
    connect(ui->groupBoxSecondPlayerTimeSettings, &QGroupBox::toggled,
            this, &StartGameDialog::updateTimeSettingsTitle);
    connect(ui->spinBoxConsecutiveGames, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StartGameDialog::updateConsecutiveGamesEnabled);
}

// ============================================================
// 先後入れ替え
// ============================================================

void StartGameDialog::swapSides()
{
    const PlayerChoice first = m_players[0]->choice();
    m_players[0]->setChoice(m_players[1]->choice());
    m_players[1]->setChoice(first);

    // 「後手に異なる時間を設定」が有効な場合のみ持ち時間も入れ替え
    if (ui->groupBoxSecondPlayerTimeSettings->isChecked()) {
        const TimeControlSide firstTime = m_times[0]->value();
        m_times[0]->setValue(m_times[1]->value());
        m_times[1]->setValue(firstTime);
    }
}

// ============================================================
// エンジン設定ダイアログ
// ============================================================

void StartGameDialog::showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine)
{
    ChangeEngineSettingsDialog dialog(engine, this);
    dialog.exec();
}

// ============================================================
// 選んだ値
// ============================================================

void StartGameDialog::forceCurrentPositionSelection()
{
    ui->comboBoxStartingPosition->setCurrentIndex(0);
}

StartGameDialogData StartGameDialog::data() const
{
    StartGameDialogData data;
    const bool separate = ui->groupBoxSecondPlayerTimeSettings->isChecked();
    for (std::size_t i = 0; i < 2; ++i) {
        data.players[i] = m_players[i]->choice();
        // 「後手に異なる時間を設定」がなければ先手と同じ
        data.players[i].time = m_times[separate ? i : std::size_t{0}]->value();
    }
    data.isLoseOnTimeout = ui->checkBoxLoseOnTimeOut->isChecked();

    const int position = ui->comboBoxStartingPosition->currentData().toInt();
    data.startingHandicap = position == kCurrentPosition ? nullptr : &Handicaps::kAll[position];
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
// 表示の更新
// ============================================================

void StartGameDialog::updateConsecutiveGamesEnabled()
{
    // 両方がエンジンの場合のみ連続対局を有効にする
    const bool enableConsecutive = m_players[0]->isEngine() && m_players[1]->isEngine();

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
        currentDir = defaultKifuSaveDir();
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
// 設定保存・読込・既定値
// ============================================================

void StartGameDialog::saveGameSettings()
{
    const std::array<const Saved::Player*, 2> saved{&Saved::player1, &Saved::player2};
    for (std::size_t i = 0; i < 2; ++i) {
        m_players[i]->save();
        saveTime(*saved[i], m_times[i]->value());
    }
    // 開始局面は手合割の名前で保存する（「現在の局面」は空）
    const int position = ui->comboBoxStartingPosition->currentData().toInt();
    Saved::startingPositionName.set(position == kCurrentPosition ? QString()
                                                                 : Handicaps::labelOf(Handicaps::kAll[position]));
    Saved::startingPositionNumber.remove();
    m_form.save();
}

void StartGameDialog::loadGameSettings()
{
    const std::array<const Saved::Player*, 2> saved{&Saved::player1, &Saved::player2};
    for (std::size_t i = 0; i < 2; ++i) m_times[i]->setValue(savedTime(*saved[i]));
    m_form.load();

    // 開始局面: 手合割の名前（空は現在の局面）。以前の版は番号（と表示した名前）で保存していた
    const Handicaps::Handicap* handicap = &Handicaps::kAll[0];
    if (Saved::startingPositionNumber.isSaved()) {
        handicap = legacyStartingPosition(Saved::startingPositionNumber());
    } else {
        const QString name = Saved::startingPositionName();
        handicap = name.isEmpty() ? nullptr : Handicaps::byLabel(name);
        if (!handicap && !name.isEmpty()) handicap = &Handicaps::kAll[0];
    }
    ui->comboBoxStartingPosition->setCurrentIndex(
        handicap ? ui->comboBoxStartingPosition->findData(static_cast<int>(handicap - Handicaps::kAll)) : 0);

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    // 先手は人間、後手は先頭の登録エンジン（無ければ人間）
    m_players[0]->reset(false);
    m_players[1]->reset(true);
    m_times[0]->setValue(defaultTime(Saved::player1));
    m_times[1]->setValue(defaultTime(Saved::player2));
    m_form.reset();
    // 開始局面は平手
    ui->comboBoxStartingPosition->setCurrentIndex(1);
}

// ============================================================
// 対局開始ダイアログ - 時間設定の確認
// ============================================================

bool StartGameDialog::confirmTimeSettings()
{
    using TimeControlValidator::Issue;
    const StartGameDialogData d = data();
    const bool hasEngine = d.hasEngine();
    const Issue issue = TimeControlValidator::validate(d.players[0].time, d.players[1].time, hasEngine);

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
