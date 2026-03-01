/// @file engineregistrationdialog.cpp
/// @brief エンジン登録ダイアログクラスの実装

#include "engineregistrationdialog.h"
#include "engineregistrationhandler.h"
#include "enginedialogsettings.h"
#include "dialogutils.h"
#include "ui_engineregistrationdialog.h"
#include "changeenginesettingsdialog.h"
#include "buttonstyles.h"
#include <memory>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

// 将棋エンジン登録ダイアログを表示する。
EngineRegistrationDialog::EngineRegistrationDialog(QWidget *parent)
    : QDialog(parent),
    ui(std::make_unique<Ui::EngineRegistrationDialog>()),
    m_handler(std::make_unique<EngineRegistrationHandler>(this)),
    m_fontSize(EngineDialogSettings::engineRegistrationFontSize())
{
    // Qt Designerで作成されたUIをプログラムのウィンドウに読み込み、初期化する。
    ui->setupUi(this);

    // シグナル・スロットの接続を行う。
    initializeSignals();

    // 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込む。
    m_handler->loadEnginesFromSettings();

    // GUIのリストウィジェットにエンジン名を追加する。
    for (const Engine& engine : std::as_const(m_handler->engineList())) {
        ui->engineListWidget->addItem(engine.name);
    }

    // ボタンスタイルを適用
    ui->addEngineButton->setStyleSheet(ButtonStyles::editOperation());
    ui->removeEngineButton->setStyleSheet(ButtonStyles::dangerStop());
    ui->configureEngineButton->setStyleSheet(ButtonStyles::primaryAction());
    ui->closeButton->setStyleSheet(ButtonStyles::secondaryNeutral());
    ui->fontDecreaseButton->setStyleSheet(ButtonStyles::fontButton());
    ui->fontIncreaseButton->setStyleSheet(ButtonStyles::fontButton());

    // フォントサイズを適用
    applyFontSize();

    // 保存されているウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, EngineDialogSettings::engineRegistrationDialogSize());
}

EngineRegistrationDialog::~EngineRegistrationDialog()
{
    // 進行中の登録処理をキャンセルする
    m_handler->cancelRegistration();

    // ウィンドウサイズを保存
    DialogUtils::saveDialogSize(this, EngineDialogSettings::setEngineRegistrationDialogSize);
}

// シグナル・スロットの接続を行う。
void EngineRegistrationDialog::initializeSignals() const
{
    // 追加ボタンが押されたときの処理を接続
    connect(ui->addEngineButton, &QPushButton::clicked, this, &EngineRegistrationDialog::addEngineFromFileSelection);

    // 削除ボタンが押されたときの処理を接続
    connect(ui->removeEngineButton, &QPushButton::clicked, this, &EngineRegistrationDialog::removeEngine);

    // 設定ボタンが押されたときの処理を接続
    connect(ui->configureEngineButton, &QPushButton::clicked, this, &EngineRegistrationDialog::configureEngine);

    // 閉じるボタンが押されたときの処理を接続
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // フォントサイズ変更ボタンの接続
    connect(ui->fontIncreaseButton, &QPushButton::clicked, this, &EngineRegistrationDialog::increaseFontSize);
    connect(ui->fontDecreaseButton, &QPushButton::clicked, this, &EngineRegistrationDialog::decreaseFontSize);

    // ハンドラシグナルの接続
    connect(m_handler.get(), &EngineRegistrationHandler::engineRegistered, this, &EngineRegistrationDialog::onEngineRegistered);
    connect(m_handler.get(), &EngineRegistrationHandler::errorOccurred, this, &EngineRegistrationDialog::onHandlerError);
    connect(m_handler.get(), &EngineRegistrationHandler::registrationInProgressChanged, this, &EngineRegistrationDialog::onRegistrationInProgressChanged);
}

// 追加ボタンが押されたときに呼び出されるスロット
// エンジン登録ダイアログで選択したエンジンを追加する。
void EngineRegistrationDialog::addEngineFromFileSelection()
{
    // ファイルの選択ダイアログのタイトル
    const QString fileSelectionDialogTitle = tr("ファイルの選択");

    // ホームディレクトリの取得
    QString homeDir = QDir::homePath();

    // ファイルの選択ダイアログを表示し、選択されたファイル名を取得する。
    QString fileName = QFileDialog::getOpenFileName(this, fileSelectionDialogTitle, homeDir);

    // fileNameのパス区切りを、実行環境のネイティブな形式に変換する。
    fileName = QDir::toNativeSeparators(fileName);

    // ファイルが選択されなかった場合、処理を中断する。
    if (fileName.isEmpty()) {
        return;
    }

    // 既にリストに同じエンジンがあるかどうかをチェック
    QString existingName;
    if (m_handler->isDuplicatePath(fileName, existingName)) {
        // エンジン登録が重複している場合、重複エラーのメッセージを表示する。
        QMessageBox::critical(this, tr("エラー"),
            tr("エンジン %1 は既に追加されています。").arg(existingName));
        return;
    }

    // ファイルのパスが有効でない場合、エラーメッセージを表示し、処理を中断する。
    if (!m_handler->validateEnginePath(fileName)) {
        // エラーメッセージを通知する。
        QMessageBox::critical(this, tr("エラー"),
            tr("ディレクトリ %1 に移動できませんでした。エンジンの追加に失敗しました。").arg(QFileInfo(fileName).path()));
        return;
    }

    // エンジン登録処理を開始する。
    m_handler->startRegistration(fileName);
}

// 削除ボタンが押されたときに呼び出されるスロット
// 選択した登録エンジンを削除する。
void EngineRegistrationDialog::removeEngine()
{
    // 選択したエンジン
    QList<QListWidgetItem *> items = ui->engineListWidget->selectedItems();

    // 選択したエンジンが正確に一つであるかをチェックする。
    if (items.count() != 1) {
        // エラーが発生したことを通知する。
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンを1つ選択してください。"));
        return;
    }

    // ここからは選択されたエンジンが正確に一つであることが保証されている。
    QListWidgetItem* selectedItem = items.first();
    int index = ui->engineListWidget->row(selectedItem);
    std::unique_ptr<QListWidgetItem> takenItem(ui->engineListWidget->takeItem(index));

    // ハンドラにエンジン削除を委譲する。
    m_handler->removeEngineAt(index);
}

// 設定ボタンが押されたときに呼び出されるスロット
// 選択したエンジンの設定を変更する。
void EngineRegistrationDialog::configureEngine()
{
    QList<QListWidgetItem *> items = ui->engineListWidget->selectedItems();

    // 選択されたアイテムが正確に一つであるかをチェックする。
    if (items.count() != 1) {
        // エラーメッセージを通知する。
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンを1つ選択してください。"));
        return;
    }

    // ここからは選択されたアイテムが正確に一つであることが保証されている。
     // 最初（唯一の）アイテムを取得する。
    QListWidgetItem* selectedItem = items.first();

    // 選択されたアイテムのエンジン番号を取得する。
    int engineNumber = ui->engineListWidget->row(selectedItem);

    // 選択されたアイテムのエンジン名を取得する。
    QString engineName = selectedItem->text();

    // 選択されたエンジンの作者名を取得する。
    QString engineAuthor = m_handler->engineAt(engineNumber).author;

    // 選択されたエンジンの設定変更ダイアログを表示する。
    ChangeEngineSettingsDialog dialog(this);

    dialog.setEngineNumber(engineNumber);
    dialog.setEngineName(engineName);
    dialog.setEngineAuthor(engineAuthor);
    dialog.setupEngineOptionsDialog();

    if (dialog.exec() == QDialog::Rejected) return;
}

// エンジン登録完了時のスロット
void EngineRegistrationDialog::onEngineRegistered(const QString& engineName)
{
    // エンジン名をリストに追加する。
    ui->engineListWidget->addItem(engineName);
}

// ハンドラエラー時のスロット
void EngineRegistrationDialog::onHandlerError(const QString& errorMessage)
{
    // エラーメッセージを表示する。
    QMessageBox::critical(this, tr("エラー"), errorMessage);
}

// 登録処理の進行状態変化時のスロット
void EngineRegistrationDialog::onRegistrationInProgressChanged(bool inProgress)
{
    ui->addEngineButton->setEnabled(!inProgress);
    ui->removeEngineButton->setEnabled(!inProgress);
    ui->configureEngineButton->setEnabled(!inProgress);

    if (inProgress) {
        ui->addEngineButton->setText(tr("登録中..."));
    } else {
        ui->addEngineButton->setText(tr("追加"));
    }
}

// フォントサイズを増加する
void EngineRegistrationDialog::increaseFontSize()
{
    if (m_fontSize < 20) {  // 最大サイズを20に制限
        m_fontSize++;
        applyFontSize();
        EngineDialogSettings::setEngineRegistrationFontSize(m_fontSize);
    }
}

// フォントサイズを減少する
void EngineRegistrationDialog::decreaseFontSize()
{
    if (m_fontSize > 8) {  // 最小サイズを8に制限
        m_fontSize--;
        applyFontSize();
        EngineDialogSettings::setEngineRegistrationFontSize(m_fontSize);
    }
}

// すべてのウィジェットにフォントサイズを適用する
void EngineRegistrationDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    this->setFont(font);

    // すべての子ウィジェットにフォントを明示的に設定する（KDE Breezeでは親の設定が伝搬しないため）
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        widget->setFont(font);
    }
}
