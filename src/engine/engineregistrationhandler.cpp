/// @file engineregistrationhandler.cpp
/// @brief エンジン登録ハンドラクラスの実装（usi〜usiok の問い合わせ・オプションの解析・設定の保存）

#include "engineregistrationhandler.h"
#include "engineprocessmanager.h"
#include "enginesettingsconstants.h"
#include "usienginesession.h"
#include "usilineparser.h"
#include "usiprotocolhandler.h"
#include "engineprofilestore.h"
#include "logcategories.h"
#include <QFile>
#include <QFileInfo>

using namespace EngineSettingsConstants;

namespace {
/// 起動から usiok までの待ち時間（オプションの多いエンジンは報告に時間がかかる）
constexpr int kUsiOkTimeoutMs = 30000;

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
    , m_probe(new UsiEngineSession(this))
{
    m_probe->setInitializationTimeoutMs(kUsiOkTimeoutMs);
    connect(m_probe->process(), &EngineProcessManager::processStarted, this, &EngineRegistrationHandler::onProbeStarted);
    connect(m_probe, &UsiEngineSession::ready, this, &EngineRegistrationHandler::onProbeReady);
    connect(m_probe, &UsiEngineSession::errorOccurred, this, &EngineRegistrationHandler::onProbeError);
}

EngineRegistrationHandler::~EngineRegistrationHandler()
{
    // 応答しないエンジンも、終了の監視へ引き渡して待たずに戻る
    m_probe->quit();
}

// 設定ファイルからエンジンリストを読み込む。
void EngineRegistrationHandler::loadEnginesFromSettings()
{
    m_engineList.clear();
    m_engineList = EngineProfileStore::loadEngines();
}

const QList<EngineListSettings::EngineEntry>& EngineRegistrationHandler::engineList() const
{
    return m_engineList;
}

const EngineListSettings::EngineEntry& EngineRegistrationHandler::engineAt(int index) const
{
    return m_engineList.at(index);
}

// パスの重複を検査する。
bool EngineRegistrationHandler::isDuplicatePath(const QString& path, QString& existingName) const
{
    const QString normalizedInput = normalizeEnginePath(path);
    for (const EngineListSettings::EngineEntry& engine : std::as_const(m_engineList)) {
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

void EngineRegistrationHandler::setRegistrationInProgress(bool inProgress)
{
    if (m_registrationInProgress == inProgress) return;
    m_registrationInProgress = inProgress;
    emit registrationInProgressChanged(inProgress);
}

void EngineRegistrationHandler::startRegistration(const QString& filePath)
{
    if (m_registrationInProgress) return;

    m_errorOccurred = false;
    m_fileName = normalizeEnginePath(filePath);
    m_engineIdName.clear();
    m_engineIdAuthor.clear();
    m_optionLines.clear();
    m_processStarted = false;

    if (!QFileInfo(filePath).isFile()) {
        setError(tr("エンジンファイルが見つかりません: %1").arg(filePath));
        return;
    }
    setRegistrationInProgress(true);
    emit progressUpdated(tr("エンジンを起動中..."));
    if (!m_probe->probeAsync(QFileInfo(filePath).absoluteFilePath())) {
        setRegistrationInProgress(false);
        setError(tr("エンジンの起動に失敗しました: %1").arg(filePath));
    }
}

void EngineRegistrationHandler::cancelRegistration()
{
    if (!m_registrationInProgress) return;
    m_probe->quit();
    setRegistrationInProgress(false);
}

bool EngineRegistrationHandler::isRegistrationInProgress() const
{
    return m_registrationInProgress;
}

void EngineRegistrationHandler::onProbeStarted()
{
    m_processStarted = true;
    emit progressUpdated(tr("USI通信中..."));
}

void EngineRegistrationHandler::onProbeReady()
{
    const UsiProtocolHandler* handler = m_probe->handler();
    const QString engineName = handler->reportedEngineName();
    const QString engineAuthor = handler->reportedEngineAuthor();
    const QStringList optionLines = handler->reportedOptionLines();
    m_probe->quit();
    if (engineName.trimmed().isEmpty()) {
        setRegistrationInProgress(false);
        setError(tr("エンジン名(id name)を取得できませんでした"));
        return;
    }
    registerEngine(engineName, engineAuthor, optionLines);
}

void EngineRegistrationHandler::onProbeError(const QString& message)
{
    if (!m_registrationInProgress) return;
    qCWarning(lcUi) << "engine registration failed:" << message;
    setRegistrationInProgress(false);
    setError(m_processStarted ? tr("エンジンからの応答がタイムアウトしました")
                              : tr("エンジンの起動に失敗しました: %1").arg(m_fileName));
}

void EngineRegistrationHandler::registerEngine(const QString& engineName, const QString& engineAuthor,
                                               const QStringList& optionLines)
{
    // 同じ実行ファイルは登録前に断っている。別の実行ファイルが同じ id name を名乗る
    // （評価関数だけ違うビルドなど）ときは、登録名に番号を付けて別のエンジンとして登録する。
    // 登録名は設定（オプション・先読み）を保存する名前にもなるので、エンジンごとに設定が分かれる
    m_engineIdName = uniqueRegisteredName(engineName);
    m_engineIdAuthor = engineAuthor;
    m_optionLines = optionLines;

    // エンジン情報をリストに追加
    EngineListSettings::EngineEntry engine;
    engine.name = m_engineIdName;
    engine.path = m_fileName;
    engine.author = m_engineIdAuthor;
    m_engineList.append(engine);

    parseEngineOptionsFromUsiOutput();
    if (m_errorOccurred) {
        setRegistrationInProgress(false);
        return;
    }

    EngineProfileStore::saveEngines(m_engineList);
    EngineProfileStore::saveOptions(m_engineIdName, m_engineOptions);

    setRegistrationInProgress(false);
    emit engineRegistered(m_engineIdName);
}

QString EngineRegistrationHandler::uniqueRegisteredName(const QString& idName) const
{
    const auto taken = [this](const QString& name) {
        for (const EngineListSettings::EngineEntry& engine : std::as_const(m_engineList)) {
            if (engine.name == name) return true;
        }
        return false;
    };
    if (!taken(idName)) return idName;
    for (int number = 2;; ++number) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(idName).arg(number);
        if (!taken(candidate)) return candidate;
    }
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

    EngineProfileStore::saveEngines(m_engineList);
    EngineProfileStore::removeEngine(removeEngineName);
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

    // USI_Ponderは自動追加しない。先読み許可と未報告オプションへの通知方針は
    // EngineProfileStore の先読みの設定で管理し、エンジン設定画面の共通項目で変更する。
}

// 将棋エンジンから送信されたオプション行を解析し、エンジンオプションリストに追加する。
void EngineRegistrationHandler::parseOptionLine(const QString& line)
{
    QString parseError;
    const std::optional<UsiOptionDeclaration> declaration = UsiLineParser::parseOption(line, &parseError);
    if (!declaration) {
        qCWarning(lcUi) << "parseOptionLine: invalid option line:" << parseError << line;
        setError(tr("オプション行の形式が無効です。"));
        return;
    }
    const UsiOptionDeclaration& parsed = *declaration;

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

// エラー設定＋シグナル送出
void EngineRegistrationHandler::setError(const QString& message)
{
    m_errorOccurred = true;
    emit errorOccurred(message);
}
