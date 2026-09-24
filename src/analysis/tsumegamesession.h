#ifndef TSUMEGAMESESSION_H
#define TSUMEGAMESESSION_H

#include <QObject>
#include <QFutureWatcher>
#include <QStringList>
#include <memory>
#include "tsume.h"
#include "tsumeevaluation.h"
#include <QElapsedTimer>
#include <QTimer>

class TsumePositionAnalyzer;
class TsumeProgressStore;

/// Hayanagiで応手を生成し、内蔵探索または外部の詰み判定器で対局を進める。
class TsumeGameSession : public QObject
{
    Q_OBJECT
public:
    enum class State { Empty, Thinking, Ready, Paused, Solved, Failed };
    Q_ENUM(State)
    enum class Outcome { Solved, NoMate, TooLong, Inconclusive, InvalidProblem };
    Q_ENUM(Outcome)

    explicit TsumeGameSession(QObject* parent = nullptr);
    ~TsumeGameSession() override;
    bool start(const QString& sfen);
    bool play(const QString& move);
    void undo();
    void retry();
    void cancel();
    void setTimeLimit(int milliseconds);
    void configureEngine(const QString& path, TsumeProgressStore* store = nullptr);
    QString detail() const { return m_detail; }
    State state() const { return m_state; }
    int remainingPlies() const { return m_remaining; }
    bool canUndo() const { return !m_history.empty(); }
    QString sfen() const;
    QStringList legalMoves() const;
    bool attackerIsBlack() const { return m_attacker == shogi::Color::Black; }

signals:
    void stateChanged();
    void positionChanged(const QString& sfen, const QString& move);
    void finished(TsumeGameSession::Outcome outcome, int remainingPlies);
    void moveRejected();

private slots:
    void searchFinished();
    void evaluationFinished(const TsumeEvaluation& result);
    void evaluateNextDefense();

private:
    void search();
    void stopWorker();
    void apply(const shogi::Move& move);
    void setState(State state);
    void beginDefenseEvaluation();
    struct Snapshot { shogi::Position position; int remaining; };
    shogi::Position m_position;
    shogi::Color m_attacker = shogi::Color::Black;
    State m_state = State::Empty;
    int m_remaining = 0;
    int m_timeLimit = 5000;
    bool m_initializing = false;
    TsumePositionAnalyzer* m_analyzer = nullptr;
    QString m_enginePath;
    QString m_detail;
    std::vector<shogi::Move> m_defenses;
    std::size_t m_defenseIndex = 0;
    shogi::Move m_bestDefense;
    int m_longest = -1;
    bool m_unresolved = false;
    QElapsedTimer m_turnElapsed;
    QTimer m_defenseTimer;
    std::vector<Snapshot> m_history;
    std::unique_ptr<QFutureWatcher<shogi::TsumeResult>> m_watcher;
    std::shared_ptr<std::atomic_bool> m_stop;
};

#endif
