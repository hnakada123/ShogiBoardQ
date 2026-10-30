/// @file enginesettingsoptionhandler.cpp
/// @brief エンジン設定オプションのウィジェット生成・データ管理ハンドラの実装

#include "enginesettingsoptionhandler.h"
#include "engineoptiondescriptions.h"
#include "engineprofilestore.h"
#include "buttonstyles.h"
#include <QApplication>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>
#include <utility>


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
    "QLabel { color: palette(text); font-weight: bold; "
    "background: transparent; padding: 4px 0px 2px 0px; }";

/// 押すと入る・切れるボタン（USI の button 型）。入っているかで見た目を変える
class OptionToggleButton : public QPushButton
{
public:
    OptionToggleButton(const QString& text, QWidget* parent) : QPushButton(text, parent)
    {
        setAutoDefault(false);
        setCheckable(true);
        connect(this, &QPushButton::toggled, this, &OptionToggleButton::restyle);
        restyle(false);
    }

private:
    void restyle(bool checked) { applyButtonToggleStyle(this, checked); }
};

/// ファイル・フォルダを選んで入力欄に入れるボタン
class PathSelectButton : public QPushButton
{
public:
    PathSelectButton(QLineEdit* target, EngineSettings::FileType fileType, QWidget* parent)
        : QPushButton(parent), m_target(target), m_fileType(fileType)
    {
        setAutoDefault(false);
        setStyleSheet(ButtonStyles::fileOperation());
        setText(fileType == EngineSettings::FileType::Directory
                    ? EngineSettingsOptionHandler::tr("フォルダ・ディレクトリの選択")
                    : EngineSettingsOptionHandler::tr("ファイルの選択"));
        connect(this, &QPushButton::clicked, this, &PathSelectButton::selectPath);
    }

private:
    void selectPath()
    {
        const QString selected = m_fileType == EngineSettings::FileType::Directory
            ? QFileDialog::getExistingDirectory(window(), EngineSettingsOptionHandler::tr("フォルダ・ディレクトリの選択"))
            : QFileDialog::getOpenFileName(window(), EngineSettingsOptionHandler::tr("ファイルの選択"));
        if (!selected.isEmpty()) m_target->setText(QDir::toNativeSeparators(selected));
    }

    QLineEdit* m_target;
    EngineSettings::FileType m_fileType;
};

} // namespace

EngineSettingsOptionHandler::EngineSettingsOptionHandler(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

// 選択したエンジンの保存したオプションを読み込む。
void EngineSettingsOptionHandler::readEngineOptions()
{
    m_optionList = EngineProfileStore::loadOptions(m_engineName);
}

// オプションウィジェットを生成してレイアウトに追加する。
void EngineSettingsOptionHandler::buildOptionWidgets(QVBoxLayout* layout)
{
    // カテゴリ別にオプションをグループ化するためのマップを作成
    QMap<EngineOptionCategory, QList<QPair<int, EngineOption>>> categoryOptionsMap;

    // オプションをカテゴリ別に分類
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        const auto& option = m_optionList.at(i);
        if (EngineProfileStore::isPonderOption(option.name)) continue; // GUI共通設定に統合
        EngineOptionCategory category = EngineOptionDescriptions::category(m_engineName, option.name);
        categoryOptionsMap[category].append(qMakePair(static_cast<int>(i), option));
    }

    // 入力欄は m_optionList と同じ並びで持つ
    m_editors.clear();
    m_editors.resize(m_optionList.size());

    // 定義された順序でカテゴリを処理
    QList<EngineOptionCategory> categoryOrder = EngineOptionDescriptions::allCategories();

    for (EngineOptionCategory category : std::as_const(categoryOrder)) {
        if (!categoryOptionsMap.contains(category) || categoryOptionsMap[category].isEmpty()) {
            continue;
        }

        // カテゴリ用の折りたたみ可能なグループボックスを作成
        CollapsibleGroupBox* groupBox = createCategoryGroupBox(category);
        QVBoxLayout* groupLayout = groupBox->contentLayout();

        // このカテゴリのオプションを処理
        const auto& optionsInCategory = categoryOptionsMap[category];
        for (const auto& pair : std::as_const(optionsInCategory)) {
            int optionIndex = pair.first;
            const EngineOption& option = pair.second;

            m_editors[optionIndex] = createEditor(option, groupLayout);
        }

        layout->addWidget(groupBox);
    }
}

// ============================================================
// 入力欄
// ============================================================

QLabel* EngineSettingsOptionHandler::addNameLabel(const QString& text, QVBoxLayout* layout)
{
    auto* label = new QLabel(text, m_parentWidget);
    label->setStyleSheet(kLabelBoldStyle);
    label->setWordWrap(true);
    layout->addWidget(label);
    return label;
}

void EngineSettingsOptionHandler::addHelpDescription(const QString& optionName, QVBoxLayout* layout)
{
    const QString description = EngineOptionDescriptions::description(m_engineName, optionName);
    if (description.isEmpty()) return;
    auto* label = new QLabel(description, m_parentWidget);
    label->setWordWrap(true);
    label->setStyleSheet("QLabel { color: palette(text); padding: 2px 0px 6px 0px; background: transparent; }");
    layout->addWidget(label);
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createEditor(const EngineOption& option,
                                                                                    QVBoxLayout* layout)
{
    if (option.type == EngineOption::kTypeFilename || option.type == EngineOption::kTypeString)
        return createTextBox(option, layout);
    if (option.type == EngineOption::kTypeSpin) return createSpinBox(option, layout);
    if (option.type == EngineOption::kTypeCheck) return createCheckBox(option, layout);
    if (option.type == EngineOption::kTypeButton) return createButton(option, layout);
    if (option.type == EngineOption::kTypeCombo) return createComboBox(option, layout);
    return {};
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createTextBox(const EngineOption& option,
                                                                                     QVBoxLayout* layout)
{
    QLabel* name = addNameLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), layout);
    addHelpDescription(option.name, layout);

    auto* lineEdit = new QLineEdit(m_parentWidget);
    lineEdit->setText(option.currentValue);
    lineEdit->setObjectName(option.name);
    lineEdit->setAccessibleName(option.name);
    name->setBuddy(lineEdit);
    layout->addWidget(lineEdit);

    // オプション名に "Dir"・"File"・"Log" を含むときだけ、ファイル・フォルダを選ぶボタンを出す
    const bool directory = option.name.contains(QLatin1String("Dir"), Qt::CaseInsensitive);
    if (directory || option.name.contains(QLatin1String("File"), Qt::CaseInsensitive)
        || option.name.contains(QLatin1String("Log"), Qt::CaseInsensitive)) {
        layout->addWidget(new PathSelectButton(
            lineEdit, directory ? EngineSettings::FileType::Directory : EngineSettings::FileType::File, m_parentWidget));
    }
    return {std::bind(&QLineEdit::setText, lineEdit, std::placeholders::_1), std::bind(&QLineEdit::text, lineEdit)};
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createSpinBox(const EngineOption& option,
                                                                                     QVBoxLayout* layout)
{
    QLabel* name = addNameLabel(option.name, layout);
    addHelpDescription(option.name, layout);

    // 入力できる範囲の説明
    auto* range = new QLabel(option.name == QLatin1String("USI_Hash")
                                 ? tr("%1 以上の値を入力してください。（既定値: %2）").arg(option.min, option.defaultValue)
                                 : tr("%1 から %2 までの値を入力してください。（既定値: %3）")
                                       .arg(option.min, option.max, option.defaultValue),
                             m_parentWidget);
    range->setStyleSheet("QLabel { color: palette(text); background: transparent; }");
    range->setWordWrap(true);
    layout->addWidget(range);

    auto* spinBox = new LongLongSpinBox(m_parentWidget);
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spinBox->setSpinBoxRange(option.min.toLongLong(), option.max.toLongLong());
    spinBox->setSpinBoxValue(option.currentValue.toLongLong());
    spinBox->setFocusPolicy(Qt::StrongFocus);
    spinBox->installEventFilter(this);
    spinBox->setObjectName(option.name);
    spinBox->setAccessibleName(option.name);
    name->setBuddy(spinBox);
    layout->addWidget(spinBox);
    return {[spinBox](const QString& value) { spinBox->setSpinBoxValue(value.toLongLong()); },
            [spinBox]() { return QString::number(spinBox->value()); }};
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createCheckBox(const EngineOption& option,
                                                                                      QVBoxLayout* layout)
{
    auto* checkBox = new QCheckBox(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), m_parentWidget);
    checkBox->setChecked(option.currentValue == QLatin1String("true"));
    checkBox->setObjectName(option.name);
    layout->addWidget(checkBox);
    addHelpDescription(option.name, layout);
    return {[checkBox](const QString& value) { checkBox->setChecked(value == QLatin1String("true")); },
            [checkBox]() { return checkBox->isChecked() ? QStringLiteral("true") : QStringLiteral("false"); }};
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createButton(const EngineOption& option,
                                                                                    QVBoxLayout* layout)
{
    auto* button = new OptionToggleButton(option.name, m_parentWidget);
    button->setChecked(option.currentValue == QLatin1String("on"));
    button->setObjectName(option.name);
    layout->addWidget(button);
    addHelpDescription(option.name, layout);
    return {[button](const QString& value) { button->setChecked(value == QLatin1String("on")); },
            [button]() { return button->isChecked() ? QStringLiteral("on") : QString(); }};
}

EngineSettingsOptionHandler::OptionEditor EngineSettingsOptionHandler::createComboBox(const EngineOption& option,
                                                                                      QVBoxLayout* layout)
{
    QLabel* name = addNameLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), layout);
    addHelpDescription(option.name, layout);

    auto* comboBox = new QComboBox(m_parentWidget);
    comboBox->setFocusPolicy(Qt::StrongFocus);
    comboBox->installEventFilter(this);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    comboBox->setMinimumContentsLength(20);
    comboBox->addItems(option.valueList);
    const int current = comboBox->findText(option.currentValue);
    if (current != -1) comboBox->setCurrentIndex(current);
    comboBox->setObjectName(option.name);
    comboBox->setAccessibleName(option.name);
    name->setBuddy(comboBox);
    layout->addWidget(comboBox);
    return {[comboBox](const QString& value) {
                const int index = comboBox->findText(value);
                if (index != -1) comboBox->setCurrentIndex(index);
            },
            std::bind(&QComboBox::currentText, comboBox)};
}

// カテゴリ別の折りたたみ可能なグループボックスを作成する。
CollapsibleGroupBox* EngineSettingsOptionHandler::createCategoryGroupBox(EngineOptionCategory category)
{
    QString categoryName = EngineOptionDescriptions::categoryDisplayName(category);
    return new CollapsibleGroupBox(categoryName, m_parentWidget);
}

// エンジンオプションを既定値に戻す。
void EngineSettingsOptionHandler::restoreDefaultOptions()
{
    for (qsizetype i = 0; i < m_optionList.size() && i < m_editors.size(); ++i) {
        if (m_editors.at(i).setValue) m_editors.at(i).setValue(m_optionList.at(i).defaultValue);
    }
}

// ウィジェットの現在値を読み取り設定ファイルに保存する。
void EngineSettingsOptionHandler::writeEngineOptions()
{
    if (m_optionList.size() != m_editors.size()) {
        qWarning() << "writeEngineOptions: リストサイズ不一致 - m_optionList:" << m_optionList.size()
                   << "m_editors:" << m_editors.size();
        return;
    }
    // 入力欄のないオプション（GUI 共通の USI_Ponder）は読んだ値のまま書く
    for (qsizetype i = 0; i < m_optionList.size(); ++i) {
        if (m_editors.at(i).value) m_optionList[i].currentValue = m_editors.at(i).value();
    }
    // 名前・型・選択肢も含めて全体を書き戻す（読み込んだときの並びに頼らない）
    EngineProfileStore::saveOptions(m_engineName, m_optionList);
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
