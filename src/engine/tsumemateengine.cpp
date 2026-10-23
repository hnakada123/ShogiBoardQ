/// @file tsumemateengine.cpp
/// @brief 最短手数を確認できる詰み探索エンジンで詰みを判定するクラスの実装

#include "tsumemateengine.h"
#include "registeredengines.h"
#include "mateoracle.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"
#include <algorithm>

namespace {
/// 起動から readyok まで。大きなハッシュの確保に時間がかかるエンジンがある
constexpr int kInitializationTimeoutMs = 15000;
} // namespace

TsumeMateEngine::TsumeMateEngine(QObject* parent)
    : QObject(parent)
    , m_oracle(new MateOracle(this))
{
    m_oracle->session()->setInitializationTimeoutMs(kInitializationTimeoutMs);
    connect(m_oracle, &MateOracle::ready, this, &TsumeMateEngine::onReady);
    connect(m_oracle, &MateOracle::errorOccurred, this, &TsumeMateEngine::onSessionError);
    connect(m_oracle, &MateOracle::timedOut, this, &TsumeMateEngine::timedOut);
    connect(m_oracle, &MateOracle::answered, this, &TsumeMateEngine::onAnswered);
}

TsumeMateEngine::~TsumeMateEngine() { cancel(); }

QList<QPair<QString, QString>> TsumeMateEngine::requiredOptions()
{
    return {{QStringLiteral("PostSearchLevel"), QStringLiteral("MinLength")},
            {QStringLiteral("GenerateAllLegalMoves"), QStringLiteral("true")},
            {QStringLiteral("RootIsAndNodeIfChecked"), QStringLiteral("false")},
            {QStringLiteral("NodesLimit"), QStringLiteral("0")},
            {QStringLiteral("MultiPV"), QStringLiteral("1")}};
}

bool TsumeMateEngine::canVerifyShortestMate(
    const std::function<std::optional<EngineOption>(const QString&)>& lookup)
{
    const std::optional<EngineOption> postSearchLevel = lookup(QStringLiteral("PostSearchLevel"));
    return postSearchLevel && postSearchLevel->valueList.contains(QStringLiteral("MinLength"))
        && lookup(QStringLiteral("GenerateAllLegalMoves")) && lookup(QStringLiteral("RootIsAndNodeIfChecked"));
}

bool TsumeMateEngine::canVerifyShortestMate(const QList<EngineOption>& options)
{
    return canVerifyShortestMate([&options](const QString& name) -> std::optional<EngineOption> {
        for (const EngineOption& option : options)
            if (option.name == name) return option;
        return std::nullopt;
    });
}

void TsumeMateEngine::setExecutable(const QString& path)
{
    if (path == m_executable) return;
    cancel();
    m_executable = path;
}

void TsumeMateEngine::cancel()
{
    m_phase = Phase::Off;
    m_oracle->quit();
}

void TsumeMateEngine::search(const QString& sfen, int milliseconds)
{
    m_sfen = sfen;
    m_milliseconds = std::clamp(milliseconds, 1, 600000);
    if (m_phase == Phase::Idle) {
        beginSearch();
        return;
    }
    cancel();
    m_phase = Phase::Starting;
    // 登録済みなら保存済みの設定を送る（名前で設定を引く）。最短手数の確認に必要な値だけ上書きする
    m_oracle->session()->handler()->setSessionOptions(requiredOptions());
    // 起動できなかったときは、戻った後に onSessionError で届く
    const std::optional<EngineEntry> registered = RegisteredEngines::load().findByPath(m_executable);
    m_oracle->start(m_executable, registered ? registered->name : QString());
}

void TsumeMateEngine::onReady()
{
    if (m_phase != Phase::Starting) return;
    const UsiProtocolHandler* handler = m_oracle->session()->handler();
    if (!canVerifyShortestMate([handler](const QString& name) { return handler->reportedOption(name); })) {
        fail(tr("最短手数を確認できるKomoringHeightsを選択してください。"));
        return;
    }
    m_phase = Phase::Idle;
    beginSearch();
}

void TsumeMateEngine::beginSearch()
{
    const std::optional<UsiPosition> position = UsiPosition::parse(m_sfen);
    if (!position) {
        // 読めない局面はエンジンに送らない
        finishSearch({TsumeEvaluation::Status::Unknown, 0, {}, tr("詰みを調べる局面を読めません。")});
        return;
    }
    m_phase = Phase::Searching;
    m_oracle->session()->handler()->sendUsiNewGame();
    // Komoring 1.1.0 は時間制限で最短化を打ち切っても既発見の PV を返す場合がある。
    // go mate infinite で探させ、制限時間までに自然に終わった MinLength の結果だけを確定扱いにする
    // （時間切れは stop を送らずにエンジンを終わらせる）
    m_oracle->askWithin(*position, m_milliseconds);
}

void TsumeMateEngine::finishSearch(const TsumeEvaluation& result)
{
    m_phase = Phase::Idle;
    emit finished(result);
}

void TsumeMateEngine::onAnswered(const MateAnswer& answer)
{
    if (m_phase != Phase::Searching) return;
    TsumeEvaluation result;
    switch (answer.kind) {
    case MateAnswer::Kind::Mate:
        result.status = TsumeEvaluation::Status::Mate;
        result.pv = answer.pv;
        result.plies = static_cast<int>(answer.pv.size());
        break;
    case MateAnswer::Kind::NoMate:
        result.status = TsumeEvaluation::Status::NoMate;
        break;
    case MateAnswer::Kind::Unsupported:
    case MateAnswer::Kind::Unknown:
        result.detail = tr("エンジンが判定を完了できませんでした。時間を増やして再判定できます。");
        break;
    }
    finishSearch(result);
}

void TsumeMateEngine::fail(const QString& message)
{
    cancel();
    emit finished({TsumeEvaluation::Status::Unknown, 0, {}, message});
}

void TsumeMateEngine::onSessionError(const QString& message)
{
    if (m_phase == Phase::Off) return;
    // 初期化が終わる前の失敗は、起動できなかったとして理由を出す
    if (m_phase == Phase::Starting) {
        fail(tr("判定エンジンを実行できません: %1").arg(message.isEmpty() ? m_executable : message));
        return;
    }
    fail(tr("判定エンジンが終了しました。実行ファイルを確認してください。"));
}

void TsumeMateEngine::timedOut()
{
    fail(tr("制限時間内に判定が完了しませんでした。不詰とは判定していません。"));
}
