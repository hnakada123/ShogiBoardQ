#ifndef TSUMEDEFENSESEARCH_H
#define TSUMEDEFENSESEARCH_H

/// @file tsumedefensesearch.h
/// @brief 詰将棋対局の内蔵の判定（攻め方が指した後の局面の、玉方の応手と詰みまでの手数）

#include "threadtypes.h"
#include "tsume.h"
#include "tsumeproofnumbersearch.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>
#include <QTimer>

#include <optional>

/**
 * @brief 攻め方が指した後の局面を内蔵の探索で判定する
 *
 * 残りの手数の中で詰むか、玉方がいちばん長く粘る応手はどれかを、2つの探索を別々のワーカーで同時に動かして求める:
 * - 反復深化の探索（shogi::TsumeSearch）: 深さを延ばしながら全部の手を調べる。短い問題ではすぐ終わる
 * - 証明数探索（TsumeProofNumberSearch）: 結論に近い手から調べる。長手数の問題では、反復深化の探索が
 *   判定時間に収まらない局面（正解の手・別解・残りの手数より長い詰み・詰まない手）も判定できる
 *
 * 反復深化の探索が結論を出せばそれを使い、証明数探索の結論が先に出たときは、少しだけ待ってからそれを使う
 * （kExactSearchGraceMs）。結果は shogi::TsumeResult で知らせる（詰みは Mate と手数と応手、残りの手数では
 * 詰まないときは Limit と応手、何手かけても詰まないときは NoMate と応手）。
 * 止めた探索は終わるのを待たずに捨てる（ワーカーは値だけを持つ）。
 * 証明数探索は専用のスレッドで動かす（共有のスレッドプールが埋まっていても、反復深化の探索と同時に進む）。
 */
class TsumeDefenseSearch : public QObject
{
    Q_OBJECT
public:
    /// 証明数探索の結論が出てから、反復深化の探索の結論を待つ時間（開始からのミリ秒）。
    /// すぐ終わる短い問題では、反復深化の探索の結論（手数と応手）をそのまま使う
    static constexpr int kExactSearchGraceMs = 500;

    explicit TsumeDefenseSearch(QObject* parent = nullptr);
    ~TsumeDefenseSearch() override;

    /// position（玉方の手番）から remainingPlies 手以内に詰むかを調べ始める。動いている探索は捨てる
    void start(const shogi::Position& position, shogi::Color attacker, int remainingPlies, int timeLimitMs);
    /// 探索を止める（finished は出さない）
    void cancel();
    bool isRunning() const { return m_stop != nullptr; }

signals:
    /// 判定が終わった。Timeout はどちらの探索も判定時間内に結論を出せなかった
    void finished(const shogi::TsumeResult& result);

private slots:
    void exactSearchFinished();
    void proofSearchFinished();
    void graceExpired();

private:
    void decide();
    void finish(shogi::TsumeResult result);
    /// 証明数探索の結論（詰み・手数内に詰まない・詰まない）。結論が出ていなければ空
    std::optional<shogi::TsumeResult> proofVerdict() const;

    QFutureWatcher<shogi::TsumeResult>* m_exactWatcher = nullptr;
    QFutureWatcher<TsumeProofNumberSearch::Result>* m_proofWatcher = nullptr;
    /// 動いている2つの探索の中断フラグ（探索していなければ空）
    CancelFlag m_stop;
    /// 反復深化の探索が結論を出せずに終わったときの結果
    std::optional<shogi::TsumeResult> m_exactResult;
    std::optional<TsumeProofNumberSearch::Result> m_proofResult;
    QElapsedTimer m_elapsed;
    QTimer m_graceTimer;
    /// 証明数探索のスレッド（1本。壊すときは、止めるよう知らせた探索が戻るのを待つ）
    QThreadPool m_proofPool;
};

#endif // TSUMEDEFENSESEARCH_H
