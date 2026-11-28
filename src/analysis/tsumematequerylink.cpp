/// @file tsumematequerylink.cpp
/// @brief 逆算生成が詰将棋エンジンへ問い合わせる窓口の実装

#include "tsumematequerylink.h"

#include "logcategories.h"
#include "tsumemateengine.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"

#include <algorithm>
#include <optional>

namespace {
constexpr int kOracleInitTimeoutMs = 30000;
constexpr int kMaxConsecutiveRecoveries = 5;
} // namespace

TsumeMateQueryLink::TsumeMateQueryLink(QObject* parent)
    : QObject(parent)
{
}

TsumeMateQueryLink::~TsumeMateQueryLink()
{
    shutdown();
}

void TsumeMateQueryLink::start(const Options& options)
{
    m_options = options;
    m_hashDirty = false;
    m_consecutiveRecoveries = 0;
    m_lastQuerySfen.clear();
    launch();
}

void TsumeMateQueryLink::launch()
{
    shutdown();
    m_ready = false;
    m_awaitingAnswer = false;
    m_oracle = new MateOracle(this);
    m_oracle->session()->setInitializationTimeoutMs(kOracleInitTimeoutMs);
    // KomoringHeights 等の最短手数確認に必要なセッションオプションを自動適用
    auto sessionOptions = TsumeMateEngine::requiredOptions();
    if (m_options.hashMb > 0) {
        sessionOptions.append({QStringLiteral("USI_Hash"), QString::number(m_options.hashMb)});
    }
    if (m_options.threads > 0) {
        sessionOptions.append({QStringLiteral("Threads"), QString::number(m_options.threads)});
    }
    m_oracle->session()->handler()->setSessionOptions(sessionOptions);
    connect(m_oracle, &MateOracle::ready, this, &TsumeMateQueryLink::onOracleReady);
    connect(m_oracle, &MateOracle::answered, this, &TsumeMateQueryLink::onOracleAnswered);
    connect(m_oracle, &MateOracle::timedOut, this, &TsumeMateQueryLink::onOracleTimedOut);
    connect(m_oracle, &MateOracle::errorOccurred, this, &TsumeMateQueryLink::errorOccurred);
    m_oracle->start(m_options.enginePath, m_options.engineName);
}

void TsumeMateQueryLink::shutdown()
{
    if (!m_oracle) return;
    m_oracle->quit();
    disconnect(m_oracle, nullptr, this, nullptr);
    m_oracle->deleteLater();
    m_oracle = nullptr;
    m_ready = false;
    m_awaitingAnswer = false;
}

bool TsumeMateQueryLink::restartForRecovery(int restartCount)
{
    ++m_consecutiveRecoveries;
    if (m_consecutiveRecoveries > kMaxConsecutiveRecoveries) return false;
    qCInfo(lcAnalysis).noquote() << "Restarting mate engine after timeout (count=" << restartCount << ")";
    launch();
    return true;
}

void TsumeMateQueryLink::onOracleReady()
{
    m_ready = true;
    m_consecutiveRecoveries = 0;
    emit ready();
}

TsumeMateQueryLink::AskResult TsumeMateQueryLink::ask(const QString& sfen, int limitMs, bool strictInfinite)
{
    const std::optional<UsiPosition> pos = UsiPosition::parse(sfen);
    if (!pos || !m_oracle || !m_ready) return AskResult::Unavailable;

    m_lastQuerySfen = sfen;
    m_lastQueryWasStrict = strictInfinite;
    if (!strictInfinite) m_hashDirty = true;
    m_awaitingAnswer = true;
    emit aboutToAsk();
    m_queryTimer.start();

    const bool ok = strictInfinite
        ? m_oracle->askWithin(*pos, std::max(1, limitMs))
        : m_oracle->ask(*pos, std::max(1, limitMs));
    if (!ok) {
        m_awaitingAnswer = false;
        return AskResult::Refused;
    }
    return AskResult::Sent;
}

void TsumeMateQueryLink::resetHashIfDirty()
{
    if (!m_hashDirty || !m_oracle || !m_oracle->session() || !m_oracle->session()->handler()) return;
    m_oracle->session()->handler()->sendUsiNewGame();
    m_hashDirty = false;
}

void TsumeMateQueryLink::onOracleAnswered(const MateAnswer& answer)
{
    if (!m_awaitingAnswer) return;
    m_awaitingAnswer = false;
    m_consecutiveRecoveries = 0;
    const qint64 elapsedMs = m_queryTimer.elapsed();

    if (answer.kind == MateAnswer::Kind::Unsupported) {
        emit unsupported();
        return;
    }
    emit answered(answer, elapsedMs);
}

void TsumeMateQueryLink::onOracleTimedOut()
{
    m_awaitingAnswer = false;
    emit timedOut();
}
