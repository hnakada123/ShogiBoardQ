/// @file engineregistrationhandler.cpp
/// @brief エンジン登録ハンドラクラスの実装（ワーカースレッド管理・USI解析・設定永続化）

#include "engineregistrationhandler.h"
#include "engineregistrationdialog.h"
#include "engineregistrationworker.h"
#include "enginesettingsconstants.h"
#include "usioptionlineparser.h"
#include "settingscommon.h"
#include "logcategories.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

using namespace EngineSettingsConstants;

namespace {
constexpr int kWorkerQuitTimeoutMs = 3000;
constexpr int kWorkerTerminateTimeoutMs = 1000;

QString normalizeEnginePath(const QString& filePath)
{
    const QFileInfo info(filePath);
    const QString canonicalPath = info.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return canonicalPath;
    }
    return info.absoluteFilePath();
}
} // namespace

EngineRegistrationHandler::EngineRegistrationHandler(QObject *parent)
    : QObject(parent)
{
}

EngineRegistrationHandler::~EngineRegistrationHandler()
{
    cancelRegistration();

    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            if (!m_workerThread->wait(kWorkerQuitTimeoutMs)) {
                qCWarning(lcUi) << "Engine registration worker thread quit timed out. Forcing terminate.";
                m_workerThread->terminate();
                if (!m_workerThread->wait(kWorkerTerminateTimeoutMs)) {
                    qCWarning(lcUi) << "Engine registration worker thread is still running after terminate.";
                }
            }
        }
    }
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
    const QString normalizedInput = normalizeEnginePath(path);
    for (const Engine& engine : std::as_const(m_engineList)) {
        if (normalizeEnginePath(engine.path) == normalizedInput) {
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
    return fi.exists() && fi.isFile() && fi.isExecutable();
}

// --- ワーカースレッド管理 ---

void EngineRegistrationHandler::ensureWorkerThread()
{
    if (m_workerThread) return;

    m_workerThread = new QThread(this);
    m_worker = new EngineRegistrationWorker();
    m_worker->moveToThread(m_workerThread);

    connect(this, &EngineRegistrationHandler::requestRegistration,
            m_worker, &EngineRegistrationWorker::registerEngine,
            Qt::QueuedConnection);
    connect(m_worker, &EngineRegistrationWorker::engineRegistered,
            this, &EngineRegistrationHandler::onWorkerRegistered,
            Qt::QueuedConnection);
    connect(m_worker, &EngineRegistrationWorker::registrationFailed,
            this, &EngineRegistrationHandler::onWorkerFailed,
            Qt::QueuedConnection);
    connect(m_worker, &EngineRegistrationWorker::registrationCanceled,
            this, &EngineRegistrationHandler::onWorkerCanceled,
            Qt::QueuedConnection);
    connect(m_worker, &EngineRegistrationWorker::progressUpdated,
            this, &EngineRegistrationHandler::progressUpdated,
            Qt::QueuedConnection);
    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

void EngineRegistrationHandler::setRegistrationInProgress(bool inProgress)
{
    if (m_registrationInProgress == inProgress) return;
    m_registrationInProgress = inProgress;
    if (!inProgress) {
        m_cancelFlag.reset();
    }
    emit registrationInProgressChanged(inProgress);
}

// エンジン登録処理を開始する（ワーカースレッドで非同期実行）。
void EngineRegistrationHandler::startRegistration(const QString& filePath)
{
    if (m_registrationInProgress) return;

    m_errorOccurred = false;
    m_fileName = normalizeEnginePath(filePath);
    m_engineIdName.clear();
    m_engineIdAuthor.clear();
    m_optionLines.clear();

    ensureWorkerThread();

    m_cancelFlag = makeCancelFlag();
    m_worker->setCancelFlag(m_cancelFlag);

    setRegistrationInProgress(true);
    emit requestRegistration(filePath);
}

// 進行中の登録処理をキャンセルする。
void EngineRegistrationHandler::cancelRegistration()
{
    if (m_cancelFlag) {
        m_cancelFlag->store(true);
    }
}

bool EngineRegistrationHandler::isRegistrationInProgress() const
{
    return m_registrationInProgress;
}

// ワーカーからの登録成功通知を処理する。
void EngineRegistrationHandler::onWorkerRegistered(const QString& engineName,
                                                   const QString& engineAuthor,
                                                   const QStringList& optionLines)
{
    m_engineIdName = engineName;
    m_engineIdAuthor = engineAuthor;
    m_optionLines = optionLines;

    // 登録エンジンの重複チェック
    for (const Engine& engine : std::as_const(m_engineList)) {
        if (m_engineIdName == engine.name) {
            setRegistrationInProgress(false);
            setError(tr("この将棋エンジンは既に登録されています。先に登録済みのエンジンを削除してください。"));
            return;
        }
    }

    // エンジン情報をリストに追加
    Engine engine;
    engine.name = m_engineIdName;
    engine.path = m_fileName;
    engine.author = m_engineIdAuthor;
    m_engineList.append(engine);

    parseEngineOptionsFromUsiOutput();
    if (m_errorOccurred) {
        setRegistrationInProgress(false);
        return;
    }

    saveEnginesToSettingsFile();
    concatenateComboOptionValues();
    saveEngineOptionsToSettings();

    setRegistrationInProgress(false);
    emit engineRegistered(m_engineIdName);
}

// ワーカーからの登録失敗通知を処理する。
void EngineRegistrationHandler::onWorkerFailed(const QString& errorMessage)
{
    setRegistrationInProgress(false);
    setError(errorMessage);
}

void EngineRegistrationHandler::onWorkerCanceled()
{
    setRegistrationInProgress(false);
}

// 指定インデックスのエンジンを削除し設定を更新する。
void EngineRegistrationHandler::removeEngineAt(int index)
{
    if (index < 0 || index >= m_engineList.size()) {
        qCWarning(lcUi) << "removeEngineAt: invalid index" << index;
        return;
    }

    QString removeEngineName = m_engineList.at(index).name;
    m_engineList.removeAt(index);

    // [Engines]グループを削除して作り直す
    removeEnginesGroup();
    saveEnginesToSettingsFile();
    removeEngineNameGroup(removeEngineName);
}

// --- USIプロトコル解析 ---

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

// 将棋エンジンから送信されたオプション行を解析し、エンジンオプションリストに追加する。
void EngineRegistrationHandler::parseOptionLine(const QString& line)
{
    ParsedOptionLine parsed;
    QString parseError;
    if (!parseUsiOptionLine(line, parsed, &parseError)) {
        qCWarning(lcUi) << "parseOptionLine: invalid option line:" << parseError << line;
        setError(tr("オプション行の形式が無効です。"));
        return;
    }

    // 既存のオプションと重複しないことを確認
    auto it = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [&parsed](const EngineOption& option) {
        return option.name == parsed.name;
    });

    if (it == m_engineOptions.end()) {
        EngineOption option{parsed.name,
                            parsed.type,
                            parsed.defaultValue,
                            parsed.minValue,
                            parsed.maxValue,
                            parsed.defaultValue,
                            parsed.comboValues};
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
        QJsonArray values;
        for (const QString& value : option.valueList) {
            values.append(value);
        }
        const QString serialized = QString::fromUtf8(
            QJsonDocument(values).toJson(QJsonDocument::Compact));
        m_concatenatedOptionValuesList.append(serialized);
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
