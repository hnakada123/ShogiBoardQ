/// @file changeenginesettingsdialog.cpp
/// @brief エンジン設定変更ダイアログクラスの実装

#include "changeenginesettingsdialog.h"
#include "enginesettingsoptionhandler.h"
#include "enginedialogsettings.h"
#include "enginepondersettings.h"
#include "dialogutils.h"
#include "buttonstyles.h"
#include "ui_changeenginesettingsdialog.h"

#include <QGroupBox>

namespace {
constexpr QSize kMinimumSize{400, 300};
} // namespace

// 将棋エンジンの設定を変更するダイアログ
ChangeEngineSettingsDialog::ChangeEngineSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ChangeEngineSettingsDialog>())
    , m_optionHandler(std::make_unique<EngineSettingsOptionHandler>(this, this))
    , m_fontHelper({EngineDialogSettings::engineSettingsFontSize(), 8, 20, 1,
                    EngineDialogSettings::setEngineSettingsFontSize})
{
    ui->setupUi(this);
}

ChangeEngineSettingsDialog::~ChangeEngineSettingsDialog()
{
    // ウィンドウサイズを保存
    DialogUtils::saveDialogSize(this, EngineDialogSettings::setEngineSettingsDialogSize);
}

// 将棋エンジン番号のsetter
void ChangeEngineSettingsDialog::setEngineNumber(const int& engineNumber)
{
    m_engineNumber = engineNumber;
}

// 将棋エンジン名のsetter
void ChangeEngineSettingsDialog::setEngineName(const QString& engineName)
{
    m_optionHandler->setEngineName(engineName);
}

// 将棋エンジン作者名のsetter
void ChangeEngineSettingsDialog::setEngineAuthor(const QString& engineAuthor)
{
    m_optionHandler->setEngineAuthor(engineAuthor);
}

// "エンジン設定"ダイアログを作成する。
void ChangeEngineSettingsDialog::setupEngineOptionsDialog()
{
    // 設定ファイルから選択したエンジンのオプションを読み込む。
    m_optionHandler->readEngineOptions();

    // エンジンオプションを設定する画面を作成する。
    createOptionWidgets();
}

// エンジンオプションに基づいてUIコンポーネントを作成して配置する。
void ChangeEngineSettingsDialog::createOptionWidgets()
{
    // 設定画面の上部にエンジン名と作者名を表示する。
    QString headerText = m_optionHandler->engineName();
    if (!m_optionHandler->engineAuthor().isEmpty()) {
        headerText += tr("\n作者: %1").arg(m_optionHandler->engineAuthor());
    }
    ui->label->setText(headerText);
    ui->label->setStyleSheet(
        "QLabel { background-color: transparent; padding: 8px; "
        "font-weight: bold; color: #333333; }");

    // ダイアログ全体の背景をクリーム色にする
    this->setStyleSheet(
        "QDialog { background-color: #fefcf6; }");

    // 画面レイアウトを作成する。
    QVBoxLayout* optionWidgetsLayout = new QVBoxLayout;

    createPonderWidgets(optionWidgetsLayout);

    // ハンドラにオプションウィジェットの生成を委譲する。
    m_optionHandler->buildOptionWidgets(optionWidgetsLayout);

    // レイアウトを上部に配置するためにスペーサーを追加（setAlignmentは水平方向の拡張を制限するため使用しない）
    optionWidgetsLayout->addStretch();

    // ダイアログのスクロールアリアのウィジェットにレイアウトをセットする。
    ui->scrollAreaWidgetContents->setLayout(optionWidgetsLayout);
    ui->scrollArea->setWidget(ui->scrollAreaWidgetContents);

    // ダイアログの最後に適用ボタンを表示する。
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("適用"));

    // "既定値に戻す"ボタンが押された場合、全てのオプションを既定値に戻す。
    connect(ui->restoreButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::restoreDefaultSettings);

    // "適用"ボタンが押された場合、全てのオプションの設定を保存してエンジン設定ダイアログを終了する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ChangeEngineSettingsDialog::saveEngineSettings);

    // "Cancel"ボタンが押された場合、エンジン設定ダイアログを保存せずに終了する。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ボタンスタイルを適用
    ui->restoreButton->setStyleSheet(ButtonStyles::undoRedo());
    ui->fontDecreaseButton->setStyleSheet(ButtonStyles::fontButton());
    ui->fontIncreaseButton->setStyleSheet(ButtonStyles::fontButton());

    // フォントサイズ変更ボタンの接続
    connect(ui->fontIncreaseButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::increaseFontSize);
    connect(ui->fontDecreaseButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::decreaseFontSize);

    // 保存されているフォントサイズを適用
    applyFontSize();

    // ダイアログの最小サイズを設定（リサイズ可能にする）
    this->setMinimumSize(kMinimumSize);

    // 保存されているウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, EngineDialogSettings::engineSettingsDialogSize());
}

void ChangeEngineSettingsDialog::createPonderWidgets(QVBoxLayout* layout)
{
    const auto prefs = EnginePonderSettings::load(m_optionHandler->engineName());
    m_defaultPonderEnabled = prefs.defaultEnabled;
    auto* group = new QGroupBox(tr("先読み"), this);
    auto* groupLayout = new QVBoxLayout(group);
    m_ponderEnabled = new QCheckBox(tr("相手の手番中に先読みする"), group);
    m_ponderEnabled->setObjectName(QStringLiteral("ponderEnabledCheckBox"));
    m_ponderEnabled->setChecked(prefs.enabled);
    m_ponderEnabled->setToolTip(tr("先読みに対応したエンジンで、予測した相手の指し手をもとに思考します。"));
    groupLayout->addWidget(m_ponderEnabled);

    m_sendUnreportedPonder = new QCheckBox(tr("未報告のUSI_Ponderにも設定を送信する"), group);
    m_sendUnreportedPonder->setObjectName(QStringLiteral("sendUnreportedPonderCheckBox"));
    m_sendUnreportedPonder->setChecked(prefs.sendUnreportedOption);
    m_sendUnreportedPonder->setToolTip(tr("通常は有効にしてください。未対応オプションのエラーが出るエンジンでは無効にします。無効にしてもGUIによる先読みは利用できます。"));
    groupLayout->addWidget(m_sendUnreportedPonder);
    layout->addWidget(group);
}

void ChangeEngineSettingsDialog::saveEngineSettings()
{
    m_optionHandler->writeEngineOptions();
    EnginePonderSettings::save(m_optionHandler->engineName(), m_ponderEnabled->isChecked(),
                              m_sendUnreportedPonder->isChecked());
    accept();
}

void ChangeEngineSettingsDialog::restoreDefaultSettings()
{
    m_optionHandler->restoreDefaultOptions();
    m_ponderEnabled->setChecked(m_defaultPonderEnabled);
    m_sendUnreportedPonder->setChecked(true);
}

// フォントサイズを増加する。
void ChangeEngineSettingsDialog::increaseFontSize()
{
    if (m_fontHelper.increase()) applyFontSize();
}

// フォントサイズを減少する。
void ChangeEngineSettingsDialog::decreaseFontSize()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

// すべてのウィジェットにフォントサイズを適用する。
void ChangeEngineSettingsDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontHelper.fontSize());
    DialogUtils::applyFontToAllChildren(this, font);
}
