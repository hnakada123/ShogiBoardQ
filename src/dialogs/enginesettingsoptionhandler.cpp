/// @file enginesettingsoptionhandler.cpp
/// @brief エンジン設定オプションのウィジェット生成・データ管理ハンドラの実装

#include "enginesettingsoptionhandler.h"
#include "enginesettingsconstants.h"
#include "engineoptiondescriptions.h"
#include "settingscommon.h"
#include <QApplication>
#include <QSettings>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>

using namespace EngineSettingsConstants;

namespace {

// ボタンのチェック状態に応じたスタイルを適用する
void applyButtonToggleStyle(QPushButton* button, bool checked)
{
    if (checked) {
        button->setStyleSheet(
            "QPushButton { border: none; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a9eff, stop:1 #2d7dd2); "
            "color: white; font-weight: bold; }");
    } else {
        button->setStyleSheet(
            "QPushButton { border: 1px solid #a8c8e8; border-radius: 4px; padding: 8px 16px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fbfe, stop:1 #e8f2fc); "
            "color: #1a5276; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "stop:0 #e8f2fc, stop:1 #d4e9f7); }");
    }
}

const char* kLabelBoldStyle =
    "QLabel { color: #333333; font-weight: bold; "
    "background: transparent; padding: 4px 0px 2px 0px; }";

} // namespace

EngineSettingsOptionHandler::EngineSettingsOptionHandler(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

// 設定ファイルから選択したエンジンのオプションを読み込む。
void EngineSettingsOptionHandler::readEngineOptions()
{
    m_optionList.clear();

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    int size = settings.beginReadArray(m_engineName);

    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);

        EngineOption option;
        option.name = settings.value(EngineOptionNameKey).toString();
        option.type = settings.value(EngineOptionTypeKey).toString();
        option.defaultValue = settings.value(EngineOptionDefaultKey).toString();
        option.min = settings.value(EngineOptionMinKey).toString();
        option.max = settings.value(EngineOptionMaxKey).toString();
        option.currentValue = settings.value(EngineOptionValueKey).toString();
        QString valueListStr = settings.value(EngineOptionValueListKey).toString();
        option.valueList = valueListStr.split(" ", Qt::SkipEmptyParts);

        m_optionList.append(option);
    }

    settings.endArray();
}

// オプションウィジェットを生成してレイアウトに追加する。
void EngineSettingsOptionHandler::buildOptionWidgets(QVBoxLayout* layout)
{
    // カテゴリ別にオプションをグループ化するためのマップを作成
    QMap<EngineOptionCategory, QList<QPair<int, EngineOption>>> categoryOptionsMap;

    // オプションをカテゴリ別に分類
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        const auto& option = m_optionList.at(i);
        EngineOptionCategory category = EngineOptionDescriptions::getCategory(m_engineName, option.name);
        categoryOptionsMap[category].append(qMakePair(static_cast<int>(i), option));
    }

    // m_engineOptionWidgetsList のサイズを事前に確保
    m_engineOptionWidgetsList.resize(m_optionList.size());

    // 定義された順序でカテゴリを処理
    QList<EngineOptionCategory> categoryOrder = EngineOptionDescriptions::getAllCategories();

    for (EngineOptionCategory category : categoryOrder) {
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

            createWidgetForOption(option, optionIndex, groupLayout);
            m_engineOptionWidgetsList[optionIndex] = m_optionWidgets;
        }

        layout->addWidget(groupBox);
    }

    // 各ボタンが押されているかどうかを確認し、ボタンの色を設定する。
    changeStatusColorTypeButton();
}

// 各ボタンが押されているかどうかを確認し、ボタンの色を設定する。
void EngineSettingsOptionHandler::changeStatusColorTypeButton()
{
    if (m_optionList.size() != m_engineOptionWidgetsList.size()) {
        qWarning() << "changeStatusColorTypeButton: リストサイズ不一致";
        return;
    }

    for (qsizetype i = 0; i < m_engineOptionWidgetsList.size(); i++) {
        if (m_optionList.at(i).type == OptionTypeButton) {
            m_engineOptionWidgetsList.at(i).selectionButton->setProperty("index", i);
            connect(m_engineOptionWidgetsList.at(i).selectionButton, &QPushButton::clicked, this, &EngineSettingsOptionHandler::changeColorTypeButton);
        }
    }
}

// ボタンが押された場合、ボタンの色を変える。
void EngineSettingsOptionHandler::changeColorTypeButton()
{
    QPushButton* pButton = qobject_cast<QPushButton *>(sender());
    if (!pButton) {
        qWarning() << "changeColorTypeButton: sender is not a QPushButton";
        return;
    }

    int index = qvariant_cast<int>(pButton->property("index"));

    if (index < 0 || index >= m_engineOptionWidgetsList.size()) {
        qWarning() << "changeColorTypeButton: invalid index" << index;
        return;
    }

    applyButtonToggleStyle(m_engineOptionWidgetsList.at(index).selectionButton,
                           m_engineOptionWidgetsList.at(index).selectionButton->isChecked());
}

// オプションの説明ラベルを作成してレイアウトに追加する。
void EngineSettingsOptionHandler::createHelpDescriptionLabel(const QString& optionName, QVBoxLayout* layout)
{
    QString description = EngineOptionDescriptions::getDescription(m_engineName, optionName);

    if (!description.isEmpty()) {
        m_optionWidgets.helpDescriptionLabel = new QLabel(description, m_parentWidget);
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
void EngineSettingsOptionHandler::createTextBox(const EngineOption& option, QVBoxLayout* layout, int optionIndex)
{
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), m_parentWidget);
    m_optionWidgets.optionNameLabel->setStyleSheet(kLabelBoldStyle);
    layout->addWidget(m_optionWidgets.optionNameLabel);

    createHelpDescriptionLabel(option.name, layout);

    // テキスト入力用のLineEditを作成する。
    m_optionWidgets.lineEdit = new QLineEdit(m_parentWidget);
    m_optionWidgets.lineEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #a8c8e8; border-radius: 4px; padding: 6px 8px; "
        "background-color: white; color: #333333; }"
        "QLineEdit:focus { border: 2px solid #4a9eff; }");
    m_optionWidgets.lineEdit->setText(option.currentValue);
    layout->addWidget(m_optionWidgets.lineEdit);

    // オプション名に"Dir"、"File"、"Log"が含まれる場合のみファイル/フォルダ選択ボタンを表示する。
    if (option.name.contains("Dir", Qt::CaseInsensitive) ||
        option.name.contains("File", Qt::CaseInsensitive) ||
        option.name.contains("Log", Qt::CaseInsensitive)) {

        m_optionWidgets.selectionButton = new QPushButton(m_parentWidget);
        m_optionWidgets.selectionButton->setStyleSheet(
            "QPushButton { border: 1px solid #7fb3d5; border-radius: 4px; padding: 6px 12px; "
            "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e8f4fc, stop:1 #d4e9f7); "
            "color: #1a5276; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
            "stop:0 #d4e9f7, stop:1 #c0ddf0); }"
            "QPushButton:pressed { background: #b8d4e8; }");

        EngineSettings::FileType fileType = EngineSettings::FileType::File;
        if (option.name.contains("Dir", Qt::CaseInsensitive)) {
            m_optionWidgets.selectionButton->setText(tr("フォルダ・ディレクトリの選択"));
            fileType = EngineSettings::FileType::Directory;
        } else {
            m_optionWidgets.selectionButton->setText(tr("ファイルの選択"));
        }

        m_optionWidgets.selectionButton->setProperty("index", optionIndex);
        m_optionWidgets.selectionButton->setProperty("fileType", static_cast<int>(fileType));
        connect(m_optionWidgets.selectionButton, &QPushButton::clicked, this, &EngineSettingsOptionHandler::openFile);
        layout->addWidget(m_optionWidgets.selectionButton);
    } else {
        m_optionWidgets.selectionButton = nullptr;
    }

    m_optionWidgets.lineEdit->setObjectName(option.name);
}

// エンジンオプションのためのスピンボックスを作成する。
void EngineSettingsOptionHandler::createSpinBox(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1").arg(option.name), m_parentWidget);
    m_optionWidgets.optionNameLabel->setStyleSheet(kLabelBoldStyle);
    layout->addWidget(m_optionWidgets.optionNameLabel);

    createHelpDescriptionLabel(option.name, layout);

    // 入力値制限についてのラベルを作成する。
    if (option.name == "USI_Hash") {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 以上の値を入力してください。（既定値: %2）")
                                    .arg(option.min, option.defaultValue), m_parentWidget);
    } else {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 から %2 までの値を入力してください。（既定値: %3）")
                                    .arg(option.min, option.max, option.defaultValue), m_parentWidget);
    }
    m_optionWidgets.optionDescriptionLabel->setStyleSheet(
        "QLabel { color: #5d6d7e; background: transparent; }");

    m_optionWidgets.integerSpinBox = new LongLongSpinBox(m_parentWidget);
    m_optionWidgets.integerSpinBox->setMaximumWidth(200);
    m_optionWidgets.integerSpinBox->setStyleSheet(
        "QSpinBox { border: 1px solid #a8c8e8; border-radius: 4px; padding: 6px 8px; "
        "background-color: white; color: #333333; }"
        "QSpinBox:focus { border: 2px solid #4a9eff; }");
    m_optionWidgets.integerSpinBox->setSpinBoxRange(option.min.toLongLong(), option.max.toLongLong());
    m_optionWidgets.integerSpinBox->setSpinBoxValue(option.currentValue.toLongLong());
    m_optionWidgets.integerSpinBox->setFocusPolicy(Qt::ClickFocus);
    m_optionWidgets.integerSpinBox->installEventFilter(this);

    layout->addWidget(m_optionWidgets.optionDescriptionLabel);
    layout->addWidget(m_optionWidgets.integerSpinBox);

    m_optionWidgets.integerSpinBox->setObjectName(option.name);
}

// エンジンオプションのためのチェックボックスを作成する。
void EngineSettingsOptionHandler::createCheckBox(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.optionCheckBox = new QCheckBox(m_parentWidget);
    m_optionWidgets.optionCheckBox->setStyleSheet(
        "QCheckBox { color: #333333; font-weight: bold; "
        "background: transparent; padding: 4px 0px 2px 0px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { border: 2px solid #a8c8e8; border-radius: 4px; "
        "background-color: white; }"
        "QCheckBox::indicator:checked { border: 2px solid #4a9eff; border-radius: 4px; "
        "background-color: #4a9eff; }"
        "QCheckBox::indicator:hover { border: 2px solid #4a9eff; }");

    m_optionWidgets.optionCheckBox->setText(tr("%1（既定値 %2）").arg(option.name, option.defaultValue));
    m_optionWidgets.optionCheckBox->setChecked(option.currentValue == "true");
    layout->addWidget(m_optionWidgets.optionCheckBox);

    createHelpDescriptionLabel(option.name, layout);

    m_optionWidgets.optionCheckBox->setObjectName(option.name);
}

// エンジンオプションのためのボタンを作成する。
void EngineSettingsOptionHandler::createButton(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.selectionButton = new QPushButton(option.name, m_parentWidget);
    m_optionWidgets.selectionButton->setCheckable(true);
    m_optionWidgets.selectionButton->setChecked(option.currentValue == "on");

    applyButtonToggleStyle(m_optionWidgets.selectionButton, m_optionWidgets.selectionButton->isChecked());

    layout->addWidget(m_optionWidgets.selectionButton);

    createHelpDescriptionLabel(option.name, layout);

    m_optionWidgets.selectionButton->setObjectName(option.name);
}

// エンジンオプションのためのコンボボックスを作成する。
void EngineSettingsOptionHandler::createComboBox(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), m_parentWidget);
    m_optionWidgets.optionNameLabel->setStyleSheet(kLabelBoldStyle);
    layout->addWidget(m_optionWidgets.optionNameLabel);

    createHelpDescriptionLabel(option.name, layout);

    m_optionWidgets.comboBox = new QComboBox(m_parentWidget);
    m_optionWidgets.comboBox->setFocusPolicy(Qt::StrongFocus);
    m_optionWidgets.comboBox->installEventFilter(this);
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

    for (const QString& value : option.valueList) {
        m_optionWidgets.comboBox->addItem(value);
    }

    int currentIndex = m_optionWidgets.comboBox->findText(option.currentValue);
    if (currentIndex != -1) {
        m_optionWidgets.comboBox->setCurrentIndex(currentIndex);
    }

    layout->addWidget(m_optionWidgets.comboBox);

    m_optionWidgets.comboBox->setObjectName(option.name);
}

// オプションのタイプに応じたUIコンポーネントを作成し、レイアウトに追加する。
void EngineSettingsOptionHandler::createWidgetForOption(const EngineOption& option, int optionIndex, QVBoxLayout* targetLayout)
{
    if (option.type == OptionTypeFilename || option.type == OptionTypeString) {
        createTextBox(option, targetLayout, optionIndex);
    } else if (option.type == OptionTypeSpin) {
        createSpinBox(option, targetLayout);
    } else if (option.type == OptionTypeCheck) {
        createCheckBox(option, targetLayout);
    } else if (option.type == OptionTypeButton) {
        createButton(option, targetLayout);
    } else if (option.type == OptionTypeCombo) {
        createComboBox(option, targetLayout);
    }
}

// カテゴリ別の折りたたみ可能なグループボックスを作成する。
CollapsibleGroupBox* EngineSettingsOptionHandler::createCategoryGroupBox(EngineOptionCategory category)
{
    QString categoryName = EngineOptionDescriptions::getCategoryDisplayName(category);
    return new CollapsibleGroupBox(categoryName, m_parentWidget);
}

// ファイル・ディレクトリ選択ダイアログを表示する。
void EngineSettingsOptionHandler::openFile()
{
    QPushButton* pButton = qobject_cast<QPushButton*>(sender());
    int index = qvariant_cast<int>(pButton->property("index"));
    int fileType = qvariant_cast<int>(pButton->property("fileType"));

    QString selectedPath;
    if (fileType == static_cast<int>(EngineSettings::FileType::Directory)) {
        selectedPath = QFileDialog::getExistingDirectory(m_parentWidget, tr("フォルダ・ディレクトリの選択"));
    } else {
        selectedPath = QFileDialog::getOpenFileName(m_parentWidget, tr("ファイルの選択"));
    }

    if (!selectedPath.isEmpty()) {
        m_engineOptionWidgetsList.at(index).lineEdit->setText(QDir::toNativeSeparators(selectedPath));
    }
}

// エンジンオプションを既定値に戻す。
void EngineSettingsOptionHandler::restoreDefaultOptions()
{
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        const auto& option = m_optionList[i];
        auto& widget = m_engineOptionWidgetsList[i];

        if (option.type == OptionTypeFilename || option.type == OptionTypeString) {
            widget.lineEdit->setText(option.defaultValue);
        } else if (option.type == OptionTypeSpin) {
            widget.integerSpinBox->setSpinBoxValue(option.defaultValue.toLongLong());
        } else if (option.type == OptionTypeCheck) {
            widget.optionCheckBox->setChecked(option.defaultValue == "true");
        } else if (option.type == OptionTypeButton) {
            bool isChecked = option.defaultValue == "on";
            widget.selectionButton->setChecked(isChecked);
            applyButtonToggleStyle(widget.selectionButton, isChecked);
        } else if (option.type == OptionTypeCombo) {
            widget.comboBox->clear();
            for (const auto& value : option.valueList) {
                widget.comboBox->addItem(value);
            }
            int index = widget.comboBox->findText(option.defaultValue);
            if (index != -1) widget.comboBox->setCurrentIndex(index);
        }
    }
}

// 設定ファイルにエンジンのオプション設定を保存する。
void EngineSettingsOptionHandler::saveOptionsToSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginWriteArray(m_engineName);

    int index = 0;
    for (const auto& option : std::as_const(m_optionList)) {
        settings.setArrayIndex(index);
        settings.setValue("value", option.currentValue);
        ++index;
    }

    settings.endArray();
}

// ウィジェットの現在値を読み取り設定ファイルに保存する。
void EngineSettingsOptionHandler::writeEngineOptions()
{
    if (m_optionList.size() != m_engineOptionWidgetsList.size()) {
        qWarning() << "writeEngineOptions: リストサイズ不一致 - "
                   << "m_optionList:" << m_optionList.size()
                   << "m_engineOptionWidgetsList:" << m_engineOptionWidgetsList.size();
        return;
    }

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

    saveOptionsToSettings();
}

// イベントフィルター（スピンボックス・コンボボックスのホイール操作抑止）
bool EngineSettingsOptionHandler::eventFilter(QObject* obj, QEvent* e)
{
    if (e->type() == QEvent::Wheel) {
        QComboBox* combo = qobject_cast<QComboBox *>(obj);
        if (combo && !combo->hasFocus()) return true;

        LongLongSpinBox* spin = qobject_cast<LongLongSpinBox*>(obj);
        if (spin && !spin->hasFocus()) return true;
    }
    return false;
}
