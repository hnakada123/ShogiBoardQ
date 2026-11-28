/// @file tsumedefensesearch.cpp
/// @brief 詰将棋対局の内蔵の判定（攻め方が指した後の局面の、玉方の応手と詰みまでの手数）

#include "tsumedefensesearch.h"

#include "tsumethreadbudget.h"

#include <QtConcurrentRun>

#include <algorithm>
#include <chrono>

namespace {

/// 止めた探索の結果を捨てる（終わったら自分で消える）
template <typename Watcher>
void discard(Watcher*& watcher, QObject* receiver)
{
    if (!watcher) return;
    QObject::disconnect(watcher, nullptr, receiver, nullptr);
    QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
    watcher = nullptr;
}

} // namespace

TsumeDefenseSearch::TsumeDefenseSearch(QObject* parent) : QObject(parent)
{
    m_proofPool.setMaxThreadCount(1);
    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, &TsumeDefenseSearch::graceExpired);
}

TsumeDefenseSearch::~TsumeDefenseSearch()
{
    cancel();
}

void TsumeDefenseSearch::cancel()
{
    m_graceTimer.stop();
    if (m_stop) m_stop->store(true);
    m_stop.reset();
    discard(m_exactWatcher, this);
    discard(m_proofWatcher, this);
    m_exactResult.reset();
    m_proofResult.reset();
}

void TsumeDefenseSearch::start(const shogi::Position& position, shogi::Color attacker, int remainingPlies,
                               int timeLimitMs)
{
    cancel();
    m_stop = makeCancelFlag();
    m_elapsed.start();
    const CancelFlag stop = m_stop;

    m_exactWatcher = new QFutureWatcher<shogi::TsumeResult>(this);
    connect(m_exactWatcher, &QFutureWatcherBase::finished, this, &TsumeDefenseSearch::exactSearchFinished);
    m_exactWatcher->setFuture(QtConcurrent::run([position, attacker, remainingPlies, timeLimitMs, stop]() {
        const TsumeThreadBudget budget;
        shogi::TsumeSearch solver;
        return solver.solve(position, attacker, remainingPlies, timeLimitMs, *stop, budget.threads());
    }));

    // 証明数探索の期限は、手数つきの探索と同じ時刻にする（止めた前の探索が戻るのを待って遅く始まっても延ばさない）
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeLimitMs);
    m_proofWatcher = new QFutureWatcher<TsumeProofNumberSearch::Result>(this);
    connect(m_proofWatcher, &QFutureWatcherBase::finished, this, &TsumeDefenseSearch::proofSearchFinished);
    m_proofWatcher->setFuture(QtConcurrent::run(&m_proofPool, [position, attacker, timeLimitMs, deadline, stop]() {
        int limit = 0;      // 0 は時間で打ち切らない
        if (timeLimitMs > 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) return TsumeProofNumberSearch::Result{};
            limit = static_cast<int>(std::min<std::chrono::milliseconds::rep>(left, timeLimitMs));
        }
        TsumeProofNumberSearch search;
        return search.solve(position, attacker, limit, *stop);
    }));
}

void TsumeDefenseSearch::exactSearchFinished()
{
    QFutureWatcher<shogi::TsumeResult>* watcher = m_exactWatcher;
    if (!watcher || sender() != watcher) return;
    m_exactWatcher = nullptr;
    const shogi::TsumeResult result = watcher->result();
    watcher->deleteLater();

    using Status = shogi::TsumeStatus;
    if (result.status == Status::Mate || result.status == Status::NoMate || result.status == Status::Limit) {
        finish(result);
        return;
    }
    // 結論が出なかった（時間切れ）。証明数探索の結果で決める
    m_exactResult = result;
    decide();
}

void TsumeDefenseSearch::proofSearchFinished()
{
    QFutureWatcher<TsumeProofNumberSearch::Result>* watcher = m_proofWatcher;
    if (!watcher || sender() != watcher) return;
    m_proofWatcher = nullptr;
    m_proofResult = watcher->result();
    watcher->deleteLater();
    decide();
}

void TsumeDefenseSearch::graceExpired()
{
    decide();
}

void TsumeDefenseSearch::decide()
{
    const bool disproven = m_proofResult && m_proofResult->status == TsumeProofNumberSearch::Status::NoMate;
    if (m_exactResult) {
        // 手数つきの探索は時間切れ。証明数探索も同じ時刻に止まるので、まだなら終わるのを待つ
        if (disproven) finish(disproof());
        else if (m_proofResult) finish(*m_exactResult);
        return;
    }
    // 手数つきの探索は動いている。「詰まない」と分かっていなければ、その結論を待つ
    if (!disproven) return;
    const qint64 wait = kExactSearchGraceMs - m_elapsed.elapsed();
    if (wait > 0) {
        m_graceTimer.start(static_cast<int>(wait));
        return;
    }
    finish(disproof());
}

shogi::TsumeResult TsumeDefenseSearch::disproof() const
{
    shogi::TsumeResult result;
    result.status = shogi::TsumeStatus::NoMate;
    result.move = m_proofResult->refutation;
    result.nodes = m_proofResult->nodes;
    return result;
}

void TsumeDefenseSearch::finish(shogi::TsumeResult result)
{
    // 残っている探索を止めてから知らせる（受け手が次の探索を始められるように）。
    // cancel() は覚えた結果も消すので、知らせる結果は値で受ける
    cancel();
    emit finished(result);
}
