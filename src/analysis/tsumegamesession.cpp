#include "tsumegamesession.h"
#include "tsumedefensesearch.h"
#include "tsumepositionanalyzer.h"
#include "tsumecollection.h"
#include "tsumeprogressstore.h"
#include <algorithm>

TsumeGameSession::TsumeGameSession(QObject* parent) : QObject(parent)
{
    m_analyzer = new TsumePositionAnalyzer(this);
    connect(m_analyzer, &TsumePositionAnalyzer::finished, this, &TsumeGameSession::evaluationFinished);
    m_defenseSearch = new TsumeDefenseSearch(this);
    connect(m_defenseSearch, &TsumeDefenseSearch::finished, this, &TsumeGameSession::searchFinished);
    m_defenseTimer.setSingleShot(true);
    connect(&m_defenseTimer, &QTimer::timeout, this, &TsumeGameSession::evaluateNextDefense);
    m_knownReplyTimer.setSingleShot(true);
    connect(&m_knownReplyTimer, &QTimer::timeout, this, &TsumeGameSession::playKnownReply);
}
TsumeGameSession::~TsumeGameSession() { stopWorker(); }

void TsumeGameSession::stopWorker()
{
    m_defenseTimer.stop();
    m_knownReplyTimer.stop();
    m_analyzer->cancel();
    m_defenseSearch->cancel();
}

void TsumeGameSession::configureEngine(const QString& path, TsumeProgressStore* store)
{
    stopWorker();
    m_enginePath = path;
    m_store = store;
    m_analyzer->configure(path, store);
}

void TsumeGameSession::setState(State state)
{
    m_state = state;
    // 記録は知らせる前に付ける（受け手が履歴を読み直すため）
    if (state == State::Ready && !m_attemptRecorded) {
        m_attemptRecorded = true;
        if (m_store) m_store->recordAttempt(m_problemId);
    } else if (state == State::Solved && !m_solvedRecorded) {
        m_solvedRecorded = true;
        if (m_store) m_store->recordSolved(m_problemId);
    }
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
    m_problemId = TsumeCollection::positionId(sfenText);
    m_attemptRecorded = false;
    m_solvedRecorded = false;
    m_initializing = true;
    m_history.clear();
    m_knownLine.clear();
    m_moves.clear();
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

void TsumeGameSession::apply(const shogi::Move& move)
{
    const QString usi = QString::fromStdString(m_position.move_to_usi(move));
    m_position.do_move(move);
    m_moves.append(usi);
    emit positionChanged(sfen(), usi);
}

bool TsumeGameSession::play(const QString& text)
{
    if (m_state != State::Ready) return false;
    for (const auto& move : m_position.generate_legal_moves()) {
        if (m_position.move_to_usi(move) != text.toStdString()) continue;
        // 詰将棋では攻め方は必ず王手。打ち歩詰め等は合法手生成で除外済み。
        if (!m_position.gives_check(move)) break;
        m_history.push_back({m_position, m_remaining, m_moves.size()});
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
    if (knownReply()) {
        // 応手が分かっている間は探索しない。盤は着手の処理の後で直前の手を示すので、応手は play() から戻った後に指す
        m_knownReplyTimer.start(0);
        return;
    }
    searchDefense();
}

std::optional<shogi::Move> TsumeGameSession::knownReply() const
{
    // 玉方の手番で、ここまでに指した手がすべて手順と同じで、手順に続きがあるときだけ
    const qsizetype played = m_moves.size();
    if (m_position.side_to_move() == m_attacker || played >= m_knownLine.size()
        || !std::equal(m_moves.cbegin(), m_moves.cend(), m_knownLine.cbegin())) return std::nullopt;
    const std::string reply = m_knownLine.at(played).toStdString();
    for (const auto& move : m_position.generate_legal_moves()) {
        if (m_position.move_to_usi(move) == reply) return move;
    }
    return std::nullopt;
}

void TsumeGameSession::playKnownReply()
{
    if (m_state != State::Thinking || m_initializing) return;
    const auto reply = knownReply();
    if (!reply) {
        searchDefense();
        return;
    }
    apply(*reply);
    m_remaining = static_cast<int>(m_knownLine.size() - m_moves.size());
    setState(State::Ready);
}

void TsumeGameSession::searchDefense()
{
    if (!m_enginePath.isEmpty()) {
        beginDefenseEvaluation();
        return;
    }
    m_defenseSearch->start(m_position, m_attacker, m_remaining, m_timeLimit);
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
            // 詰みまでの手順が付いていれば覚える（同梱の検証済みの正解手順か、判定器が返した最短の手順）
            if (result.plies > 0 && result.pv.size() == result.plies) m_knownLine = result.pv;
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

void TsumeGameSession::searchFinished(const shogi::TsumeResult& result)
{
    if (m_state != State::Thinking || m_initializing) return;
    using Status = shogi::TsumeStatus;
    if (result.status == Status::Timeout || result.status == Status::Cancelled) {
        if (result.status == Status::Timeout) {
            m_detail = tr("内蔵判定の制限時間に達しました。判定時間を増やすか、問題一覧でKomoringHeightsを選択して再判定できます。");
        }
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
    m_moves.resize(m_history.back().moves);
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
