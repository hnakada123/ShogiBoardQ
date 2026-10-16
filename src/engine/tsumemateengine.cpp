/// @file tsumemateengine.cpp
/// @brief 最短手数を確認できる詰み探索エンジンで詰みを判定するクラスの実装

#include "tsumemateengine.h"
#include "enginelistsettings.h"
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
    QString error;
    if (!m_oracle->start(m_executable, EngineListSettings::nameForPath(m_executable), &error)) {
        fail(tr("判定エンジンを実行できません: %1").arg(error.isEmpty() ? m_executable : error));
    }
}

void TsumeMateEngine::onReady()
{
    if (m_phase != Phase::Starting) return;
    const UsiProtocolHandler* handler = m_oracle->session()->handler();
    const auto postSearchLevel = handler->reportedOption(QStringLiteral("PostSearchLevel"));
    const bool supported =
        handler->reportedEngineName().contains(QStringLiteral("KomoringHeights"), Qt::CaseInsensitive)
        && postSearchLevel && postSearchLevel->comboValues.contains(QStringLiteral("MinLength"))
        && handler->reportedOption(QStringLiteral("GenerateAllLegalMoves"))
        && handler->reportedOption(QStringLiteral("RootIsAndNodeIfChecked"));
    if (!supported) {
        fail(tr("最短手数を確認できるKomoringHeightsを選択してください。"));
        return;
    }
    m_phase = Phase::Idle;
    beginSearch();
}

void TsumeMateEngine::beginSearch()
{
    m_phase = Phase::Searching;
    m_oracle->session()->handler()->sendUsiNewGame();
    // Komoring 1.1.0 は時間制限で最短化を打ち切っても既発見の PV を返す場合がある。
    // go mate infinite で探させ、制限時間までに自然に終わった MinLength の結果だけを確定扱いにする
    // （時間切れは stop を送らずにエンジンを終わらせる）
    m_oracle->askWithin(UsiPosition::parse(m_sfen).value_or(UsiPosition{}), m_milliseconds);
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

void TsumeMateEngine::onSessionError(const QString&)
{
    if (m_phase == Phase::Off) return;
    fail(tr("判定エンジンが終了しました。実行ファイルを確認してください。"));
}

void TsumeMateEngine::timedOut()
{
    fail(tr("制限時間内に判定が完了しませんでした。不詰とは判定していません。"));
}
