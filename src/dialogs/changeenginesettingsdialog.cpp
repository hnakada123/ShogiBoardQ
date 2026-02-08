/// @file changeenginesettingsdialog.cpp
/// @brief エンジン設定変更ダイアログクラスの実装

#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include "engineoptiondescriptions.h"
#include "settingsservice.h"
#include "ui_changeenginesettingsdialog.h"
#include <QApplication>
#include <QSettings>
#include <QGroupBox>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>
#include <functional>

using namespace EngineSettingsConstants;

// 将棋エンジンの設定を変更するダイアログ
ChangeEngineSettingsDialog::ChangeEngineSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChangeEngineSettingsDialog)
    , m_fontSize(SettingsService::engineSettingsFontSize())
{
    ui->setupUi(this);
}

ChangeEngineSettingsDialog::~ChangeEngineSettingsDialog()
{
    // ウィンドウサイズを保存
    SettingsService::setEngineSettingsDialogSize(this->size());

    delete ui;
}

// 将棋エンジン番号のsetter
void ChangeEngineSettingsDialog::setEngineNumber(const int& engineNumber)
{
    m_engineNumber = engineNumber;
}

// 将棋エンジン名のsetter
void ChangeEngineSettingsDialog::setEngineName(const QString& engineName)
{
    m_engineName = engineName;
}

// 将棋エンジン作者名のsetter
void ChangeEngineSettingsDialog::setEngineAuthor(const QString& engineAuthor)
{
    m_engineAuthor = engineAuthor;
}

// 設定ファイルから選択したエンジンのオプションを読み込む。
void ChangeEngineSettingsDialog::readEngineOptions()
{
    // オプションリストを初期化する。
    m_optionList.clear();

    // アプリケーションのディレクトリに移動してから設定ファイルを操作する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // エンジンオプションの数を取得する。
    int size = settings.beginReadArray(m_engineName);

    // エンジンオプションの数だけ繰り返す。
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);

        // オプションリストに読み込んだオプションを追加する。
        m_optionList.append(readOption(settings));
    }

    // エンジン名のグループの配列の読み込みを終了する。
    settings.endArray();
}

// 一つのオプションを読み込む。
EngineOption ChangeEngineSettingsDialog::readOption(const QSettings& settings) const
{
    EngineOption option;

    option.name = settings.value(EngineOptionNameKey).toString();
    option.type = settings.value(EngineOptionTypeKey).toString();
    option.defaultValue = settings.value(EngineOptionDefaultKey).toString();
    option.min = settings.value(EngineOptionMinKey).toString();
    option.max = settings.value(EngineOptionMaxKey).toString();
    option.currentValue = settings.value(EngineOptionValueKey).toString();
    QString valueListStr = settings.value(EngineOptionValueListKey).toString();

    // 空の文字列をスキップ
    option.valueList = valueListStr.split(" ", Qt::SkipEmptyParts);

    return option;
}

// "エンジン設定"ダイアログを作成する。
void ChangeEngineSettingsDialog::setupEngineOptionsDialog()
{
    // 設定ファイルから選択したエンジンのオプションを読み込む。
    readEngineOptions();

    // エンジンオプションを設定する画面を作成する。
    createOptionWidgets();
}

// 各ボタンが押されているかどうかを確認し、ボタンの色を設定する。
void ChangeEngineSettingsDialog::changeStatusColorTypeButton()
{
    // 並列配列のサイズ整合性を確認
    if (m_optionList.size() != m_engineOptionWidgetsList.size()) {
        qWarning() << "changeStatusColorTypeButton: リストサイズ不一致";
        return;
    }

    // オプション数までループする。
    for (qsizetype i = 0; i < m_engineOptionWidgetsList.size(); i++) {
        // オプションタイプが"button"の場合
        if (m_optionList.at(i).type == OptionTypeButton) {
            m_engineOptionWidgetsList.at(i).selectionButton->setProperty("index", i);

            // ボタンが押された場合、ボタンの色を変える。
            connect(m_engineOptionWidgetsList.at(i).selectionButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::changeColorTypeButton);
        }
    }
}

// ボタンが押された場合、ボタンの色を変える。
void ChangeEngineSettingsDialog::changeColorTypeButton()
{
    // イベントを送信したオブジェクト（ボタン）へのポインタを取得し、QPushButton型にキャストする。
    QPushButton* pButton = qobject_cast<QPushButton *>(sender());
    if (!pButton) {
        qWarning() << "changeColorTypeButton: sender is not a QPushButton";
        return;
    }

    // ボタンのプロパティから、どのボタンがクリックされたのかを識別するためのインデックスを取得する。
    int index = qvariant_cast<int>(pButton->property("index"));

    // インデックスの範囲チェック
    if (index < 0 || index >= m_engineOptionWidgetsList.size()) {
        qWarning() << "changeColorTypeButton: invalid index" << index;
        return;
    }

    // ボタンが押された状態の場合
    if (m_engineOptionWidgetsList.at(index).selectionButton->isChecked()) {
        m_engineOptionWidgetsList.at(index).selectionButton->setStyleSheet(
            "QPushButton { border: none; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a9eff, stop:1 #2d7dd2); "
            "color: white; font-weight: bold; }");
    }
    // ボタンが押されていない場合
    else {
        m_engineOptionWidgetsList.at(index).selectionButton->setStyleSheet(
            "QPushButton { border: 1px solid #a8c8e8; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fbfe, stop:1 #e8f2fc); "
            "color: #1a5276; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "stop:0 #e8f2fc, stop:1 #d4e9f7); }");
    }
}

// オプションの説明ラベルを作成してレイアウトに追加する。
void ChangeEngineSettingsDialog::createHelpDescriptionLabel(const QString& optionName, QVBoxLayout* layout)
{
    // エンジン名に基づいて説明を取得（対応エンジンのみ説明が返る）
    QString description = EngineOptionDescriptions::getDescription(m_engineName, optionName);

    if (!description.isEmpty()) {
        m_optionWidgets.helpDescriptionLabel = new QLabel(description, this);
        m_optionWidgets.helpDescriptionLabel->setWordWrap(true);
        m_optionWidgets.helpDescriptionLabel->setStyleSheet(
            "QLabel { color: #333333; padding: 2px 0px 6px 0px; "
            "background: transparent; }");
        layout->addWidget(m_optionWidgets.helpDescriptionLabel);
    } else {
        m_optionWidgets.helpDescriptionLabel = nullptr;
    }
}

// エンジンオプションのためのテキストボックスを作成する。
void ChangeEngineSettingsDialog::createTextBox(const EngineOption& option, QVBoxLayout* layout, int optionIndex)
{
    // オプション名でラベルを作成する。
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), this);
    m_optionWidgets.optionNameLabel->setStyleSheet(
        "QLabel { color: #333333; font-weight: bold; "
        "background: transparent; padding: 4px 0px 2px 0px; }");

    // UI要素を指定されたレイアウトに追加する。
    layout->addWidget(m_optionWidgets.optionNameLabel);

    // オプションの説明ラベルを追加する。
    createHelpDescriptionLabel(option.name, layout);

    // テキスト入力用のLineEditを作成する。
    m_optionWidgets.lineEdit = new QLineEdit(this);
    m_optionWidgets.lineEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #a8c8e8; border-radius: 4px; padding: 6px 8px; "
        "background-color: white; color: #333333; }"
        "QLineEdit:focus { border: 2px solid #4a9eff; }");

    // 現在値をセットする。
    m_optionWidgets.lineEdit->setText(option.currentValue);

    layout->addWidget(m_optionWidgets.lineEdit);

    // オプション名に"Dir"、"File"、"Log"が含まれる場合のみファイル/フォルダ選択ボタンを表示する。
    if (option.name.contains("Dir", Qt::CaseInsensitive) ||
        option.name.contains("File", Qt::CaseInsensitive) ||
        option.name.contains("Log", Qt::CaseInsensitive)) {

        // ファイルタイプを判定して対応するボタンを作成する。
        m_optionWidgets.selectionButton = new QPushButton(this);
        m_optionWidgets.selectionButton->setStyleSheet(
            "QPushButton { border: 1px solid #7fb3d5; border-radius: 4px; padding: 6px 12px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e8f4fc, stop:1 #d4e9f7); "
            "color: #1a5276; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "stop:0 #d4e9f7, stop:1 #c0ddf0); }"
            "QPushButton:pressed { background: #b8d4e8; }");

        // デフォルトをファイルタイプに設定する。
        EngineSettings::FileType fileType = EngineSettings::FileType::File;

        // オプション名に"Dir"が含まれるかチェックして、対応するボタンのテキストを設定する。
        if (option.name.contains("Dir", Qt::CaseInsensitive)) {
            m_optionWidgets.selectionButton->setText(tr("フォルダ・ディレクトリの選択"));

            // ディレクトリタイプに設定
            fileType = EngineSettings::FileType::Directory;
        } else {
            m_optionWidgets.selectionButton->setText(tr("ファイルの選択"));

            // ファイルタイプに設定する。
            fileType = EngineSettings::FileType::File;
        }

        // ボタンのプロパティにオプションインデックスとファイルタイプを設定する。
        m_optionWidgets.selectionButton->setProperty("index", optionIndex);
        m_optionWidgets.selectionButton->setProperty("fileType", static_cast<int>(fileType));

        // ボタンのクリックイベントに対するスロットを接続
        connect(m_optionWidgets.selectionButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::openFile);

        layout->addWidget(m_optionWidgets.selectionButton);
    } else {
        m_optionWidgets.selectionButton = nullptr;
    }

    // テキストボックスのオブジェクト名を設定
    m_optionWidgets.lineEdit->setObjectName(option.name);
}

// エンジンオプションのためのスピンボックスを作成する。
void ChangeEngineSettingsDialog::createSpinBox(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1").arg(option.name), this);
    m_optionWidgets.optionNameLabel->setStyleSheet(
        "QLabel { color: #333333; font-weight: bold; "
        "background: transparent; padding: 4px 0px 2px 0px; }");

    // オプション名のラベルをレイアウトに追加する。
    layout->addWidget(m_optionWidgets.optionNameLabel);

    // オプションの説明ラベルを追加する。
    createHelpDescriptionLabel(option.name, layout);

    // オプション名が"USI_Hash"の場合、入力値制限についてのラベルを作成する。
    if (option.name == "USI_Hash") {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 以上の値を入力してください。（既定値: %2）")
                                    .arg(option.min, option.defaultValue), this);
    } else {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 から %2 までの値を入力してください。（既定値: %3）")
                                    .arg(option.min, option.max, option.defaultValue), this);
    }
    m_optionWidgets.optionDescriptionLabel->setStyleSheet(
        "QLabel { color: #5d6d7e; background: transparent; }");

    // スピンボックスを作成する。
    m_optionWidgets.integerSpinBox = new LongLongSpinBox(this);

    // スピンボックスの横幅を制限する。
    m_optionWidgets.integerSpinBox->setMaximumWidth(200);

    // スタイルシートを設定
    m_optionWidgets.integerSpinBox->setStyleSheet(
        "QSpinBox { border: 1px solid #a8c8e8; border-radius: 4px; padding: 6px 8px; "
        "background-color: white; color: #333333; }"
        "QSpinBox:focus { border: 2px solid #4a9eff; }");

    // スピンボックスの範囲を設定する。オプションのminとmaxを使用する。
    m_optionWidgets.integerSpinBox->setSpinBoxRange(option.min.toLongLong(), option.max.toLongLong());

    // スピンボックスの現在値を設定する。オプションの現在値を使用する。
    m_optionWidgets.integerSpinBox->setSpinBoxValue(option.currentValue.toLongLong());

    // スピンボックスのフォーカスポリシーを設定する。これはマウスクリックによってのみフォーカスされるようにするためである。
    m_optionWidgets.integerSpinBox->setFocusPolicy(Qt::ClickFocus);

    // このダイアログ自体をスピンボックスのイベントフィルタとしてインストールする。これにより、特定のイベントをこのダイアログで処理できるようになる。
    m_optionWidgets.integerSpinBox->installEventFilter(this);

    // 入力値制限のラベルとスピンボックスをレイアウトに追加する。
    layout->addWidget(m_optionWidgets.optionDescriptionLabel);
    layout->addWidget(m_optionWidgets.integerSpinBox);

    // スピンボックスのオブジェクト名を設定
    m_optionWidgets.integerSpinBox->setObjectName(option.name);
}

// エンジンオプションのためのチェックボックスを作成する。
void ChangeEngineSettingsDialog::createCheckBox(const EngineOption& option, QVBoxLayout* layout)
{
    // チェックボックスを作成
    m_optionWidgets.optionCheckBox = new QCheckBox(this);
    m_optionWidgets.optionCheckBox->setStyleSheet(
        "QCheckBox { color: #333333; font-weight: bold; "
        "background: transparent; padding: 4px 0px 2px 0px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { border: 2px solid #a8c8e8; border-radius: 4px; "
        "background-color: white; }"
        "QCheckBox::indicator:checked { border: 2px solid #4a9eff; border-radius: 4px; "
        "background-color: #4a9eff; }"
        "QCheckBox::indicator:hover { border: 2px solid #4a9eff; }");

    // チェックボックスにオプション名と既定値のテキストをセット
    m_optionWidgets.optionCheckBox->setText(tr("%1（既定値 %2）").arg(option.name, option.defaultValue));

    // 設定ファイルのオプションの現在値に基づいてチェック状態を設定
    m_optionWidgets.optionCheckBox->setChecked(option.currentValue == "true");

    // チェックボックスをレイアウトに追加
    layout->addWidget(m_optionWidgets.optionCheckBox);

    // オプションの説明ラベルを追加する。
    createHelpDescriptionLabel(option.name, layout);

    // ここで、チェックボックスのオブジェクト名をオプション名に基づいて設定しておくことで、
    // 後で特定のオプションに対応するチェックボックスを識別できるようにしておく。
    m_optionWidgets.optionCheckBox->setObjectName(option.name);
}

// エンジンオプションのためのボタンを作成する。
void ChangeEngineSettingsDialog::createButton(const EngineOption& option, QVBoxLayout* layout)
{
    // ボタンの作成
    m_optionWidgets.selectionButton = new QPushButton(option.name, this);

    // ボタンのチェック可能状態を設定（オン/オフスイッチのように使用）
    m_optionWidgets.selectionButton->setCheckable(true);

    // ボタンの現在値に基づいてチェック状態を設定
    m_optionWidgets.selectionButton->setChecked(option.currentValue == "on");

    // ボタンのスタイルをチェック状態に応じて設定
    if (m_optionWidgets.selectionButton->isChecked()) {
        m_optionWidgets.selectionButton->setStyleSheet(
            "QPushButton { border: none; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a9eff, stop:1 #2d7dd2); "
            "color: white; font-weight: bold; }");
    } else {
        m_optionWidgets.selectionButton->setStyleSheet(
            "QPushButton { border: 1px solid #a8c8e8; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fbfe, stop:1 #e8f2fc); "
            "color: #1a5276; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "stop:0 #e8f2fc, stop:1 #d4e9f7); }");
    }

    // ボタンをレイアウトに追加
    layout->addWidget(m_optionWidgets.selectionButton);

    // オプションの説明ラベルを追加する。
    createHelpDescriptionLabel(option.name, layout);

    // ボタンのオブジェクト名を設定
    m_optionWidgets.selectionButton->setObjectName(option.name);
}

// エンジンオプションのためのコンボボックスを作成する。
void ChangeEngineSettingsDialog::createComboBox(const EngineOption& option, QVBoxLayout* layout)
{
    // オプション名のラベルを作成
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), this);
    m_optionWidgets.optionNameLabel->setStyleSheet(
        "QLabel { color: #333333; font-weight: bold; "
        "background: transparent; padding: 4px 0px 2px 0px; }");

    // ラベルをレイアウトに追加
    layout->addWidget(m_optionWidgets.optionNameLabel);

    // オプションの説明ラベルを追加する。
    createHelpDescriptionLabel(option.name, layout);

    // コンボボックスを作成
    m_optionWidgets.comboBox = new QComboBox(this);
    m_optionWidgets.comboBox->setFocusPolicy(Qt::StrongFocus);
    m_optionWidgets.comboBox->installEventFilter(this);

    // コンボボックスの横幅を制限する。
    m_optionWidgets.comboBox->setMaximumWidth(200);

    m_optionWidgets.comboBox->setStyleSheet(
        "QComboBox { border: 1px solid #a8c8e8; border-radius: 4px; padding: 6px 8px; "
        "background-color: white; color: #333333; }"
        "QComboBox:focus { border: 2px solid #4a9eff; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; "
        "border-right: 5px solid transparent; border-top: 6px solid #4a9eff; }"
        "QComboBox QAbstractItemView { border: 1px solid #a8c8e8; background-color: white; "
        "selection-background-color: #d4e9f7; selection-color: #333333; }");

    // コンボボックスに選択肢を追加
    for (const QString& value : option.valueList) {
        m_optionWidgets.comboBox->addItem(value);
    }

    // コンボボックスの現在選択をオプションの現在値に設定
    int currentIndex = m_optionWidgets.comboBox->findText(option.currentValue);
    if (currentIndex != -1) { // 該当する項目が見つかった場合
        m_optionWidgets.comboBox->setCurrentIndex(currentIndex);
    }

    // コンボボックスをレイアウトに追加
    layout->addWidget(m_optionWidgets.comboBox);

    // コンボボックスのオブジェクト名を設定
    m_optionWidgets.comboBox->setObjectName(option.name);
}

// オプションのタイプに応じたUIコンポーネントを作成し、レイアウトに追加する。
void ChangeEngineSettingsDialog::createWidgetForOption(const EngineOption& option, int optionIndex, QVBoxLayout* targetLayout)
{
    // オプションタイプが"filename"か"string"の場合
    if (option.type == OptionTypeFilename || option.type == OptionTypeString) {
        // エンジンオプションのためのテキストボックスを作成する。
        createTextBox(option, targetLayout, optionIndex);
    }
    // オプションタイプが"spin"の場合
    else if (option.type == OptionTypeSpin) {
        // エンジンオプションのためのスピンボックスを作成する。
        createSpinBox(option, targetLayout);
    }
    // オプションタイプが"check"の場合
    else if (option.type == OptionTypeCheck) {
        // エンジンオプションのためのチェックボックスを作成する。
        createCheckBox(option, targetLayout);
    }
    // オプションタイプが"button"の場合
    else if (option.type == OptionTypeButton) {
        // エンジンオプションのためのボタンを作成する。
        createButton(option, targetLayout);

    }
    // オプションタイプが"combo"の場合
    else if (option.type == OptionTypeCombo) {
        // エンジンオプションのためのコンボボックスを作成する。
        createComboBox(option, targetLayout);
    }
}

// カテゴリ別の折りたたみ可能なグループボックスを作成する。
CollapsibleGroupBox* ChangeEngineSettingsDialog::createCategoryGroupBox(EngineOptionCategory category)
{
    QString categoryName = EngineOptionDescriptions::getCategoryDisplayName(category);
    CollapsibleGroupBox* groupBox = new CollapsibleGroupBox(categoryName, this);
    return groupBox;
}

// エンジンオプションに基づいてUIコンポーネントを作成して配置する。
void ChangeEngineSettingsDialog::createOptionWidgets()
{
    // 設定画面の上部にエンジン名と作者名を表示する。
    QString headerText = m_engineName;
    if (!m_engineAuthor.isEmpty()) {
        headerText += tr("\n作者: %1").arg(m_engineAuthor);
    }
    ui->label->setText(headerText);
    ui->label->setStyleSheet(
        "QLabel { background-color: transparent; padding: 8px; "
        "font-weight: bold; color: #333333; }");

    // ダイアログ全体の背景をクリーム色にする
    this->setStyleSheet(
        "QDialog { background-color: #fefcf6; }");

    // 画面レイアウトを作成する。
    m_optionWidgetsLayout = new QVBoxLayout;

    // カテゴリ別にオプションをグループ化するためのマップを作成
    // キー: カテゴリ、値: (オプションインデックス, オプション) のリスト
    QMap<EngineOptionCategory, QList<QPair<int, EngineOption>>> categoryOptionsMap;

    // オプションをカテゴリ別に分類（エンジン名に基づいてカテゴリを取得）
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        const auto& option = m_optionList.at(i);
        EngineOptionCategory category = EngineOptionDescriptions::getCategory(m_engineName, option.name);
        categoryOptionsMap[category].append(qMakePair(static_cast<int>(i), option));
    }

    // m_engineOptionWidgetsList のサイズを事前に確保（インデックスでアクセスするため）
    m_engineOptionWidgetsList.resize(m_optionList.size());

    // 定義された順序でカテゴリを処理
    QList<EngineOptionCategory> categoryOrder = EngineOptionDescriptions::getAllCategories();

    for (EngineOptionCategory category : categoryOrder) {
        // このカテゴリにオプションがない場合はスキップ
        if (!categoryOptionsMap.contains(category) || categoryOptionsMap[category].isEmpty()) {
            continue;
        }

        // カテゴリ用の折りたたみ可能なグループボックスを作成
        CollapsibleGroupBox* groupBox = createCategoryGroupBox(category);
        QVBoxLayout* groupLayout = groupBox->contentLayout();

        // このカテゴリのオプションを処理
        const auto& optionsInCategory = categoryOptionsMap[category];
        for (const auto& pair : optionsInCategory) {
            int optionIndex = pair.first;
            const EngineOption& option = pair.second;

            // オプションのタイプに応じたUIコンポーネントを作成し、グループレイアウトに追加
            createWidgetForOption(option, optionIndex, groupLayout);

            // 各オプションの状態をメニューリストの対応する位置に格納
            m_engineOptionWidgetsList[optionIndex] = m_optionWidgets;
        }

        // グループボックスをメインレイアウトに追加
        m_optionWidgetsLayout->addWidget(groupBox);
    }

    // 各ボタンが押されているかどうかを確認し、ボタンの色を設定する。
    changeStatusColorTypeButton();

    // レイアウトを上部に配置するためにスペーサーを追加（setAlignmentは水平方向の拡張を制限するため使用しない）
    m_optionWidgetsLayout->addStretch();

    // ダイアログのスクロールアリアのウィジェットにレイアウトをセットする。
    ui->scrollAreaWidgetContents->setLayout(m_optionWidgetsLayout);
    ui->scrollArea->setWidget(ui->scrollAreaWidgetContents);

    // ダイアログの最後に適用ボタンを表示する。
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("適用"));

    // "既定値に戻す"ボタンが押された場合、全てのオプションを既定値に戻す。
    connect(ui->restoreButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::restoreDefaultOptions);

    // "適用"ボタンが押された場合、全てのオプションの設定を保存してエンジン設定ダイアログを終了する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ChangeEngineSettingsDialog::writeEngineOptions);

    // "Cancel"ボタンが押された場合、エンジン設定ダイアログを保存せずに終了する。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // フォントサイズ変更ボタンの接続
    connect(ui->fontIncreaseButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::increaseFontSize);
    connect(ui->fontDecreaseButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::decreaseFontSize);

    // 保存されているフォントサイズを適用
    applyFontSize();

    // ダイアログの最小サイズを設定（リサイズ可能にする）
    this->setMinimumSize(400, 300);

    // 保存されているウィンドウサイズを復元
    QSize savedSize = SettingsService::engineSettingsDialogSize();
    if (savedSize.isValid()) {
        this->resize(savedSize);
    }
}

// ファイルまたはディレクトリの選択ダイアログを開き、選択されたパスを返す。
QString ChangeEngineSettingsDialog::openSelectionDialog(QWidget* parent, int fileType, const QString& initialDir)
{
    QString selectedPath;

    // fileTypeに応じて、適切なダイアログを開く
    if (fileType == 1) { // ディレクトリ選択
        selectedPath = QFileDialog::getExistingDirectory(parent, QObject::tr("フォルダ・ディレクトリの選択"), initialDir);
    } else { // ファイル選択
        selectedPath = QFileDialog::getOpenFileName(parent, QObject::tr("ファイルの選択"), initialDir);
    }

    // 選択されたパスが空でない場合にのみ、システムに適したパス形式に変換する。
    if (!selectedPath.isEmpty()) {
        selectedPath = QDir::toNativeSeparators(selectedPath);
    }

    return selectedPath;
}

// 設定画面で"フォルダ・ディレクトリの選択"ボタンあるいは"ファイルの選択"ボタンをクリックした場合にディレクトリ、ファイルを選択する画面を表示する。
void ChangeEngineSettingsDialog::openFile()
{
    // イベントを送信したオブジェクト（ボタン）へのポインタを取得し、QPushButton型にキャストする。
    QPushButton* pButton = qobject_cast<QPushButton*>(sender());

    // ボタンのプロパティから、どのボタンがクリックされたのかを識別するためのインデックスを取得する。
    int index = qvariant_cast<int>(pButton->property("index"));

    // ボタンのプロパティから、ファイル選択かディレクトリ選択かを識別するためのタイプを取得する。
    int fileType = qvariant_cast<int>(pButton->property("fileType"));

    // 汎用の選択ダイアログを使って、ファイルまたはディレクトリのパスを取得する。
    QString selectedPath = openSelectionDialog(this, fileType);

    // 選択されたパスが空ではない場合、対応する設定画面のフィールドにパスを設定する。
    if (!selectedPath.isEmpty()) {
        m_engineOptionWidgetsList.at(index).lineEdit->setText(selectedPath);
    }
}

// エンジンオプションを既定値に戻す。
void ChangeEngineSettingsDialog::restoreDefaultOptions() {
    // エンジンオプションの数だけ繰り返し、各オプションウィジェットに既定値を設定する
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        const auto& option = m_optionList[i]; // 現在のオプションを参照
        auto& widget = m_engineOptionWidgetsList[i]; // 対応するウィジェットを参照

        // オプションタイプに応じた既定値の設定
        if (option.type == OptionTypeFilename || option.type == OptionTypeString) {
            // ファイル名または文字列オプションの場合、LineEditに既定値を設定
            widget.lineEdit->setText(option.defaultValue);
        } else if (option.type == OptionTypeSpin) {
            // 数値オプションの場合、SpinBoxに既定値を設定（文字列を数値に変換）
            widget.integerSpinBox->setSpinBoxValue(option.defaultValue.toLongLong());
        } else if (option.type == OptionTypeCheck) {
            // チェックボックスオプションの場合、既定値が"true"ならチェック
            widget.optionCheckBox->setChecked(option.defaultValue == "true");
        } else if (option.type == OptionTypeButton) {
            // ボタンオプションの場合、既定値が"on"ならボタンをチェック状態に
            bool isChecked = option.defaultValue == "on";
            widget.selectionButton->setChecked(isChecked);
            // チェック状態に応じてボタンのスタイルを変更
            if (isChecked) {
                widget.selectionButton->setStyleSheet(
                    "QPushButton { border: none; border-radius: 4px; padding: 8px 16px; "
                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a9eff, stop:1 #2d7dd2); "
                    "color: white; font-weight: bold; }");
            } else {
                widget.selectionButton->setStyleSheet(
                    "QPushButton { border: 1px solid #a8c8e8; border-radius: 4px; padding: 8px 16px; "
                    "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fbfe, stop:1 #e8f2fc); "
                    "color: #1a5276; }"
                    "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                    "stop:0 #e8f2fc, stop:1 #d4e9f7); }");
            }
        } else if (option.type == OptionTypeCombo) {
            // コンボボックスオプションの場合、既定値に基づいて選択肢を設定
            widget.comboBox->clear(); // 既存の選択肢をクリア
            for (const auto& value : option.valueList) {
                // 選択可能な値を追加
                widget.comboBox->addItem(value);
            }
            // 既定値に対応する選択肢を選択状態に
            int index = widget.comboBox->findText(option.defaultValue);
            if (index != -1) widget.comboBox->setCurrentIndex(index);
        }
    }
}

// 設定ファイルにエンジンのオプション設定を保存する。
void ChangeEngineSettingsDialog::saveOptionsToSettings()
{
    // アプリケーションのディレクトリに移動してから設定ファイルを操作する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを操作するためのQSettingsオブジェクトを作成する。ファイル名とフォーマットを指定。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 設定ファイル内でエンジン名に基づくオプションの配列の書き込みを開始する。
    settings.beginWriteArray(m_engineName);

    // 配列内の位置を指示するためのインデックス変数。
    int index = 0;

    // オプションリストの各オプションに対してループを実行する。
    for (const auto& option : std::as_const(m_optionList)) {
        // 現在のインデックスで配列の要素を設定する。
        settings.setArrayIndex(index);

        // オプションの現在の値を設定ファイルに保存。
        settings.setValue("value", option.currentValue);

        // 次のオプションのためにインデックスをインクリメント。
        ++index;
    }

    // 配列の書き込みを終了する。
    settings.endArray();
}

// 設定ファイルに追加エンジンのオプションを書き込む。
void ChangeEngineSettingsDialog::writeEngineOptions()
{
    // 並列配列のサイズ整合性を確認
    if (m_optionList.size() != m_engineOptionWidgetsList.size()) {
        qWarning() << "writeEngineOptions: リストサイズ不一致 - "
                   << "m_optionList:" << m_optionList.size()
                   << "m_engineOptionWidgetsList:" << m_engineOptionWidgetsList.size();
        return;
    }

    // エンジンオプションの数だけ繰り返す。
    for (qsizetype i = 0; i < m_optionList.size(); i++) {
        EngineOption option = m_optionList.at(i);

        if (option.type == OptionTypeFilename || option.type == OptionTypeString) {
            option.currentValue = m_engineOptionWidgetsList.at(i).lineEdit->text();
        } else if (option.type == OptionTypeSpin) {
            option.currentValue = QString::number(m_engineOptionWidgetsList.at(i).integerSpinBox->value());
        } else if (option.type == OptionTypeCheck) {
            option.currentValue = m_engineOptionWidgetsList.at(i).optionCheckBox->isChecked() ? "true" : "false";
        } else if (option.type == OptionTypeButton) {
            option.currentValue = m_engineOptionWidgetsList.at(i).selectionButton->isChecked() ? "on" : "";
        } else if (option.type == OptionTypeCombo) {
            option.currentValue = m_engineOptionWidgetsList.at(i).comboBox->currentText();
        }

        m_optionList.replace(i, option);
    }

    // 設定ファイルにエンジンのオプション設定を保存する。
    saveOptionsToSettings();
}

// イベントフィルター関数。QObject型のobjとQEvent型のeを引数に取る。
bool ChangeEngineSettingsDialog::eventFilter(QObject* obj, QEvent* e)
{
    // イベントのタイプがマウスホイールの動作（スクロール）であるかどうかを確認する。
    if (e->type() == QEvent::Wheel) {
        // イベントが発生したオブジェクトがQComboBox型かどうかを確認し、QComboBox型へキャストする。
        QComboBox* combo = qobject_cast<QComboBox *>(obj);

        // キャストが成功し、かつQComboBoxがフォーカスを持っていない場合、trueを返してイベントの伝播を止める。
        // これにより、フォーカスがない状態でのマウスホイール操作による不意の値変更を防ぐ。
        if (combo && !combo->hasFocus()) return true;

        // 同様に、イベントが発生したオブジェクトがLongLongSpinBox型かどうかを確認し、LongLongSpinBox型へキャストする。
        LongLongSpinBox* spin = qobject_cast<LongLongSpinBox*>(obj);

        // キャストが成功し、かつLongLongSpinBoxがフォーカスを持っていない場合、trueを返してイベントの伝播を止めます。
        if (spin && !spin->hasFocus()) return true;
    }
    // 上記の条件に当てはまらない場合は、falseを返してイベントの伝播を続行させる。
    return false;
}

// フォントサイズを増加する。
void ChangeEngineSettingsDialog::increaseFontSize()
{
    if (m_fontSize < 20) {  // 最大サイズを20に制限
        m_fontSize++;
        applyFontSize();
        SettingsService::setEngineSettingsFontSize(m_fontSize);
    }
}

// フォントサイズを減少する。
void ChangeEngineSettingsDialog::decreaseFontSize()
{
    if (m_fontSize > 8) {  // 最小サイズを8に制限
        m_fontSize--;
        applyFontSize();
        SettingsService::setEngineSettingsFontSize(m_fontSize);
    }
}

// すべてのウィジェットにフォントサイズを適用する。
void ChangeEngineSettingsDialog::applyFontSize()
{
    // ダイアログ全体のフォントを設定
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    this->setFont(font);

    // スクロールエリアのコンテンツにも明示的にフォントを設定
    ui->scrollAreaWidgetContents->setFont(font);

    // エンジン名・作者ラベルにもフォントを設定
    ui->label->setFont(font);

    // 全ての子ウィジェットにフォントを再帰的に適用
    const std::function<void(QWidget*)> applyToChildren = [&font, &applyToChildren](QWidget* widget) {
        if (!widget) return;
        widget->setFont(font);
        const QObjectList& children = widget->children();
        for (QObject* child : children) {
            QWidget* childWidget = qobject_cast<QWidget*>(child);
            if (childWidget) {
                applyToChildren(childWidget);
            }
        }
    };

    applyToChildren(ui->scrollAreaWidgetContents);
}
