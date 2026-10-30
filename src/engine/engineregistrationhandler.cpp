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
#include <QFileInfo>

using namespace EngineSettingsConstants;

namespace {
/// 起動から usiok までの待ち時間（オプションの多いエンジンは報告に時間がかかる）
constexpr int kUsiOkTimeoutMs = 30000;
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
    m_engines = RegisteredEngines::load();
    emit enginesChanged();
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
    m_fileName = RegisteredEngines::canonicalPath(filePath);
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
    // 起動できなかったときは、戻った後に onProbeError で届く
    m_probe->probeAsync(QFileInfo(filePath).absoluteFilePath());
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
    m_engineIdName = m_engines.uniqueName(engineName);
    m_engineIdAuthor = engineAuthor;
    m_optionLines = optionLines;

    // オプションを読めたエンジンだけを一覧に足す（読めなければ一覧も設定も変えない）
    parseEngineOptionsFromUsiOutput();
    if (m_errorOccurred) {
        setRegistrationInProgress(false);
        return;
    }

    EngineEntry engine;
    engine.name = m_engineIdName;
    engine.path = m_fileName;
    engine.author = m_engineIdAuthor;
    m_engines.append(engine);

    EngineProfileStore::saveEngines(m_engines.entries());
    EngineProfileStore::saveOptions(m_engineIdName, m_engineOptions);

    setRegistrationInProgress(false);
    emit enginesChanged();
    emit engineRegistered(m_engineIdName);
}

// 指定インデックスのエンジンを削除し設定を更新する。
void EngineRegistrationHandler::removeEngineAt(int index)
{
    if (index < 0 || index >= m_engines.entries().size()) {
        qCWarning(lcUi) << "removeEngineAt: invalid index" << index;
        return;
    }

    const QString removeEngineName = m_engines.entries().at(index).name;
    m_engines.removeAt(index);

    EngineProfileStore::saveEngines(m_engines.entries());
    EngineProfileStore::removeEngine(removeEngineName);
    emit enginesChanged();
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
    const std::optional<EngineOption> parsed = UsiLineParser::parseOption(line, &parseError);
    if (!parsed) {
        qCWarning(lcUi) << "parseOptionLine: invalid option line:" << parseError << line;
        setError(tr("オプション行の形式が無効です。"));
        return;
    }

    // 既存のオプションと重複しないことを確認
    const QString& name = parsed->name;
    auto it = std::find_if(m_engineOptions.begin(), m_engineOptions.end(), [&name](const EngineOption& option) {
        return option.name == name;
    });

    if (it == m_engineOptions.end()) {
        m_engineOptions.append(*parsed);   // 値は既定値から始める
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
