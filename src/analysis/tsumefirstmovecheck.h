#ifndef TSUMEFIRSTMOVECHECK_H
#define TSUMEFIRSTMOVECHECK_H

/// @file tsumefirstmovecheck.h
/// @brief 成長中の候補局面の、軽い初手余詰の検査（引き出し型）

#include "mateoracle.h"
#include "tsumelinemetrics.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

/**
 * @brief 成長中の候補局面について、主手順の初手とは別の王手で詰まないかを軽く調べる
 *
 * 引き出し型: nextQuery() で次に詰みを問い合わせる局面を受け取り、答えを submit() で返す。
 * 結論が出ると verdict() が値を持つ。エンジンには触れない（問い合わせるのは生成器）。
 * 厳密な証明ではない（問い合わせの回数に上限があり、上限に達したら未確定にする）。
 */
class TsumeFirstMoveCheck
{
public:
    /// 検査の結論
    struct Verdict {
        bool unique = false;   ///< 別の初手では詰まないと確かめられた
        QString cookMove;      ///< 見つけた余詰の初手（空で unique が false なら未確定）
    };

    /// 判定済みの答えの引き当て（無ければ nullptr）
    using CacheLookup = std::function<const MateAnswer*(const QString& sfen)>;

    /// 検査を始める。内蔵探索だけで結論が出れば verdict() が値を持つ。局面を読めなければ nullopt
    static std::optional<TsumeFirstMoveCheck> begin(const QString& sfen, const QStringList& pv,
                                                    const TsumeLineMetrics::Metrics& metrics, bool fromRepair);

    const QString& sfen() const { return m_sfen; }
    const QStringList& mainPv() const { return m_mainPv; }
    const TsumeLineMetrics::Metrics& metrics() const { return m_metrics; }
    bool fromRepair() const { return m_fromRepair; }
    const std::optional<Verdict>& verdict() const { return m_verdict; }

    /// 次に問い合わせる局面（別の初手と応手の後の、攻方の手番）。結論が出たら nullopt
    std::optional<QString> nextQuery(const CacheLookup& cached);
    /// 問い合わせ（または引き当てた判定済み）の答えを渡す
    void submit(const MateAnswer& answer);

private:
    TsumeFirstMoveCheck(const QString& sfen, const QStringList& pv,
                        const TsumeLineMetrics::Metrics& metrics, bool fromRepair);
    /// いま調べている別の初手の後の、1手詰にならない応手の局面を並べる。局面に指せなければ false
    bool expandDefenses();

    QString m_sfen;
    QStringList m_mainPv;
    TsumeLineMetrics::Metrics m_metrics;
    bool m_fromRepair = false;
    QStringList m_altMoves;        ///< 主手順の初手以外の王手
    int m_altMoveIndex = 0;
    QStringList m_defenses;        ///< いまの別の初手に対する応手の後の局面（逃れになりやすい順）
    int m_defenseIndex = 0;
    bool m_altUnresolved = false;  ///< 詰むとも詰まないとも分からなかった応手がある
    int m_queriesUsed = 0;
    int m_maxQueries = 48;
    std::optional<Verdict> m_verdict;
};

#endif // TSUMEFIRSTMOVECHECK_H
