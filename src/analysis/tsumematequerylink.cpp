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
/// 答えのない期限切れがこの回数続いたら、再起動の後にエンジンが生きているかを確かめる
constexpr int kProbeAfterTimeouts = 5;
/// 生きているかを確かめる問い合わせの制限時間。答えが無ければ MateOracle が余裕の後に stop を送り、
/// それにも答えなければ応答しないとみなす（合わせて約10秒）
constexpr int kProbeLimitMs = 2000;
/// 生きているかを確かめる局面（1手詰。どの詰将棋エンジンでもすぐに答えが出る）
constexpr char kProbeSfen[] = "8k/6G2/7G1/9/9/9/9/9/9 b R 1";
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
    m_unansweredTimeouts = 0;
    m_lastQuerySfen.clear();
    launch(false);
}

void TsumeMateQueryLink::launch(bool probeFirst)
{
    shutdown();
    m_probeOnReady = probeFirst;
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
    m_probing = false;
    m_probeOnReady = false;
}

void TsumeMateQueryLink::restartForRecovery(int restartCount)
{
    // 期限切れは、厳密証明の探索が期限までに終わらなかっただけのこともある（エンジンは正常）。
    // 答えが1つも返らないまま続いたときだけ、起動し直した後に生きているかを確かめる
    ++m_unansweredTimeouts;
    qCInfo(lcAnalysis).noquote() << "Restarting mate engine after timeout (count=" << restartCount << ")";
    launch(m_unansweredTimeouts >= kProbeAfterTimeouts);
}

void TsumeMateQueryLink::onOracleReady()
{
    if (m_probeOnReady) {
        m_probeOnReady = false;
        sendProbe();
        return;
    }
    m_ready = true;
    emit ready();
}

void TsumeMateQueryLink::sendProbe()
{
    // 使う側にはまだ ready を知らせない（確かめ終わるまで問い合わせを受け付けない）
    const std::optional<UsiPosition> pos = UsiPosition::parse(QString::fromLatin1(kProbeSfen));
    m_probing = pos && m_oracle && m_oracle->ask(*pos, kProbeLimitMs);
    if (!m_probing) emit unresponsive();
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
    if (m_probing) {
        m_probing = false;
        if (answer.kind == MateAnswer::Kind::Unsupported) {
            emit unsupported();
            return;
        }
        // 答えが返った（中身は問わない）: エンジンは生きている。数え直して問い合わせを受け付ける。
        // 確かめに使った探索の結果は、後の厳密な問い合わせに持ち越さない
        m_unansweredTimeouts = 0;
        m_hashDirty = true;
        resetHashIfDirty();
        m_ready = true;
        emit ready();
        return;
    }

    if (!m_awaitingAnswer) return;
    m_awaitingAnswer = false;
    m_unansweredTimeouts = 0;
    const qint64 elapsedMs = m_queryTimer.elapsed();

    if (answer.kind == MateAnswer::Kind::Unsupported) {
        emit unsupported();
        return;
    }
    emit answered(answer, elapsedMs);
}

void TsumeMateQueryLink::onOracleTimedOut()
{
    if (m_probing) {
        // 1手詰の問い合わせにも stop にも答えない
        m_probing = false;
        emit unresponsive();
        return;
    }
    m_awaitingAnswer = false;
    emit timedOut();
}
