/// @file tsumesolutionsearch.cpp
/// @brief 詰将棋の問題の内蔵の判定（開始局面の、最短の詰み手数と詰みまでの手順）

#include "tsumesolutionsearch.h"

#include "tsume.h"
#include "tsumeproofnumbersearch.h"
#include "tsumethreadbudget.h"

#include <QtConcurrentRun>

#include <algorithm>
#include <chrono>

namespace {

using Outcome = TsumeSolutionSearch::Outcome;

Outcome concluded(TsumeEvaluation::Status status, int plies = 0)
{
    return {TsumeEvaluation{status, plies, {}, {}}, true};
}

/// 反復深化の探索で判定する。深さを延ばしながら調べるので、最初に見つかった詰みが最短の手数
Outcome searchIteratively(shogi::Position position, int maxPlies, int milliseconds, bool requireLine,
                          const std::atomic_bool& stop)
{
    QElapsedTimer elapsed;
    elapsed.start();
    const TsumeThreadBudget budget;
    shogi::TsumeSearch solver;
    const auto attacker = position.side_to_move();
    auto search = solver.solve(position, attacker, maxPlies, milliseconds, stop, budget.threads());
    if (search.status == shogi::TsumeStatus::NoMate) return concluded(TsumeEvaluation::Status::NoMate);
    if (search.status == shogi::TsumeStatus::Limit) return concluded(TsumeEvaluation::Status::Unknown);
    if (search.status != shogi::TsumeStatus::Mate) return {};
    Outcome result = concluded(TsumeEvaluation::Status::Mate, search.plies);
    if (!requireLine) return result;
    // 探索は最善手1手を返すので、攻方の最短手・玉方の最長応手を順に取得する。
    // 各局面で同じ時間を使い直さず、手順全体で制限時間と中止フラグを共有する。
    for (int remaining = search.plies; remaining > 0; --remaining) {
        if (stop.load() || search.status != shogi::TsumeStatus::Mate
            || search.plies != remaining || !search.move.is_valid()) return {};
        result.evaluation.pv.append(QString::fromStdString(position.move_to_usi(search.move)));
        position.do_move(search.move);
        if (remaining > 1) {
            const qint64 timeLeft = milliseconds - elapsed.elapsed();
            if (timeLeft <= 0) return {};
            search = solver.solve(position, attacker, remaining - 1, static_cast<int>(timeLeft), stop, budget.threads());
        }
    }
    return result;
}

/// 証明数探索で判定する。最短の手数だと確かめられた詰みだけを結論にする
Outcome searchByProofNumbers(shogi::Position position, int maxPlies, int milliseconds, bool requireLine,
                             const std::atomic_bool& stop)
{
    TsumeProofNumberSearch solver;
    const auto attacker = position.side_to_move();
    const auto search = requireLine ? solver.solveLine(position, attacker, maxPlies, milliseconds, stop)
                                    : solver.solve(position, attacker, maxPlies, milliseconds, stop);
    using Status = TsumeProofNumberSearch::Status;
    if (search.status == Status::NoMate) return concluded(TsumeEvaluation::Status::NoMate);
    if (search.status == Status::TooLong) return concluded(TsumeEvaluation::Status::Unknown);
    if (search.status != Status::Mate || !search.exact) return {};
    Outcome result = concluded(TsumeEvaluation::Status::Mate, search.plies);
    if (!requireLine) return result;
    if (static_cast<int>(search.line.size()) != search.plies) return {};
    for (const shogi::Move& move : search.line) {
        result.evaluation.pv.append(QString::fromStdString(position.move_to_usi(move)));
        position.do_move(move);
    }
    return result;
}

/// 止めた探索の結果を捨てる（終わったら自分で消える）
void discard(QFutureWatcher<Outcome>*& watcher, QObject* receiver)
{
    if (!watcher) return;
    QObject::disconnect(watcher, nullptr, receiver, nullptr);
    QObject::connect(watcher, &QFutureWatcherBase::finished, watcher, &QObject::deleteLater);
    watcher = nullptr;
}

} // namespace

TsumeSolutionSearch::TsumeSolutionSearch(QObject* parent) : QObject(parent)
{
    m_proofPool.setMaxThreadCount(1);
    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, &TsumeSolutionSearch::graceExpired);
}

TsumeSolutionSearch::~TsumeSolutionSearch()
{
    cancel();
}

void TsumeSolutionSearch::cancel()
{
    m_graceTimer.stop();
    if (m_stop) m_stop->store(true);
    m_stop.reset();
    discard(m_exactWatcher, this);
    discard(m_proofWatcher, this);
    m_exactResult.reset();
    m_proofResult.reset();
}

void TsumeSolutionSearch::start(const shogi::Position& position, int maxPlies, int timeLimitMs, bool requireLine)
{
    cancel();
    m_stop = makeCancelFlag();
    m_requireLine = requireLine;
    m_elapsed.start();
    const CancelFlag stop = m_stop;

    m_exactWatcher = new QFutureWatcher<Outcome>(this);
    connect(m_exactWatcher, &QFutureWatcherBase::finished, this, &TsumeSolutionSearch::exactSearchFinished);
    m_exactWatcher->setFuture(QtConcurrent::run([position, maxPlies, timeLimitMs, requireLine, stop]() {
        return searchIteratively(position, maxPlies, timeLimitMs, requireLine, *stop);
    }));

    // 証明数探索の期限は、反復深化の探索と同じ時刻にする（止めた前の探索が戻るのを待って遅く始まっても延ばさない）
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeLimitMs);
    m_proofWatcher = new QFutureWatcher<Outcome>(this);
    connect(m_proofWatcher, &QFutureWatcherBase::finished, this, &TsumeSolutionSearch::proofSearchFinished);
    m_proofWatcher->setFuture(QtConcurrent::run(&m_proofPool, [position, maxPlies, timeLimitMs, requireLine, deadline, stop]() {
        int limit = 0;      // 0 は時間で打ち切らない
        if (timeLimitMs > 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) return Outcome{};
            limit = static_cast<int>(std::min<std::chrono::milliseconds::rep>(left, timeLimitMs));
        }
        return searchByProofNumbers(position, maxPlies, limit, requireLine, *stop);
    }));
}

void TsumeSolutionSearch::exactSearchFinished()
{
    QFutureWatcher<Outcome>* watcher = m_exactWatcher;
    if (!watcher || sender() != watcher) return;
    m_exactWatcher = nullptr;
    m_exactResult = watcher->result();
    watcher->deleteLater();
    decide();
}

void TsumeSolutionSearch::proofSearchFinished()
{
    QFutureWatcher<Outcome>* watcher = m_proofWatcher;
    if (!watcher || sender() != watcher) return;
    m_proofWatcher = nullptr;
    m_proofResult = watcher->result();
    watcher->deleteLater();
    decide();
}

void TsumeSolutionSearch::graceExpired()
{
    decide();
}

void TsumeSolutionSearch::decide()
{
    if (m_exactResult && m_exactResult->concluded) {
        finish(m_exactResult->evaluation);
        return;
    }
    if (m_proofResult && m_proofResult->concluded) {
        // 手順を求めるときは、まだ動いている反復深化の探索の結論を少しだけ待つ
        const bool lineFound = m_requireLine && m_proofResult->evaluation.status == TsumeEvaluation::Status::Mate;
        const qint64 wait = lineFound && !m_exactResult ? kExactSearchGraceMs - m_elapsed.elapsed() : 0;
        if (wait > 0) m_graceTimer.start(static_cast<int>(wait));
        else finish(m_proofResult->evaluation);
        return;
    }
    // どちらも結論を出せずに終わった
    if (m_exactResult && m_proofResult) finish({});
}

void TsumeSolutionSearch::finish(const TsumeEvaluation& result)
{
    // 残っている探索を止めてから知らせる（受け手が次の探索を始められるように）。
    // cancel() は覚えた結果も消すので、知らせる結果は先に写しておく
    const TsumeEvaluation delivered = result;
    cancel();
    emit finished(delivered);
}
