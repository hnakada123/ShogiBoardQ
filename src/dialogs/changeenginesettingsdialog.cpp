/// @file changeenginesettingsdialog.cpp
/// @brief エンジン設定変更ダイアログクラスの実装

#include "changeenginesettingsdialog.h"
#include "dialogfontscale.h"
#include "enginesettingsoptionhandler.h"
#include "enginedialogsettings.h"
#include "engineprofilestore.h"
#include "buttonstyles.h"
#include "ui_changeenginesettingsdialog.h"

#include <QGroupBox>

namespace {
constexpr QSize kMinimumSize{400, 300};
} // namespace

// 将棋エンジンの設定を変更するダイアログ
ChangeEngineSettingsDialog::ChangeEngineSettingsDialog(const EngineEntry& engine,
                                                       QWidget* parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ChangeEngineSettingsDialog>())
    , m_optionHandler(std::make_unique<EngineSettingsOptionHandler>(this))   // 所有は unique_ptr だけ（QObject の親にしない）
{
    ui->setupUi(this);
    m_optionHandler->setEngineName(engine.name);
    m_optionHandler->setEngineAuthor(engine.author);
    // 保存されているオプションを読み、設定する画面を作る
    m_optionHandler->readEngineOptions();
    createOptionWidgets();
}

ChangeEngineSettingsDialog::~ChangeEngineSettingsDialog() = default;

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

    // ダイアログの最小サイズを設定（リサイズ可能にする）
    this->setMinimumSize(kMinimumSize);

    // 文字の大きさ（作ったオプションの部品を含む）と、閉じたときに保存する大きさ
    DialogFontScale::Options fontOptions;
    fontOptions.maxSize = 20;
    fontOptions.decrease = ui->fontDecreaseButton;
    fontOptions.increase = ui->fontIncreaseButton;
    fontOptions.size = &EngineDialogSettings::engineSettingsDialogSize;
    DialogFontScale::install(this, EngineDialogSettings::engineSettingsFontSize, fontOptions);
}

void ChangeEngineSettingsDialog::createPonderWidgets(QVBoxLayout* layout)
{
    const auto prefs = EngineProfileStore::loadPonder(m_optionHandler->engineName());
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
    EngineProfileStore::savePonder(m_optionHandler->engineName(), m_ponderEnabled->isChecked(),
                                   m_sendUnreportedPonder->isChecked());
    accept();
}

void ChangeEngineSettingsDialog::restoreDefaultSettings()
{
    m_optionHandler->restoreDefaultOptions();
    m_ponderEnabled->setChecked(m_defaultPonderEnabled);
    m_sendUnreportedPonder->setChecked(true);
}
