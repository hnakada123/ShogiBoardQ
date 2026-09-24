#include "tsumegamesession.h"
#include <QtConcurrentRun>
#include <algorithm>

TsumeGameSession::TsumeGameSession(QObject* parent) : QObject(parent) {}
TsumeGameSession::~TsumeGameSession() { stopWorker(); }

void TsumeGameSession::stopWorker()
{
    if (!m_watcher) return;
    disconnect(m_watcher.get(), nullptr, this, nullptr);
    m_stop->store(true);
    m_watcher->waitForFinished();
    m_watcher.reset();
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
    setState(State::Thinking);
    m_stop = std::make_shared<std::atomic_bool>(false);
    m_watcher = std::make_unique<QFutureWatcher<shogi::TsumeResult>>(this);
    connect(m_watcher.get(), &QFutureWatcher<shogi::TsumeResult>::finished,
            this, &TsumeGameSession::searchFinished);
    const auto position = m_position;
    const auto attacker = m_attacker;
    const auto stop = m_stop;
    const int depth = m_initializing ? 31 : m_remaining;
    const int millis = m_timeLimit;
    m_watcher->setFuture(QtConcurrent::run([position, attacker, depth, millis, stop]() {
        shogi::TsumeSearch solver;
        return solver.solve(position, attacker, depth, millis, *stop);
    }));
}

void TsumeGameSession::searchFinished()
{
    if (sender() != m_watcher.get()) return;
    const auto result = m_watcher->result();
    m_watcher.release()->deleteLater();
    using Status = shogi::TsumeStatus;
    if (result.status == Status::Timeout || result.status == Status::Cancelled ||
        (m_initializing && result.status == Status::Limit)) {
        setState(State::Paused);
        emit finished(Outcome::Inconclusive, m_remaining);
        return;
    }
    if (m_initializing) {
        if (result.status == Status::Mate) {
            m_remaining = result.plies;
            m_initializing = false;
            setState(State::Ready);
        } else {
            setState(State::Failed);
            emit finished(Outcome::InvalidProblem, 0);
        }
        return;
    }
    if (result.status == Status::Mate && result.plies == 0) {
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
