#ifndef TSUMEGAMESESSION_H
#define TSUMEGAMESESSION_H

#include <QObject>
#include <QStringList>
#include <memory>
#include <optional>
#include "tsume.h"
#include "tsumeevaluation.h"
#include <QElapsedTimer>
#include <QTimer>

class TsumeDefenseSearch;
class TsumePositionAnalyzer;
class TsumeProgressStore;

/// Hayanagiで応手を生成し、内蔵探索（TsumeDefenseSearch）または外部の詰み判定器で対局を進める。
/// 開始局面の判定で最善の手順（同梱の検証済みの問題集の正解手順・判定器が返した手順）が分かっていれば、
/// その手順の上を指している間は探索せずに手順の応手を指す（長手数の問題は、正解の手でも1手ごとの探索が
/// 判定時間に収まらない）。手順を外れた手だけを探索で判定する。
/// 挑戦と正答の記録（TsumeProgressStore）もここで付ける: 開始してから初めて攻め方の手番になったときに挑戦を1回、
/// 詰ませたときに正答を1回（どちらも start() ごとに1回だけ）。
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
    /// 外部の詰み判定器と、解析の結果・挑戦の記録を残す先（どちらも省略可）
    void configureEngine(const QString& path, TsumeProgressStore* store = nullptr);
    QString detail() const { return m_detail; }
    State state() const { return m_state; }
    int remainingPlies() const { return m_remaining; }
    bool canUndo() const { return !m_history.empty(); }
    QString sfen() const;
    bool attackerIsBlack() const { return m_attacker == shogi::Color::Black; }

signals:
    void stateChanged();
    void positionChanged(const QString& sfen, const QString& move);
    void finished(TsumeGameSession::Outcome outcome, int remainingPlies);
    void moveRejected();

private slots:
    void searchFinished(const shogi::TsumeResult& result);
    void evaluationFinished(const TsumeEvaluation& result);
    void evaluateNextDefense();
    void playKnownReply();

private:
    void search();
    void searchDefense();
    void stopWorker();
    void apply(const shogi::Move& move);
    void setState(State state);
    void beginDefenseEvaluation();
    /// 指した手が分かっている手順の上にあれば、その手順の玉方の次の応手
    std::optional<shogi::Move> knownReply() const;
    struct Snapshot { shogi::Position position; int remaining; qsizetype moves; };
    shogi::Position m_position;
    shogi::Color m_attacker = shogi::Color::Black;
    State m_state = State::Empty;
    int m_remaining = 0;
    int m_timeLimit = 5000;
    bool m_initializing = false;
    TsumePositionAnalyzer* m_analyzer = nullptr;
    QString m_enginePath;
    TsumeProgressStore* m_store = nullptr;
    QString m_problemId;          ///< 記録に使う開始局面の識別子
    bool m_attemptRecorded = false;
    bool m_solvedRecorded = false;
    QString m_detail;
    std::vector<shogi::Move> m_defenses;
    std::size_t m_defenseIndex = 0;
    shogi::Move m_bestDefense;
    int m_longest = -1;
    bool m_unresolved = false;
    QElapsedTimer m_turnElapsed;
    QTimer m_defenseTimer;
    /// 開始局面から分かっている最善の手順（攻め方は最短の手、玉方は最長の応手。詰みまで）。分からなければ空
    QStringList m_knownLine;
    /// 開始局面から盤に指した手（攻め方と玉方）
    QStringList m_moves;
    /// 分かっている手順の応手を、着手の処理（play）から戻った後に指すためのタイマー
    QTimer m_knownReplyTimer;
    std::vector<Snapshot> m_history;
    /// 内蔵の判定（攻め方が指した後の局面の、応手と詰みまでの手数）
    TsumeDefenseSearch* m_defenseSearch = nullptr;
};

#endif
