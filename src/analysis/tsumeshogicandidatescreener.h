#ifndef TSUMESHOGICANDIDATESCREENER_H
#define TSUMESHOGICANDIDATESCREENER_H

/// @file tsumeshogicandidatescreener.h
/// @brief 外部エンジンに送る前に候補局面を内蔵探索で選別するクラスの定義

#include "threadtypes.h"
#include "tsumeshogipositiongenerator.h"

#include <QString>
#include <QStringList>

#include <atomic>

/**
 * @brief Hayanagi の有限深さ詰み探索で、ランダム局面を外部エンジンに送る前に選別する
 *
 * 目標手数ちょうどで詰み、目標手数（＋上乗せ）以内で詰む初手が1つだけの局面を候補とする。
 * 有限深さの探索で見つかった詰みは実際の詰みなので、短い詰み・複数初手による除外は確定的である。
 * 時間切れ・中断・不正な局面は Unknown とし、取りこぼしを避けるため候補として扱う。
 * 唯一解の最終判定は従来どおり外部エンジンと TsumeshogiVerifier が行う。
 */
class TsumeshogiCandidateScreener
{
public:
    enum class Verdict {
        Candidate,          ///< 目標手数ちょうどの詰みがあり、探索範囲で詰む初手は1つ
        NotMateInTarget,    ///< 目標手数以内に詰みがない、または目標より短く詰む
        MultipleFirstMoves, ///< 探索範囲で詰む初手が2つ以上（余詰）
        Unknown             ///< 時間切れ・中断・不正な局面（候補として扱う）
    };

    struct Limits {
        int timeLimitMs = 20;            ///< 1局面の詰み存在探索の時間上限(ms)
        int alternativeTimeLimitMs = 10; ///< 各初手の別詰探索の時間上限(ms)
        int alternativeExtraPlies = 0;   ///< 別詰を探す深さの上乗せ（目標手数 + この値まで）
    };

    struct Batch {
        QStringList candidates;     ///< 候補局面（Unknown を含む）
        int generated = 0;          ///< 生成した局面数（候補以外を含む）
        int multipleFirstMoves = 0; ///< 複数初手で除外した局面数
        int unknown = 0;            ///< 時間切れで候補として通した局面数
    };

    static Verdict screen(const QString& sfen, int targetMoves, const Limits& limits,
                          const std::atomic_bool& stop);

    /// count 個の候補が集まるか、maxGenerated 局面を生成するか、cancelFlag が立つまで生成と選別を繰り返す。
    /// 呼び出し側がワーカースレッドで実行する想定
    static Batch generateBatch(const TsumeshogiPositionGenerator::Settings& settings, int targetMoves,
                               int count, int maxGenerated, const Limits& limits,
                               const CancelFlag& cancelFlag);
};

#endif // TSUMESHOGICANDIDATESCREENER_H
