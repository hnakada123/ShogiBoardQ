#ifndef TSUMESOLUTIONSEARCH_H
#define TSUMESOLUTIONSEARCH_H

/// @file tsumesolutionsearch.h
/// @brief 詰将棋の問題の内蔵の判定（開始局面の、最短の詰み手数と詰みまでの手順）

#include "position.h"
#include "threadtypes.h"
#include "tsumeevaluation.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>
#include <QTimer>

#include <optional>

/**
 * @brief 問題の開始局面（攻め方の手番）を内蔵の探索で判定する
 *
 * 最短の詰み手数（攻め方が最も短く詰ませ、玉方が最も長く粘ったとき）と、頼まれれば詰みまでの手順を、
 * 2つの探索を別々のワーカーで同時に動かして求める:
 * - 反復深化の探索（shogi::TsumeSearch）: 深さを延ばしながら全部の手を調べる。短い問題ではすぐ終わる
 * - 証明数探索（TsumeProofNumberSearch）: 詰みを見つけてから、2手短い手数では詰まないことを確かめる。
 *   長手数の問題は、反復深化の探索が判定時間に収まらなくても判定できる
 *
 * どちらの結論（詰みと最短の手数・不詰・手数の上限では詰まない）も同じになるので、先に出たほうを使う。
 * 手順を求めるときだけ、証明数探索の詰みの結論が先に出ても、反復深化の探索の結論を少しだけ待つ
 * （kExactSearchGraceMs。最短の手順がいくつもある短い問題で、手順がいつも同じになるようにする）。
 * 結果は TsumeEvaluation で知らせる（Mate と手数と手順、NoMate。手数の上限では詰まないときと、どちらの探索も
 * 判定時間内に結論を出せなかったときは Unknown）。
 * 止めた探索は終わるのを待たずに捨てる（ワーカーは値だけを持つ）。
 * 証明数探索は専用のスレッドで動かす（共有のスレッドプールが埋まっていても、反復深化の探索と同時に進む）。
 */
class TsumeSolutionSearch : public QObject
{
    Q_OBJECT
public:
    /// 手順を求めるとき、証明数探索の結論が出てから、反復深化の探索の結論を待つ時間（開始からのミリ秒）
    static constexpr int kExactSearchGraceMs = 500;

    explicit TsumeSolutionSearch(QObject* parent = nullptr);
    ~TsumeSolutionSearch() override;

    /// position（攻め方の手番）が maxPlies 手以内に詰むかを調べ始める。requireLine なら詰みまでの手順も求める
    /// （判定時間は手順を含めた全体の時間）。動いている探索は捨てる
    void start(const shogi::Position& position, int maxPlies, int timeLimitMs, bool requireLine);
    /// 探索を止める（finished は出さない）
    void cancel();
    bool isRunning() const { return m_stop != nullptr; }

    /// 1つの探索の結果
    struct Outcome {
        TsumeEvaluation evaluation;
        /// 結論が出た（詰みと最短の手数・不詰・手数の上限では詰まない）。false は時間切れなどで分からなかった
        bool concluded = false;
    };

signals:
    void finished(const TsumeEvaluation& result);

private slots:
    void exactSearchFinished();
    void proofSearchFinished();
    void graceExpired();

private:
    void decide();
    void finish(const TsumeEvaluation& result);

    QFutureWatcher<Outcome>* m_exactWatcher = nullptr;
    QFutureWatcher<Outcome>* m_proofWatcher = nullptr;
    /// 動いている2つの探索の中断フラグ（探索していなければ空）
    CancelFlag m_stop;
    /// 終わった探索の結果（結論を出せなかったものも入れる）
    std::optional<Outcome> m_exactResult;
    std::optional<Outcome> m_proofResult;
    bool m_requireLine = false;
    QElapsedTimer m_elapsed;
    QTimer m_graceTimer;
    /// 証明数探索のスレッド（1本。壊すときは、止めるよう知らせた探索が戻るのを待つ）
    QThreadPool m_proofPool;
};

#endif // TSUMESOLUTIONSEARCH_H
