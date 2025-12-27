#include "engineregistrationdialog.h"
#include "enginesettingsconstants.h"
#include "qdebug.h"
#include "ui_engineregistrationdialog.h"
#include "changeenginesettingsdialog.h"
#include <QDir>
#include <QFileDialog>
#include <QProcess>
#include <QTime>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QThread>

using namespace EngineSettingsConstants;

// 将棋エンジン登録ダイアログを表示する。
// コンストラクタ
EngineRegistrationDialog::EngineRegistrationDialog(QWidget *parent)
    : QDialog(parent),
    ui(new Ui::EngineRegistrationDialog),
    m_process(nullptr),
    m_errorOccurred(false)
{
    // Qt Designerで作成されたUIをプログラムのウィンドウに読み込み、初期化する。
    ui->setupUi(this);

    // シグナル・スロットの接続を行う。
    initializeSignals();

    // 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込み、GUIのリストウィジェットにエンジン名を追加する。
    loadEnginesFromSettings();
}

// デストラクタ
EngineRegistrationDialog::~EngineRegistrationDialog()
{
    // 既存のプロセスが存在する場合
    if (m_process != nullptr) {
        // プロセスのシグナル・スロットの接続を解除する。
        disconnect(m_process, nullptr, this, nullptr);

        // プロセスの状態が実行中の場合
        if (m_process->state() == QProcess::Running) {
            // プロセスを終了する。
            m_process->terminate();

            // プロセスの終了を待機する。
            if (!m_process->waitForFinished(3000)) {
                // プロセスを強制終了する。
                m_process->kill();
            }
        }

        // プロセスオブジェクトを削除する。
        delete m_process;
    }

    // エンジン登録ダイアログのUIを削除する。
    delete ui;
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
}

// プロセスの標準エラー出力を処理する。
void EngineRegistrationDialog::processEngineErrorOutput()
{
    // エンジンのプロセスから標準エラー出力を読み込む。
    QByteArray stderrBytes = m_process->readAllStandardError();

    QString stderrString = QString::fromUtf8(stderrBytes).trimmed();

    if (!stderrString.isEmpty()) {
        // エラーメッセージと発生時刻をログに記録
        qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " - Engine Error Output:" << stderrString;

        // エラーメッセージを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::processEngineErrorOutput. Engine error output: %1").arg(stderrString));
    }
}

// QProcessのエラーが発生したときに呼び出されるスロット
void EngineRegistrationDialog::onProcessError(QProcess::ProcessError error)
{
    QString errorMessage;

    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. The process failed to start.");
        break;
    case QProcess::Crashed:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. The process crashed.");
        break;
    case QProcess::Timedout:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. The process timed out.");
        break;
    case QProcess::WriteError:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. An issue occurred while writing data.");
        break;
    case QProcess::ReadError:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. An issue occurred while reading data.");
        break;
    case QProcess::UnknownError:
    default:
        errorMessage = tr("An error occurred in EngineRegistrationDialog::onProcessError. An unknown error occurred.");
        break;
    }

    // エラーが発生したことを通知する。
    showErrorMessage(errorMessage);
}

// エラーメッセージを表示する。
void EngineRegistrationDialog::showErrorMessage(const QString &errorMessage)
{
    // エラー状態を設定する。
    m_errorOccurred = true;

    // エラーメッセージを表示する。
    QMessageBox::critical(this, "Error", errorMessage);
}

// 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込み、GUIのリストウィジェットにエンジン名を追加する。
void EngineRegistrationDialog::loadEnginesFromSettings()
{
    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // [Engines]セクションからエンジンの数を読み込む。
    int engineCount = settings.beginReadArray(EnginesGroupName);

    // 既存のエンジンリストをクリア
    m_engineList.clear();

    // GUIのリストもクリア
    ui->engineListWidget->clear();

    for (int i = 0; i < engineCount; ++i) {
        settings.setArrayIndex(i);

        // 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込む。
        Engine engine = readEngineFromSettings(settings);

        // 読み込んだエンジンをリストに追加する。
        m_engineList.append(engine);

        // GUIのリストウィジェットにエンジン名を追加する。
        ui->engineListWidget->addItem(engine.name);
    }

    // [Engines]グループの配列の読み込みを終了する。
    settings.endArray();
}

// 設定ファイルからエンジン名と絶対パス付きの実行ファイル名を読み込む。
Engine EngineRegistrationDialog::readEngineFromSettings(const QSettings& settings) const
{
    Engine engine;

    // 将棋エンジンの名前と実行ファイルの絶対パスを設定ファイルから読み込む。
    engine.name = settings.value(EngineNameKey).toString();
    engine.path = settings.value(EnginePathKey).toString();

    return engine;
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
    m_fileName = QDir::toNativeSeparators(fileName);

    // ファイルが選択されなかった場合、処理を中断する。
    if (m_fileName.isEmpty()) {
        // ファイルが選択されなかった場合、処理を中断する。
        return;
    }

    // 将棋エンジンのディレクトリを取得する。
    m_engineDir = QDir::currentPath();

    // 既にリストに同じエンジンがあるかどうかをチェック
    foreach (const Engine& engine, m_engineList) {
        if (engine.path == m_fileName) {
            // エンジン登録が重複している場合、重複エラーのメッセージを表示する。
            handleDuplicateEngine(engine.name);

            // 重複が見つかった場合は、処理を中断する。
            return;
        }
    }

    // ファイルのパスが有効でない場合、エラーメッセージを表示し、処理を中断する。
    if (!validateEnginePath(m_fileName)) {
        // エラーメッセージを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::addEngineFromFileSelection. Could not move to %1. Failed to add shogi engine.").arg(QFileInfo(m_fileName).path()));

        // ディレクトリの検証に失敗した場合、処理を中断する。
        return;
    }

    // エンジンを起動し、usiコマンドを送信する。
    startAndInitializeEngine(m_fileName);
}

// エンジン登録が重複している場合、重複エラーのメッセージを表示する。
void EngineRegistrationDialog::handleDuplicateEngine(const QString& engineName)
{
    // 重複エラーのメッセージを通知する。
    showErrorMessage(tr("An error occurred in EngineRegistrationDialog::handleDuplicateEngine. The engine %1 is already added.").arg(engineName));
}

// ファイルのパスが有効かどうかを検証する。
bool EngineRegistrationDialog::validateEnginePath(const QString& filePath) const
{
    // ファイルのパスからディレクトリのパスを取得する。
    QFileInfo fi(filePath);

    // ディレクトリを移動する。
    return QDir::setCurrent(fi.absolutePath());
}

// 将棋エンジンの出力を１行ずつ読み取り、エンジン情報やオプション情報を取得する。
void EngineRegistrationDialog::processEngineOutput()
{
    while (m_process->canReadLine()) {
        QString line = m_process->readLine();

        // 将棋エンジンからの出力を解析する。
        parseEngineOutput(line.trimmed());

        // エラーが発生している場合
        if (m_errorOccurred) return;
    }
}

// 将棋エンジンからの出力を解析する。
void EngineRegistrationDialog::parseEngineOutput(const QString& line)
{
    // "id name"で始まる行の場合
    if (line.startsWith(IdNamePrefix)) {
        // エンジン名と絶対パスでの実行ファイル名を取得する。
        processIdName(line);
    }
    // "option name"で始まる行の場合
    else if (line.startsWith(OptionNamePrefix)) {
        // オプション行をQStringList型の変数に蓄える。
        m_optionLines.append(line);
    }
    // "usiok"で始まる行の場合
    else if (line.startsWith(UsiOkPrefix)) {
        // 将棋エンジンにquitコマンドを送信する。
        sendQuitCommand();

        // 設定ファイルを書き込むディレクトリに移動する。（現時点では実行ファイルと同じディレクトリ）
        QDir::setCurrent(m_engineDir);

        // usiコマンドの出力からエンジンオプションを取り出す。
        getEngineOptions();

        // エラーが発生している場合
        if (m_errorOccurred) return;

        // 設定ファイルにエンジン名と絶対パス付きの実行ファイル名を書き込む。
        saveEnginesToSettingsFile();

        // comboタイプのオプションの文字列を作成する。
        concatenateComboOptionValues();

        // 設定ファイルに追加エンジンのオプションを書き込む。
        saveEngineOptionsToSettings();

        // エンジン名をリストに追加する。
        ui->engineListWidget->addItem(m_engineIdName);
    }
}

// エンジン名と絶対パスでの実行ファイル名を取得する。
void EngineRegistrationDialog::processIdName(const QString& line)
{
    // "id name"を含んだ行
    m_engineIdName = line;

    // "id name"を含んだ行の先頭の"id name "を削除する。
    m_engineIdName.remove(QString(IdNamePrefix) + " ");

    // 登録エンジンの重複チェック
    for (int j = 0; j < m_engineList.size(); j++) {
        if (m_engineIdName == m_engineList.at(j).name) {
            // 登録したいエンジンが既に登録されている場合、重複エラーのメッセージを表示する。
            duplicateEngine();

            return;
        }
    }

    // エンジンリストにエンジン名と絶対パス付きの実行ファイル名を追加する。
    Engine engine;
    engine.name = m_engineIdName;
    engine.path = m_fileName;
    m_engineList.append(engine);
}

// 将棋エンジンを起動し、usiコマンドを送信する。
void EngineRegistrationDialog::startAndInitializeEngine(const QString& engineFile)
{
    // 将棋エンジンを起動する。
    startEngine(engineFile);

    // エラーが発生している場合
    if (m_errorOccurred) return;

    // usiコマンドを将棋エンジンに送信する。
    sendUsiCommand();
}

// 将棋エンジンを起動する。
void EngineRegistrationDialog::startEngine(const QString& engineFile)
{
    // エンジンファイルの存在を確認
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        // エラーメッセージを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::startEngine. The specified engine file does not exist: %1").arg(engineFile));

        return;
    }

    // 旧プロセスが存在する場合
    if (m_process != nullptr) {
        // 旧プロセスのシグナル・スロットの接続を解除する。
        disconnect(m_process, nullptr, this, nullptr);

        // プロセスを終了する。
        m_process->terminate();

        // プロセスの終了を待機する。
        m_process->waitForFinished();

        // プロセスを削除する。
        delete m_process;

        // プロセスをnullptrに設定する。
        m_process = nullptr;
    }

    // 新しいプロセスを作成する。
    m_process = new QProcess;

    // 標準出力が読み取り可能になったときにプロセスの出力を処理するスロットを接続
    connect(m_process, &QProcess::readyReadStandardOutput, this, &EngineRegistrationDialog::processEngineOutput);

    // 標準エラー出力が読み取り可能になったときにプロセスのエラー出力を処理するスロットを接続
    connect(m_process, &QProcess::readyReadStandardError, this, &EngineRegistrationDialog::processEngineErrorOutput);

    // プロセスのエラーが発生したときに呼び出されるスロットを接続
    connect(m_process, &QProcess::errorOccurred, this, &EngineRegistrationDialog::onProcessError);

    // 将棋エンジンの起動引数を設定（必要に応じて）
    QStringList args;

    // 将棋エンジンを起動
    m_process->start(engineFile, args, QIODevice::ReadWrite);

    if (!m_process->waitForStarted()) {
        // エラーメッセージを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::startEngine.　Failed to start the engine： %1").arg(engineFile));

        return;
    }
}

// usiコマンドを将棋エンジンに送信する。
void EngineRegistrationDialog::sendUsiCommand() const
{
    // 将棋エンジンにusiコマンドを送信する。
    m_process->write(UsiCommand);
}

// quitコマンドを将棋エンジンに送信する。
void EngineRegistrationDialog::sendQuitCommand() const
{
    // 将棋エンジンにquitコマンドを送信する。
    m_process->write(QuitCommand);
}

// 削除ボタンが押されたときに呼び出されるスロット
// 選択した登録エンジンを削除する。
void EngineRegistrationDialog::removeEngine()
{
    // 削除エンジン名
    QString removeEngineName;

    // エンジン登録ダイアログの削除エンジン名を削除する。
    removeSelectedEngineFromList(removeEngineName);

    // [Engines]グループを削除する。
    removeEnginesGroup();

    // [Engines]グループを新たに作り直しする。
    saveEnginesToSettingsFile();

    // 削除エンジン名グループの削除
    removeEngineNameGroup(removeEngineName);
}

// 登録したいエンジンが既に登録されている場合、重複エラーのメッセージを表示する。
void EngineRegistrationDialog::duplicateEngine()
{
    // エラーメッセージを通知する。
    showErrorMessage(tr("The shogi engine is already registered. Please delete the previously registered shogi engine first. Please select one shogi engine."));
}

// 設定ボタンが押されたときに呼び出されるスロット
// 選択したエンジンの設定を変更する。
void EngineRegistrationDialog::configureEngine()
{
    QList<QListWidgetItem *> items = ui->engineListWidget->selectedItems();

    // 選択されたアイテムが正確に一つであるかをチェックする。
    if (items.count() != 1) {
        // エラーメッセージを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::startEngine.　Please select one shogi engine."));

        // 一つではない場合は、処理を中断して戻る。
        return;
    }

    // ここからは選択されたアイテムが正確に一つであることが保証されている。
     // 最初（唯一の）アイテムを取得する。
    QListWidgetItem* selectedItem = items.first();

    // 選択されたアイテムのエンジン番号を取得する。
    int engineNumber = ui->engineListWidget->row(selectedItem);

    // 選択されたアイテムのエンジン名を取得する。
    QString engineName = selectedItem->text();

    // 選択されたエンジンの設定変更ダイアログを表示する。
    ChangeEngineSettingsDialog dialog(this);

    dialog.setEngineNumber(engineNumber);
    dialog.setEngineName(engineName);
    dialog.setupEngineOptionsDialog();

    if (dialog.exec() == QDialog::Rejected) return;
}

// 設定ファイルにエンジン名と絶対パス付きの実行ファイル名を書き込む。
void EngineRegistrationDialog::saveEnginesToSettingsFile() const
{
    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 書き込むグループに[Engines]を指定する。
    settings.beginWriteArray(EnginesGroupName);

    // 登録エンジンの数だけ繰り返す。
    for (int i = 0; i < m_engineList.size(); i++) {
        // 設定ファイルの[Engines]グループの配列のi番目の要素に移動する。
        settings.setArrayIndex(i);

        // 設定ファイルの[Engines]グループの配列のi番目の要素にエンジン名と絶対パス付きの実行ファイル名を書き込む。
        saveEngineToSettings(settings, m_engineList.at(i));
    }

    // [Engines]グループの配列の書き込みを終了する。
    settings.endArray();
}

// 指定されたエンジン情報を設定ファイルに保存する。
void EngineRegistrationDialog::saveEngineToSettings(QSettings& settings, const Engine& engine) const
{
    // 指定されたエンジン情報を設定ファイルに保存する。
    settings.setValue(EngineNameKey, engine.name);

    // エンジンの実行ファイルパスを設定ファイルに書き込む。
    settings.setValue(EnginePathKey, engine.path);
}

// 設定ファイルに追加エンジンのオプションを書き込む。
void EngineRegistrationDialog::saveEngineOptionsToSettings() const
{
    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // 書き込むグループにエンジン名を指定する。
    settings.beginWriteArray(m_engineIdName);

    // エンジンオプションの数だけ繰り返す。
    for (int i = 0; i < m_engineOptions.size(); i++) {
        // 設定ファイルのエンジン名のグループの配列のi番目の要素に移動する。
        settings.setArrayIndex(i);

        // 設定ファイルのエンジン名のグループの配列のi番目の要素にエンジンオプションを書き込む。
        settings.setValue(EngineOptionNameKey, m_engineOptions.at(i).name);
        settings.setValue(EngineOptionTypeKey, m_engineOptions.at(i).type);
        settings.setValue(EngineOptionDefaultKey, m_engineOptions.at(i).defaultValue);
        settings.setValue(EngineOptionMinKey, m_engineOptions.at(i).min);
        settings.setValue(EngineOptionMaxKey, m_engineOptions.at(i).max);
        settings.setValue(EngineOptionValueKey, m_engineOptions.at(i).currentValue);
        settings.setValue(EngineOptionValueListKey, m_concatenatedOptionValuesList.at(i));
    }

    // エンジン名のグループの配列の書き込みを終了する。
    settings.endArray();
}

// usiコマンドの出力からエンジンオプションを取り出す。
void EngineRegistrationDialog::getEngineOptions()
{
    // optionリストを初期化する。これをしないと以前読み込んだエンジンのoptionリストが残ってしまう。
    m_engineOptions.clear();

    // usiコマンドの出力からエンジンオプションを取り出す。
    for (const QString& line : std::as_const(m_optionLines)) {
        parseOptionLine(line);

        // エラーが発生している場合
        if (m_errorOccurred) return;
    }

    // USI_Hashオプションが存在するかを確認する。
    auto usiHashIt = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [](const EngineOption& option) {
        return option.name == "USI_Hash";
    });

    // USI_Hashオプションが存在しない場合は追加する。
    if (usiHashIt == m_engineOptions.end()) {
        addOption("USI_Hash", "spin", "1024", "1", "2000", "1024");

        qInfo() << tr("USI_Hash option added.");
    }

    // USI_Ponderオプションが存在するかを確認
    auto usiPonderIt = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [](const EngineOption& option) {
        return option.name == "USI_Ponder";
    });

    // USI_Ponderオプションが存在しない場合は追加する。
    if (usiPonderIt == m_engineOptions.end()) {
        addOption("USI_Ponder", "check", "false", "", "", "false");

        qInfo() <<  tr("USI_Ponder option added.");
    }
}

// エンジンオプション構造体に値を設定する。
void EngineRegistrationDialog::addOption(const QString& name, const QString& type, const QString& defaultValue, const QString& min, const QString& max, const QString& currentValue)
{
    EngineOption option{name, type, defaultValue, min, max, currentValue, QStringList()};
    m_engineOptions.append(option);
    qInfo() << tr("New option added:") << name << ", type:" << type;
}

// optionコマンドの文法が正しいかどうかをチェックする。
EngineRegistrationDialog::ValidationResult EngineRegistrationDialog::checkOptionSyntax(const QString& optionCommand)
{
    // optionコマンドは"option"で始まる必要がある。
    if (!optionCommand.startsWith("option")) {
        return {false, "Command does not start with 'option'"};
    }

    // "option"を除いた部分を取得する。
    QString commandBody = optionCommand.mid(QString("option").length()).trimmed();

    // 静的なQRegularExpressionオブジェクト
    static const QRegularExpression optionRegex(R"(name\s+(\S+)\s+type\s+(\S+)(.*))");
    static const QRegularExpression whitespaceRegex(R"(\s+)");

    // name, type は必須フィールド
    QRegularExpressionMatch match = optionRegex.match(commandBody);

    if (!match.hasMatch()) {
        return {false, "Invalid syntax. Expected 'option name <name> type <type>'"};
    }

    QString type = match.captured(2);
    QString remaining = match.captured(3).trimmed();

    // typeがbuttonの場合、追加オプションがあってはいけない。
    if (type == "button") {
        if (!remaining.isEmpty()) {
            return {false, "No additional options are allowed for type 'button'"};
        }
    } else {
        // 追加オプションをチェックする。
        QStringList tokens = remaining.split(whitespaceRegex);
        QStringList allowedKeywords = {"default", "min", "max", "var"};

        for (int i = 0; i < tokens.size(); ++i) {
            if (allowedKeywords.contains(tokens[i])) {
                if (tokens[i] == "default") {
                    // "type" が "string" であれば、値がなくてもOK。
                    if (type == "string" && (i + 1 >= tokens.size() || allowedKeywords.contains(tokens[i + 1]))) {
                        // 値がなくても続行する。
                        continue;
                    } else if (i + 1 < tokens.size() && !allowedKeywords.contains(tokens[i + 1])) {
                        // 値がある場合は次のトークンに進む。
                        ++i;
                    } else {
                        return {false, QString("Value for '%1' is missing or invalid").arg(tokens[i])};
                    }
                } else if (tokens[i] == "min" || tokens[i] == "max") {
                    if (i + 1 < tokens.size() && !allowedKeywords.contains(tokens[i + 1])) {
                        // 値をスキップする。
                        ++i;
                    } else {
                        return {false, QString("Value for '%1' is missing or invalid").arg(tokens[i])};
                    }
                } else if (tokens[i] == "var") {
                    while (i + 1 < tokens.size() && !allowedKeywords.contains(tokens[i + 1])) {
                        ++i;
                    }
                }
            } else {
                return {false, QString("Unknown keyword or missing value: '%1'").arg(tokens[i])};
            }
        }
    }

    // ここまで来れば文法チェックは成功である。
    return {true, ""};
}

// 将棋エンジンから送信されたオプション行を解析し、エンジンオプションリストに追加する。
void EngineRegistrationDialog::parseOptionLine(const QString& line)
{
    // オプションの文法が正しいかどうかをチェックする。
    ValidationResult result = checkOptionSyntax(line);

    // 文法が正しい場合
    if (result.isValid) {
        qDebug() << "The option command is valid:" << line;
    }
    // 文法が正しくない場合
    else {
        qDebug() << "The option command is invalid:" << line << result.errorMessage;

        // エラーが発生したことを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::parseOptionLine. Invalid option line format."));

        return;
    }

    QStringList parts = line.split(" ");

    QString name = parts.at(2);
    QString type = parts.at(4);
    QString defaultValue = "";
    QString min = "";
    QString max = "";
    QString currentValue = "";
    QStringList valueList;

    int defaultIndex = static_cast<int>(parts.indexOf(EngineOptionDefaultKey));

    // `defaultIndex + 1 < parts.size()`を追加して、`"default"`の後に値があるかどうかをチェックする。
    if (defaultIndex != -1 && defaultIndex + 1 < parts.size()) {
        defaultValue = parts.at(defaultIndex + 1);
        currentValue = defaultValue;
    }

    if (type == "spin") {
        int minIndex = static_cast<int>(parts.indexOf("min"));
        int maxIndex = static_cast<int>(parts.indexOf("max"));
        if (minIndex != -1 && parts.size() > minIndex) min = parts.at(minIndex + 1);
        if (maxIndex != -1 && parts.size() > maxIndex) max = parts.at(maxIndex + 1);
    } else if (type == "combo") {
        int varIndex = 0;
        while ((varIndex = static_cast<int>(parts.indexOf("var", varIndex + 1))) != -1) {
            if (parts.size() > varIndex) valueList.append(parts.at(varIndex + 1));
        }
    }

    // 新しいオプションを追加する際、既存のオプションと重複しないことを確認
    auto it = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [&name](const EngineOption& option) {
        return option.name == name;
    });

    // デバッグ用に既存のオプションを出力
    qDebug() << "現在のエンジンオプション:";
    for (int i = 0; i < m_engineOptions.size(); ++i) {
        const EngineOption& option = m_engineOptions[i];
        qDebug() << "Name:" << option.name << "Type:" << option.type;
    }

    // オプションが最後まで見つからなかった（重複しなかった）場合
    if (it == m_engineOptions.end()) {
        EngineOption option{name, type, defaultValue, min, max, currentValue, valueList};
        m_engineOptions.append(option);

        qInfo() << "New engine option added:" << name << ", type:" << type;
    } else {
         qWarning() << "Duplicate engine option found:" << name;

        // エラーが発生したことを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::parseOptionLine. Duplicate engine option found."));

        return;
    }
}

// 設定ファイルから[Engines]グループを削除する。
void EngineRegistrationDialog::removeEnginesGroup() const
{
    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // [Engines]グループの削除
    settings.beginGroup(EnginesGroupName);
    settings.remove("");
    settings.endGroup();
}

// 設定ファイルからエンジン名グループを削除する。
void EngineRegistrationDialog::removeEngineNameGroup(const QString& removeEngineName) const
{
    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // エンジン名グループの削除
    settings.beginGroup(removeEngineName);
    settings.remove("");
    settings.endGroup();
}

// エンジン登録ダイアログから選択したエンジンを削除する。
void EngineRegistrationDialog::removeSelectedEngineFromList(QString& removeEngineName)
{
    // 選択したエンジン
    QList<QListWidgetItem *> items = ui->engineListWidget->selectedItems();

    // 選択したエンジンが正確に一つであるかをチェックする。
    if (items.count() != 1) {
        // エラーが発生したことを通知する。
        showErrorMessage(tr("An error occurred in EngineRegistrationDialog::removeSelectedEngineFromList. Please select one shogi engine."));

        // 一つではない場合は、処理を中断する。
        return;
    }

    // ここからは選択されたエンジンが正確に一つであることが保証されている。
    QListWidgetItem* selectedItem = items.first();
    int i = ui->engineListWidget->row(selectedItem);
    removeEngineName = ui->engineListWidget->takeItem(i)->text();

    // アイテムの削除
    delete selectedItem;

    m_engineList.removeAt(i);
}

// comboタイプのオプションの文字列を作成する。
void EngineRegistrationDialog::concatenateComboOptionValues()
{
    // 既存のリストをクリアする。
    m_concatenatedOptionValuesList.clear();

    // オプションの文字列リスト分、繰り返す。
    for (const auto& option : std::as_const(m_engineOptions)) {
        // comboタイプのオプションの値を " " で結合する。
        m_concatenatedOptionValuesList.append(option.valueList.join(" "));
    }
}
