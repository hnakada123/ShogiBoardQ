/// @file engineregistrationhandler.cpp
/// @brief エンジン登録ハンドラクラスの実装（プロセス管理・USI通信・設定永続化）

#include "engineregistrationhandler.h"
#include "engineregistrationdialog.h"
#include "enginesettingsconstants.h"
#include "settingscommon.h"
#include "logcategories.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

using namespace EngineSettingsConstants;

EngineRegistrationHandler::EngineRegistrationHandler(QObject *parent)
    : QObject(parent)
{
}

EngineRegistrationHandler::~EngineRegistrationHandler()
{
    cleanupEngineProcess();
}

// 設定ファイルからエンジンリストを読み込む。
void EngineRegistrationHandler::loadEnginesFromSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    int engineCount = settings.beginReadArray(EnginesGroupName);
    m_engineList.clear();

    for (int i = 0; i < engineCount; ++i) {
        settings.setArrayIndex(i);
        m_engineList.append(readEngineFromSettings(settings));
    }

    settings.endArray();
}

const QList<Engine>& EngineRegistrationHandler::engineList() const
{
    return m_engineList;
}

const Engine& EngineRegistrationHandler::engineAt(int index) const
{
    return m_engineList.at(index);
}

// パスの重複を検査する。
bool EngineRegistrationHandler::isDuplicatePath(const QString& path, QString& existingName) const
{
    for (const Engine& engine : std::as_const(m_engineList)) {
        if (engine.path == path) {
            existingName = engine.name;
            return true;
        }
    }
    return false;
}

// ファイルのパスが有効かどうかを検証する。
bool EngineRegistrationHandler::validateEnginePath(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return QDir::setCurrent(fi.absolutePath());
}

// エンジン登録処理を開始する。
void EngineRegistrationHandler::startRegistration(const QString& filePath)
{
    m_errorOccurred = false;
    m_fileName = filePath;
    m_engineDir = QFileInfo(filePath).absolutePath();
    m_engineIdName.clear();
    m_engineIdAuthor.clear();
    m_optionLines.clear();

    startEngine(m_fileName);
    if (m_errorOccurred) return;

    sendUsiCommand();
}

// 指定インデックスのエンジンを削除し設定を更新する。
void EngineRegistrationHandler::removeEngineAt(int index)
{
    QString removeEngineName = m_engineList.at(index).name;
    m_engineList.removeAt(index);

    // [Engines]グループを削除して作り直す
    removeEnginesGroup();
    saveEnginesToSettingsFile();
    removeEngineNameGroup(removeEngineName);
}

// --- プロセスライフサイクル ---

// エンジンプロセスをクリーンアップする。
void EngineRegistrationHandler::cleanupEngineProcess()
{
    if (!m_process) return;

    disconnect(m_process.get(), nullptr, this, nullptr);

    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
    }

    m_process.reset();
}

// 将棋エンジンを起動する。
void EngineRegistrationHandler::startEngine(const QString& engineFile)
{
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        setError(tr("指定されたエンジンファイルが存在しません: %1").arg(engineFile));
        return;
    }

    cleanupEngineProcess();
    m_process = std::make_unique<QProcess>();

    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &EngineRegistrationHandler::processEngineOutput);
    connect(m_process.get(), &QProcess::readyReadStandardError, this, &EngineRegistrationHandler::processEngineErrorOutput);
    connect(m_process.get(), &QProcess::errorOccurred, this, &EngineRegistrationHandler::onProcessError);

    QStringList args;
    m_process->start(engineFile, args, QIODevice::ReadWrite);

    if (!m_process->waitForStarted()) {
        setError(tr("エンジンの起動に失敗しました: %1").arg(engineFile));
    }
}

// usiコマンドを将棋エンジンに送信する。
void EngineRegistrationHandler::sendUsiCommand() const
{
    m_process->write(UsiCommand);
}

// quitコマンドを将棋エンジンに送信する。
void EngineRegistrationHandler::sendQuitCommand() const
{
    m_process->write(QuitCommand);
}

// 将棋エンジンの出力を１行ずつ読み取り、エンジン情報やオプション情報を取得する。
void EngineRegistrationHandler::processEngineOutput()
{
    while (m_process->canReadLine()) {
        QString line = m_process->readLine();
        parseEngineOutput(line.trimmed());
        if (m_errorOccurred) return;
    }
}

// プロセスの標準エラー出力を処理する。
void EngineRegistrationHandler::processEngineErrorOutput()
{
    QByteArray stderrBytes = m_process->readAllStandardError();
    QString stderrString = QString::fromUtf8(stderrBytes).trimmed();

    if (!stderrString.isEmpty()) {
        qCDebug(lcUi) << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " - Engine Error Output:" << stderrString;
        setError(tr("エンジンからエラー出力がありました: %1").arg(stderrString));
    }
}

// QProcessのエラーが発生したときに呼び出されるスロット
void EngineRegistrationHandler::onProcessError(QProcess::ProcessError error)
{
    QString errorMessage;

    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = tr("エンジンの起動に失敗しました。");
        break;
    case QProcess::Crashed:
        errorMessage = tr("エンジンがクラッシュしました。");
        break;
    case QProcess::Timedout:
        errorMessage = tr("エンジンがタイムアウトしました。");
        break;
    case QProcess::WriteError:
        errorMessage = tr("エンジンへのデータ書き込み中にエラーが発生しました。");
        break;
    case QProcess::ReadError:
        errorMessage = tr("エンジンからのデータ読み込み中にエラーが発生しました。");
        break;
    case QProcess::UnknownError:
    default:
        errorMessage = tr("不明なエラーが発生しました。");
        break;
    }

    setError(errorMessage);
}

// --- USIプロトコル解析 ---

// 将棋エンジンからの出力を解析する。
void EngineRegistrationHandler::parseEngineOutput(const QString& line)
{
    if (line.startsWith(IdNamePrefix)) {
        processIdName(line);
    }
    else if (line.startsWith(IdAuthorPrefix)) {
        m_engineIdAuthor = line.mid(QString(IdAuthorPrefix).length() + 1);
    }
    else if (line.startsWith(OptionNamePrefix)) {
        m_optionLines.append(line);
    }
    else if (line.startsWith(UsiOkPrefix)) {
        sendQuitCommand();

        // 設定ファイルを書き込むディレクトリに移動する
        QDir::setCurrent(m_engineDir);

        // "id name" と "id author" の両方を受信した後なので、ここで追加する。
        Engine engine;
        engine.name = m_engineIdName;
        engine.path = m_fileName;
        engine.author = m_engineIdAuthor;
        m_engineList.append(engine);

        parseEngineOptionsFromUsiOutput();
        if (m_errorOccurred) return;

        saveEnginesToSettingsFile();
        concatenateComboOptionValues();
        saveEngineOptionsToSettings();

        emit engineRegistered(m_engineIdName);
    }
}

// エンジン名を取得し重複チェックする。
void EngineRegistrationHandler::processIdName(const QString& line)
{
    m_engineIdName = line;
    m_engineIdName.remove(QString(IdNamePrefix) + " ");

    // 登録エンジンの重複チェック
    for (qsizetype j = 0; j < m_engineList.size(); j++) {
        if (m_engineIdName == m_engineList.at(j).name) {
            setError(tr("この将棋エンジンは既に登録されています。先に登録済みのエンジンを削除してください。"));
            return;
        }
    }
    // 注意: エンジンリストへの追加は "usiok" 受信後に行う（"id author" が後から来るため）
}

// usiコマンドの出力からエンジンオプションを取り出す。
void EngineRegistrationHandler::parseEngineOptionsFromUsiOutput()
{
    // optionリストを初期化する。これをしないと以前読み込んだエンジンのoptionリストが残ってしまう。
    m_engineOptions.clear();

    for (const QString& line : std::as_const(m_optionLines)) {
        parseOptionLine(line);
        if (m_errorOccurred) return;
    }

    // USI_Hashオプションが存在しない場合は追加する。
    auto usiHashIt = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [](const EngineOption& option) {
        return option.name == "USI_Hash";
    });

    if (usiHashIt == m_engineOptions.end()) {
        addOption("USI_Hash", OptionTypeSpin, "1024", "1", "2000", "1024");
        qCDebug(lcUi) << tr("USI_Hash option added.");
    }

    // 注: USI_Ponderはエンジンが報告した場合のみ使用する。
    // 詰み探索専用エンジン等はUSI_Ponderを実装していないため、
    // 自動追加するとsetoptionでエラーになる。
}

// optionコマンドの文法が正しいかどうかをチェックする。
EngineRegistrationHandler::ValidationResult EngineRegistrationHandler::checkOptionSyntax(const QString& optionCommand)
{
    if (!optionCommand.startsWith("option")) {
        return {false, "Command does not start with 'option'"};
    }

    QString commandBody = optionCommand.mid(QString("option").length()).trimmed();

    static const QRegularExpression optionRegex(R"(name\s+(\S+)\s+type\s+(\S+)(.*))");
    static const QRegularExpression whitespaceRegex(R"(\s+)");

    QRegularExpressionMatch match = optionRegex.match(commandBody);
    if (!match.hasMatch()) {
        return {false, "Invalid syntax. Expected 'option name <name> type <type>'"};
    }

    QString type = match.captured(2);
    QString remaining = match.captured(3).trimmed();

    if (type == OptionTypeButton) {
        if (!remaining.isEmpty()) {
            return {false, "No additional options are allowed for type 'button'"};
        }
    } else {
        QStringList tokens = remaining.split(whitespaceRegex, Qt::SkipEmptyParts);
        QStringList allowedKeywords = {"default", "min", "max", "var"};

        for (qsizetype i = 0; i < tokens.size(); ++i) {
            if (allowedKeywords.contains(tokens[i])) {
                if (tokens[i] == "default") {
                    // "type" が "string" であれば、値がなくてもOK。
                    if (type == OptionTypeString && (i + 1 >= tokens.size() || allowedKeywords.contains(tokens[i + 1]))) {
                        continue;
                    } else if (i + 1 < tokens.size() && !allowedKeywords.contains(tokens[i + 1])) {
                        ++i;
                    } else {
                        return {false, QString("Value for '%1' is missing or invalid").arg(tokens[i])};
                    }
                } else if (tokens[i] == "min" || tokens[i] == "max") {
                    if (i + 1 < tokens.size() && !allowedKeywords.contains(tokens[i + 1])) {
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

    return {true, ""};
}

// 将棋エンジンから送信されたオプション行を解析し、エンジンオプションリストに追加する。
void EngineRegistrationHandler::parseOptionLine(const QString& line)
{
    ValidationResult result = checkOptionSyntax(line);
    if (!result.isValid) {
        setError(tr("オプション行の形式が無効です。"));
        return;
    }

    // 空文字列をスキップして分割する（複数のスペースがあっても正しく処理できるようにする）
    QStringList parts = line.split(" ", Qt::SkipEmptyParts);

    if (parts.size() < 5) {
        qCWarning(lcUi) << "parseOptionLine: 不正なオプション行（トークン数不足）:" << line;
        setError(tr("オプション行の形式が無効です。"));
        return;
    }

    QString name = parts.at(2);
    QString type = parts.at(4);
    QString defaultValue;
    QString min;
    QString max;
    QString currentValue;
    QStringList valueList;

    int defaultIndex = static_cast<int>(parts.indexOf(EngineOptionDefaultKey));
    if (defaultIndex != -1 && defaultIndex + 1 < parts.size()) {
        defaultValue = parts.at(defaultIndex + 1);
        currentValue = defaultValue;
    }

    if (type == OptionTypeSpin) {
        int minIndex = static_cast<int>(parts.indexOf("min"));
        int maxIndex = static_cast<int>(parts.indexOf("max"));
        if (minIndex != -1 && parts.size() > minIndex) min = parts.at(minIndex + 1);
        if (maxIndex != -1 && parts.size() > maxIndex) max = parts.at(maxIndex + 1);
    } else if (type == OptionTypeCombo) {
        int varIndex = 0;
        while ((varIndex = static_cast<int>(parts.indexOf("var", varIndex + 1))) != -1) {
            if (parts.size() > varIndex) valueList.append(parts.at(varIndex + 1));
        }
    }

    // 既存のオプションと重複しないことを確認
    auto it = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [&name](const EngineOption& option) {
        return option.name == name;
    });

    if (it == m_engineOptions.end()) {
        EngineOption option{name, type, defaultValue, min, max, currentValue, valueList};
        m_engineOptions.append(option);
    } else {
        setError(tr("重複したエンジンオプションが見つかりました。"));
    }
}

// エンジンオプション構造体に値を設定する。
void EngineRegistrationHandler::addOption(const QString& name, const QString& type, const QString& defaultValue, const QString& min, const QString& max, const QString& currentValue)
{
    EngineOption option{name, type, defaultValue, min, max, currentValue, QStringList()};
    m_engineOptions.append(option);
}

// comboタイプのオプションの文字列を作成する。
void EngineRegistrationHandler::concatenateComboOptionValues()
{
    m_concatenatedOptionValuesList.clear();

    for (const auto& option : std::as_const(m_engineOptions)) {
        m_concatenatedOptionValuesList.append(option.valueList.join(" "));
    }
}

// --- 設定I/O ---

// 設定ファイルからエンジン情報を読み込む。
Engine EngineRegistrationHandler::readEngineFromSettings(const QSettings& settings) const
{
    Engine engine;
    engine.name = settings.value(EngineNameKey).toString();
    engine.path = settings.value(EnginePathKey).toString();
    engine.author = settings.value(EngineAuthorKey).toString();
    return engine;
}

// 設定ファイルにエンジン名と実行ファイルパスを書き込む。
void EngineRegistrationHandler::saveEnginesToSettingsFile() const
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginWriteArray(EnginesGroupName);

    for (qsizetype i = 0; i < m_engineList.size(); i++) {
        settings.setArrayIndex(static_cast<int>(i));
        saveEngineToSettings(settings, m_engineList.at(i));
    }

    settings.endArray();
}

// 指定されたエンジン情報を設定ファイルに保存する。
void EngineRegistrationHandler::saveEngineToSettings(QSettings& settings, const Engine& engine) const
{
    settings.setValue(EngineNameKey, engine.name);
    settings.setValue(EnginePathKey, engine.path);
    settings.setValue(EngineAuthorKey, engine.author);
}

// 設定ファイルに追加エンジンのオプションを書き込む。
void EngineRegistrationHandler::saveEngineOptionsToSettings() const
{
    // 並列配列のサイズ整合性を確認
    if (m_engineOptions.size() != m_concatenatedOptionValuesList.size()) {
        qCWarning(lcUi) << "saveEngineOptionsToSettings: リストサイズ不一致 - "
                   << "m_engineOptions:" << m_engineOptions.size()
                   << "m_concatenatedOptionValuesList:" << m_concatenatedOptionValuesList.size();
        return;
    }

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginWriteArray(m_engineIdName);

    for (qsizetype i = 0; i < m_engineOptions.size(); i++) {
        settings.setArrayIndex(static_cast<int>(i));
        settings.setValue(EngineOptionNameKey, m_engineOptions.at(i).name);
        settings.setValue(EngineOptionTypeKey, m_engineOptions.at(i).type);
        settings.setValue(EngineOptionDefaultKey, m_engineOptions.at(i).defaultValue);
        settings.setValue(EngineOptionMinKey, m_engineOptions.at(i).min);
        settings.setValue(EngineOptionMaxKey, m_engineOptions.at(i).max);
        settings.setValue(EngineOptionValueKey, m_engineOptions.at(i).currentValue);
        settings.setValue(EngineOptionValueListKey, m_concatenatedOptionValuesList.at(i));
    }

    settings.endArray();
}

// 設定ファイルから[Engines]グループを削除する。
void EngineRegistrationHandler::removeEnginesGroup() const
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(EnginesGroupName);
    settings.remove("");
    settings.endGroup();
}

// 設定ファイルからエンジン名グループを削除する。
void EngineRegistrationHandler::removeEngineNameGroup(const QString& removeEngineName) const
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(removeEngineName);
    settings.remove("");
    settings.endGroup();
}

// エラー設定＋シグナル送出
void EngineRegistrationHandler::setError(const QString& message)
{
    m_errorOccurred = true;
    emit errorOccurred(message);
}
