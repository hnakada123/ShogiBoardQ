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
 * 2つの探索を別々のワーカーで同時に動かす:
 * - 手数つきの探索（shogi::TsumeSearch）: 残りの手数の中で詰むか、最長に抵抗する応手はどれかを求める。
 *   結論が出ればそれを使う
 * - 証明数探索（TsumeProofNumberSearch）: 何手かけても詰まないことを確かめる。長手数の問題で正解以外の手を
 *   指した後は、手数つきの探索では判定時間に収まらないので、こちらの「詰まない」を使う
 *
 * 結果は shogi::TsumeResult で知らせる（証明数探索の「詰まない」は NoMate と、詰みを防ぐ応手）。
 * 止めた探索は終わるのを待たずに捨てる（ワーカーは値だけを持つ）。
 * 証明数探索は専用のスレッドで動かす（共有のスレッドプールが埋まっていても、手数つきの探索と同時に進む）。
 */
class TsumeDefenseSearch : public QObject
{
    Q_OBJECT
public:
    /// 「詰まない」と分かってから、手数つきの探索の結論を待つ時間（開始からのミリ秒）。
    /// すぐ終わる短い問題では、手数つきの探索の応手（最長の抵抗）をそのまま使う
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
    shogi::TsumeResult disproof() const;

    QFutureWatcher<shogi::TsumeResult>* m_exactWatcher = nullptr;
    QFutureWatcher<TsumeProofNumberSearch::Result>* m_proofWatcher = nullptr;
    /// 動いている2つの探索の中断フラグ（探索していなければ空）
    CancelFlag m_stop;
    /// 手数つきの探索が結論を出せずに終わったときの結果
    std::optional<shogi::TsumeResult> m_exactResult;
    std::optional<TsumeProofNumberSearch::Result> m_proofResult;
    QElapsedTimer m_elapsed;
    QTimer m_graceTimer;
    /// 証明数探索のスレッド（1本。壊すときは、止めるよう知らせた探索が戻るのを待つ）
    QThreadPool m_proofPool;
};

#endif // TSUMEDEFENSESEARCH_H
