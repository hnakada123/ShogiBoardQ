#ifndef TSUMEVERIFICATIONJOB_H
#define TSUMEVERIFICATIONJOB_H

/// @file tsumeverificationjob.h
/// @brief 1局面の唯一解の検査を、総予算つきの詰みの問い合わせとして進める

#include "matequeryloop.h"
#include "tsumeshogiverifier.h"

/**
 * @brief TsumeshogiVerifier の問い合わせを、総予算と1回の上限で区切って出す（引き出し型）
 *
 * 1回の問い合わせは kQueryMaxMs まで（難しい1問で総予算を使い切らず、ほかの攻手の余詰も調べる）。
 * 答えにかかった時間の合計が総予算を超えたら検査を打ち切り、結果は Unknown になる。
 * 詰将棋生成（TsumeCandidateJob）と CLI の余詰検査（TsumeVerificationRunner）が同じ手順で使う。
 */
class TsumeVerificationJob : public MateQuerySource
{
public:
    /// 1回の問い合わせの上限
    static constexpr int kQueryMaxMs = 1000;

    TsumeVerificationJob(const QString& sfen, int targetMoves, const TsumeshogiVerifier::Options& options,
                         int budgetMs);

    std::optional<MateQuery> nextQuery() override;
    void submit(const MateAnswer& answer, qint64 elapsedMs) override;
    bool isDone() const override;

    /// 打ち切る（結果は Unknown）
    void abort() { m_verifier.abort(); }
    const TsumeshogiVerifier::Result& result() const { return m_verifier.result(); }
    QString sfen() const { return m_sfen; }
    int queries() const { return m_queries; }

private:
    TsumeshogiVerifier m_verifier;
    QString m_sfen;
    int m_budgetMs = 0;
    qint64 m_spentMs = 0;
    int m_queries = 0;
};

#endif // TSUMEVERIFICATIONJOB_H
