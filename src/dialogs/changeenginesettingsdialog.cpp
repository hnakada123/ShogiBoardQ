/// @file changeenginesettingsdialog.cpp
/// @brief エンジン設定変更ダイアログクラスの実装

#include "changeenginesettingsdialog.h"
#include "enginesettingsoptionhandler.h"
#include "enginedialogsettings.h"
#include "dialogutils.h"
#include "buttonstyles.h"
#include "ui_changeenginesettingsdialog.h"

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
    connect(ui->restoreButton, &QPushButton::clicked, m_optionHandler.get(), &EngineSettingsOptionHandler::restoreDefaultOptions);

    // "適用"ボタンが押された場合、全てのオプションの設定を保存してエンジン設定ダイアログを終了する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, m_optionHandler.get(), &EngineSettingsOptionHandler::writeEngineOptions);

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
