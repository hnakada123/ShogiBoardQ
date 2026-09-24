#include "tsumegamesession.h"
#include "tsumepositionanalyzer.h"
#include <QtConcurrentRun>
#include <algorithm>

TsumeGameSession::TsumeGameSession(QObject* parent) : QObject(parent)
{
    m_analyzer = new TsumePositionAnalyzer(this);
    connect(m_analyzer, &TsumePositionAnalyzer::finished, this, &TsumeGameSession::evaluationFinished);
    m_defenseTimer.setSingleShot(true);
    connect(&m_defenseTimer, &QTimer::timeout, this, &TsumeGameSession::evaluateNextDefense);
}
TsumeGameSession::~TsumeGameSession() { stopWorker(); }

void TsumeGameSession::stopWorker()
{
    m_defenseTimer.stop();
    m_analyzer->cancel();
    if (!m_watcher) return;
    disconnect(m_watcher.get(), nullptr, this, nullptr);
    m_stop->store(true);
    m_watcher->waitForFinished();
    m_watcher.reset();
}

void TsumeGameSession::configureEngine(const QString& path, TsumeProgressStore* store)
{
    stopWorker();
    m_enginePath = path;
    m_analyzer->configure(path, store);
}

void TsumeGameSession::setState(State state)
{
    m_state = state;
    emit stateChanged();
}

void TsumeGameSession::setTimeLimit(int milliseconds)
{
    m_timeLimit = std::clamp(milliseconds, 100, 600000);
}

bool TsumeGameSession::start(const QString& sfenText)
{
    shogi::Position position;
    if (!position.set_sfen(sfenText.toStdString(), true) ||
        position.find_king(shogi::opposite(position.side_to_move())) < 0 ||
        position.is_in_check(shogi::opposite(position.side_to_move()))) return false;
    stopWorker();
    m_position = position;
    m_attacker = position.side_to_move();
    m_initializing = true;
    m_history.clear();
    m_remaining = 0;
    m_detail.clear();
    emit positionChanged(sfen(), {});
    search();
    return true;
}

QString TsumeGameSession::sfen() const
{
    return QString::fromStdString(m_position.to_sfen());
}

QStringList TsumeGameSession::legalMoves() const
{
    QStringList result;
    if (m_state != State::Ready) return result;
    for (const auto& move : m_position.generate_legal_moves())
        result.append(QString::fromStdString(m_position.move_to_usi(move)));
    return result;
}

void TsumeGameSession::apply(const shogi::Move& move)
{
    const QString usi = QString::fromStdString(m_position.move_to_usi(move));
    m_position.do_move(move);
    emit positionChanged(sfen(), usi);
}

bool TsumeGameSession::play(const QString& text)
{
    if (m_state != State::Ready) return false;
    for (const auto& move : m_position.generate_legal_moves()) {
        if (m_position.move_to_usi(move) != text.toStdString()) continue;
        // 詰将棋では攻め方は必ず王手。打ち歩詰め等は合法手生成で除外済み。
        if (!m_position.gives_check(move)) break;
        m_history.push_back({m_position, m_remaining});
        --m_remaining;
        setState(State::Thinking);
        apply(move);
        search();
        return true;
    }
    emit moveRejected();
    return false;
}

void TsumeGameSession::search()
{
    stopWorker();
    m_detail.clear();
    setState(State::Thinking);
    if (m_initializing) {
        m_analyzer->evaluate(sfen(), m_timeLimit);
        return;
    }
    if (!m_enginePath.isEmpty()) {
        beginDefenseEvaluation();
        return;
    }
    m_stop = std::make_shared<std::atomic_bool>(false);
    m_watcher = std::make_unique<QFutureWatcher<shogi::TsumeResult>>(this);
    connect(m_watcher.get(), &QFutureWatcher<shogi::TsumeResult>::finished,
            this, &TsumeGameSession::searchFinished);
    const auto position = m_position;
    const auto attacker = m_attacker;
    const auto stop = m_stop;
    const int depth = m_remaining;
    const int millis = m_timeLimit;
    m_watcher->setFuture(QtConcurrent::run([position, attacker, depth, millis, stop]() {
        shogi::TsumeSearch solver;
        return solver.solve(position, attacker, depth, millis, *stop);
    }));
}

void TsumeGameSession::beginDefenseEvaluation()
{
    // 応手生成・適用には Hayanagi を使用。外部判定には各応手後の攻方手番だけを送る。
    m_defenses = m_position.generate_legal_moves();
    if (m_defenses.empty()) {
        m_remaining = 0;
        setState(State::Solved);
        emit finished(Outcome::Solved, 0);
        return;
    }
    if (m_remaining <= 0) {
        apply(m_defenses.front());
        setState(State::Failed);
        emit finished(Outcome::TooLong, 0);
        return;
    }
    m_defenseIndex = 0;
    m_longest = -1;
    m_bestDefense = {};
    m_unresolved = false;
    m_turnElapsed.start();
    evaluateNextDefense();
}

void TsumeGameSession::evaluateNextDefense()
{
    if (m_state != State::Thinking || m_initializing) return;
    if (m_defenseIndex == m_defenses.size()) {
        if (m_unresolved || !m_bestDefense.is_valid()) {
            setState(State::Paused);
            emit finished(Outcome::Inconclusive, m_remaining);
        } else {
            apply(m_bestDefense);
            m_remaining = m_longest;
            setState(State::Ready);
        }
        return;
    }
    const qint64 remainingTime = m_timeLimit - m_turnElapsed.elapsed();
    if (remainingTime <= 0) {
        m_detail = tr("すべての応手を時間内に確認できませんでした。時間を増やして再判定できます。");
        setState(State::Paused);
        emit finished(Outcome::Inconclusive, m_remaining);
        return;
    }
    auto next = m_position;
    next.do_move(m_defenses[m_defenseIndex]);
    m_analyzer->evaluate(QString::fromStdString(next.to_sfen()), static_cast<int>(remainingTime));
}

void TsumeGameSession::evaluationFinished(const TsumeEvaluation& result)
{
    if (m_state != State::Thinking) return;
    m_detail = result.detail;
    using Status = TsumeEvaluation::Status;
    if (m_initializing) {
        if (result.status == Status::Mate) {
            m_remaining = result.plies;
            m_initializing = false;
            setState(State::Ready);
        } else if (result.status == Status::NoMate) {
            setState(State::Failed);
            emit finished(Outcome::InvalidProblem, 0);
        } else {
            setState(State::Paused);
            emit finished(Outcome::Inconclusive, 0);
        }
        return;
    }
    if (m_defenseIndex >= m_defenses.size()) return;
    const auto move = m_defenses[m_defenseIndex];
    if (result.status == Status::NoMate || (result.status == Status::Mate && result.plies > m_remaining - 1)) {
        apply(move);
        setState(State::Failed);
        emit finished(result.status == Status::NoMate ? Outcome::NoMate : Outcome::TooLong,
                      std::max(0, m_remaining - 1));
        return;
    }
    if (result.status == Status::Mate && result.plies > m_longest) {
        m_bestDefense = move;
        m_longest = result.plies;
    }
    m_unresolved |= result.status == Status::Unknown;
    ++m_defenseIndex;
    m_defenseTimer.start(0);
}

void TsumeGameSession::searchFinished()
{
    if (sender() != m_watcher.get()) return;
    const auto result = m_watcher->result();
    m_watcher.release()->deleteLater();
    using Status = shogi::TsumeStatus;
    if (result.status == Status::Timeout || result.status == Status::Cancelled) {
        setState(State::Paused);
        emit finished(Outcome::Inconclusive, m_remaining);
        return;
    }
    if (result.status == Status::Mate && result.plies == 0) {
        m_remaining = 0;
        setState(State::Solved);
        emit finished(Outcome::Solved, 0);
        return;
    }
    auto reply = result.move;
    if (!reply.is_valid()) {
        // 最終手でまだ応手がある場合（残り0手）、盤上に逃れを示す。
        const auto legal = m_position.generate_legal_moves();
        if (!legal.empty()) reply = legal.front();
    }
    if (reply.is_valid()) apply(reply);
    if (result.status == Status::Mate) {
        m_remaining = result.plies - 1;
        setState(State::Ready);
    } else {
        setState(State::Failed);
        emit finished(result.status == Status::NoMate ? Outcome::NoMate : Outcome::TooLong,
                      std::max(0, m_remaining - 1));
    }
}

void TsumeGameSession::undo()
{
    if (m_history.empty()) return;
    stopWorker();
    m_position = m_history.back().position;
    m_remaining = m_history.back().remaining;
    m_history.pop_back();
    m_initializing = false;
    emit positionChanged(sfen(), {});
    setState(State::Ready);
}

void TsumeGameSession::retry()
{
    if (m_state == State::Paused) search();
}

void TsumeGameSession::cancel()
{
    if (m_state != State::Thinking) return;
    stopWorker();
    setState(State::Paused);
}
