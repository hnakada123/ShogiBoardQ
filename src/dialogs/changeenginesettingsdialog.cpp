#include "changeenginesettingsdialog.h"
#include "enginesettingsconstants.h"
#include "ui_changeenginesettingsdialog.h"
#include <QSettings>
#include <QGroupBox>
#include <QPushButton>
#include <QDir>
#include <QFileDialog>

using namespace EngineSettingsConstants;

// 将棋エンジンの設定を変更するダイアログ
// コンストラクタ
ChangeEngineSettingsDialog::ChangeEngineSettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ChangeEngineSettingsDialog)
{
    ui->setupUi(this);
}

// デストラクタ
ChangeEngineSettingsDialog::~ChangeEngineSettingsDialog()
{
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

// 設定ファイルから選択したエンジンのオプションを読み込む。
void ChangeEngineSettingsDialog::readEngineOptions()
{
    // オプションリストを初期化する。
    m_optionList.clear();

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
    // オプション数までループする。
    for (qsizetype i = 0; i < m_engineOptionWidgetsList.size(); i++) {
        // オプションタイプが"button"の場合
        if (m_optionList.at(i).type == "button") {
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

    // ボタンのプロパティから、どのボタンがクリックされたのかを識別するためのインデックスを取得する。
    int index = qvariant_cast<int>(pButton->property("index"));

    // ボタンが押された状態の場合
    if (m_engineOptionWidgetsList.at(index).selectionButton->isChecked()) {
        m_engineOptionWidgetsList.at(index).selectionButton->setStyleSheet("background-color: #3daee9; color: white");
    }
    // ボタンが押されていない場合
    else {
        m_engineOptionWidgetsList.at(index).selectionButton->setStyleSheet("");
    }
}

// エンジンオプションのためのテキストボックスを作成する。
void ChangeEngineSettingsDialog::createTextBox(const EngineOption& option, QVBoxLayout* layout)
{
    // オプション名でラベルを作成する。
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), this);

    // テキスト入力用のLineEditを作成する。
    m_optionWidgets.lineEdit = new QLineEdit(this);

    // 現在値をセットする。
    m_optionWidgets.lineEdit->setText(option.currentValue);

    // ファイルタイプを判定して対応するボタンを作成する。
    m_optionWidgets.selectionButton = new QPushButton(this);

    // デフォルトをファイルタイプに設定する。
    EngineSettings::FileType fileType = EngineSettings::FileType::File;

    // オプション名に"Dir"もしくは"dir"が含まれるかチェックして、対応するボタンのテキストを設定する。
    if (option.name.contains("Dir", Qt::CaseInsensitive)) {
        m_optionWidgets.selectionButton->setText(tr("フォルダ・ディレクトリの選択"));

        // ディレクトリタイプに設定
        fileType = EngineSettings::FileType::Directory;
    } else {
        m_optionWidgets.selectionButton->setText(tr("ファイルの選択"));

        // ファイルタイプに設定する。
        fileType = EngineSettings::FileType::File;
    }

    // ボタンのプロパティにインデックスとファイルタイプを設定する。
    // layoutに既に追加された要素の数をインデックスとして使用する。
    m_optionWidgets.selectionButton->setProperty("index", layout->count());
    m_optionWidgets.selectionButton->setProperty("fileType", static_cast<int>(fileType));

    // ボタンのクリックイベントに対するスロットを接続
    connect(m_optionWidgets.selectionButton, &QPushButton::clicked, this, &ChangeEngineSettingsDialog::openFile);

    // UI要素を指定されたレイアウトに追加する。
    layout->addWidget(m_optionWidgets.optionNameLabel);
    layout->addWidget(m_optionWidgets.lineEdit);
    layout->addWidget(m_optionWidgets.selectionButton);

    // テキストボックスのオブジェクト名を設定
    m_optionWidgets.lineEdit->setObjectName(option.name);
}

// エンジンオプションのためのスピンボックスを作成する。
void ChangeEngineSettingsDialog::createSpinBox(const EngineOption& option, QVBoxLayout* layout)
{
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1").arg(option.name), this);

    // オプション名が"USI_Hash"の場合、入力値制限についてのラベルを作成する。
    if (option.name == "USI_Hash") {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 以上の値を入力してください。（既定値: %2）")
                                    .arg(option.min, option.defaultValue), this);
    } else {
        m_optionWidgets.optionDescriptionLabel = new QLabel(tr("%1 から %2 までの値を入力してください。（既定値: %3）")
                                    .arg(option.min, option.max, option.defaultValue), this);
    }

    // スピンボックスを作成する。
    m_optionWidgets.integerSpinBox = new LongLongSpinBox(this);

    // スタイルシートの確認と修正
    m_optionWidgets.integerSpinBox->setStyleSheet("");

    // スピンボックスの範囲を設定する。オプションのminとmaxを使用する。
    m_optionWidgets.integerSpinBox->setSpinBoxRange(option.min.toLongLong(), option.max.toLongLong());

    // スピンボックスの現在値を設定する。オプションの現在値を使用する。
    m_optionWidgets.integerSpinBox->setSpinBoxValue(option.currentValue.toLongLong());

    // スピンボックスのフォーカスポリシーを設定する。これはマウスクリックによってのみフォーカスされるようにするためである。
    m_optionWidgets.integerSpinBox->setFocusPolicy(Qt::ClickFocus);

    // このダイアログ自体をスピンボックスのイベントフィルタとしてインストールする。これにより、特定のイベントをこのダイアログで処理できるようになる。
    m_optionWidgets.integerSpinBox->installEventFilter(this);

    // オプション名のラベルとスピンボックスをレイアウトに追加する。
    layout->addWidget(m_optionWidgets.optionNameLabel);
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

    // チェックボックスにオプション名と既定値のテキストをセット
    m_optionWidgets.optionCheckBox->setText(tr("%1（既定値 %2）").arg(option.name, option.defaultValue));

    // 設定ファイルのオプションの現在値に基づいてチェック状態を設定
    m_optionWidgets.optionCheckBox->setChecked(option.currentValue == "true");

    // チェックボックスをレイアウトに追加
    layout->addWidget(m_optionWidgets.optionCheckBox);

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
        m_optionWidgets.selectionButton->setStyleSheet("background-color: #3daee9; color: white");
    } else {
        m_optionWidgets.selectionButton->setStyleSheet("");
    }

    // ボタンをレイアウトに追加
    layout->addWidget(m_optionWidgets.selectionButton);

    // ボタンのオブジェクト名を設定
    m_optionWidgets.selectionButton->setObjectName(option.name);
}

// エンジンオプションのためのコンボボックスを作成する。
void ChangeEngineSettingsDialog::createComboBox(const EngineOption& option, QVBoxLayout* layout)
{
    // オプション名のラベルを作成
    m_optionWidgets.optionNameLabel = new QLabel(tr("%1（既定値 %2）").arg(option.name, option.defaultValue), this);

    // コンボボックスを作成
    m_optionWidgets.comboBox = new QComboBox(this);
    m_optionWidgets.comboBox->setFocusPolicy(Qt::StrongFocus);
    m_optionWidgets.comboBox->installEventFilter(this);

    // コンボボックスに選択肢を追加
    for (const QString& value : option.valueList) {
        m_optionWidgets.comboBox->addItem(value);
    }

    // コンボボックスの現在選択をオプションの現在値に設定
    int currentIndex = m_optionWidgets.comboBox->findText(option.currentValue);
    if (currentIndex != -1) { // 該当する項目が見つかった場合
        m_optionWidgets.comboBox->setCurrentIndex(currentIndex);
    }

    // ラベルとコンボボックスをレイアウトに追加
    layout->addWidget(m_optionWidgets.optionNameLabel);
    layout->addWidget(m_optionWidgets.comboBox);

    // コンボボックスのオブジェクト名を設定
    m_optionWidgets.comboBox->setObjectName(option.name);
}

// オプションのタイプに応じたUIコンポーネントを作成し、レイアウトに追加する。
void ChangeEngineSettingsDialog::createWidgetForOption(const EngineOption& option)
{
    // オプションタイプが"filename"か"string"の場合
    if (option.type == "filename" || option.type == "string") {
        // エンジンオプションのためのテキストボックスを作成する。
        createTextBox(option, optionWidgetsLayout);
    }
    // オプションタイプが"spin"の場合
    else if (option.type == "spin") {
        // エンジンオプションのためのスピンボックスを作成する。
        createSpinBox(option, optionWidgetsLayout);
    }
    // オプションタイプが"check"の場合
    else if (option.type == "check") {
        // エンジンオプションのためのチェックボックスを作成する。
        createCheckBox(option, optionWidgetsLayout);
    }
    // オプションタイプが"button"の場合
    else if (option.type == "button") {
        // エンジンオプションのためのボタンを作成する。
        createButton(option, optionWidgetsLayout);

    }
    // オプションタイプが"combo"の場合
    else if (option.type == "combo") {
        // エンジンオプションのためのコンボボックスを作成する。
        createComboBox(option, optionWidgetsLayout);
    }
}

// エンジンオプションに基づいてUIコンポーネントを作成して配置する。
void ChangeEngineSettingsDialog::createOptionWidgets()
{
    // 設定画面の上部にエンジン名を表示する。
    ui->label->setText(m_engineName);

    // 画面レイアウトを作成する。
    optionWidgetsLayout = new QVBoxLayout;

    // オプション数でループさせる。
    for (const auto& option : std::as_const(m_optionList)) {
        // オプションのタイプに応じたUIコンポーネントを作成し、レイアウトに追加する。
        createWidgetForOption(option);

        // 各オプションの状態をメニューリストに追加する。
        m_engineOptionWidgetsList.append(m_optionWidgets);
    }

    // 各ボタンが押されているかどうかを確認し、ボタンの色を設定する。
    changeStatusColorTypeButton();

    // レイアウトを上部に配置する。
    optionWidgetsLayout->setAlignment(Qt::AlignTop);

    // ダイアログのスクロールアリアのウィジェットにレイアウトをセットする。
    ui->scrollAreaWidgetContents->setLayout(optionWidgetsLayout);
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

    // ダイアログのサイズをコンテンツに合わせて固定する。
    this->setFixedSize(this->geometry().width(), this->geometry().height());
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
    int fileType = qvariant_cast<int>(pButton->property("filetype"));

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
        if (option.type == "filename" || option.type == "string") {
            // ファイル名または文字列オプションの場合、LineEditに既定値を設定
            widget.lineEdit->setText(option.defaultValue);
        } else if (option.type == "spin") {
            // 数値オプションの場合、SpinBoxに既定値を設定（文字列を数値に変換）
            widget.integerSpinBox->setSpinBoxValue(option.defaultValue.toLongLong());
        } else if (option.type == "check") {
            // チェックボックスオプションの場合、既定値が"true"ならチェック
            widget.optionCheckBox->setChecked(option.defaultValue == "true");
        } else if (option.type == "button") {
            // ボタンオプションの場合、既定値が"on"ならボタンをチェック状態に
            bool isChecked = option.defaultValue == "on";
            widget.selectionButton->setChecked(isChecked);
            // チェック状態に応じてボタンのスタイルを変更
            widget.selectionButton->setStyleSheet(isChecked ? "background-color: #3daee9; color: white" : "");
        } else if (option.type == "combo") {
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
    // エンジンオプションの数だけ繰り返す。
    for (qsizetype i = 0; i < m_optionList.size(); i++) {
        EngineOption option = m_optionList.at(i);

        if (option.type == "filename" || option.type == "string") {
            option.currentValue = m_engineOptionWidgetsList.at(i).lineEdit->text();
        } else if (option.type == "spin") {
            option.currentValue = QString::number(m_engineOptionWidgetsList.at(i).integerSpinBox->value());
        } else if (option.type == "check") {
            option.currentValue = m_engineOptionWidgetsList.at(i).optionCheckBox->isChecked() ? "true" : "false";
        } else if (option.type == "button") {
            option.currentValue = m_engineOptionWidgetsList.at(i).selectionButton->isChecked() ? "on" : "";
        } else if (option.type == "combo") {
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
