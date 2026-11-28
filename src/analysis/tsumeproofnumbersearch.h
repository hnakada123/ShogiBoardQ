#ifndef TSUMEPROOFNUMBERSEARCH_H
#define TSUMEPROOFNUMBERSEARCH_H

/// @file tsumeproofnumbersearch.h
/// @brief 証明数探索（df-pn）による、王手の連続で詰むか・詰まないかの判定

#include "position.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

/**
 * @brief 王手の連続で詰むか・詰まないかを、証明数探索（df-pn）で調べる
 *
 * 深さを延ばしながら全部の手を調べる探索（shogi::TsumeSearch）は最短の手数を求められるが、
 * 「何手かけても詰まない」ことを確かめるのには向かない（長手数の詰将棋で、正解以外の手を指した後の局面など）。
 * この探索は手数を求めない代わりに、結論に近い手から調べるので、詰まないことを短い時間で確かめられる。
 *
 * 不詰（NoMate）の結論は次の2つだけから組み立てる。どちらも、詰みがあるなら成り立たない:
 * - 攻め方に王手が無い
 * - 手順の途中の局面に戻る（詰みがあるなら、同じ局面を繰り返さない詰みの手順がある）
 *
 * 局面に戻ることに頼った不詰は、頼った局面が今の手順にある間だけ使う（別の手順から来た同じ局面には使わない）。
 * 深さの上限に達した局面は詰みと仮定するので、不詰の結論はこの仮定の下でも成り立つ。仮定を使った探索は
 * 詰み（Mate）を返さない。
 *
 * 1つのインスタンスを同時に複数のスレッドから使わない。
 */
class TsumeProofNumberSearch
{
public:
    enum class Status {
        NoMate,     ///< 何手かけても詰まない
        Mate,       ///< 詰みがある（手数は分からない）
        Unknown,    ///< 時間・記憶域の上限までに分からなかった
        Cancelled,  ///< 止められた
    };

    struct Result {
        Status status = Status::Unknown;
        shogi::Move refutation{};   ///< NoMate で、調べた局面が玉方の手番のとき: 詰みを防ぐ応手
        std::uint64_t nodes = 0;    ///< 調べた局面の数
    };

    /// position から attacker が王手を続けて詰ませられるかを調べる。
    /// position は攻め方の手番か、王手のかかった玉方の手番。timeLimitMs が 0 なら時間で打ち切らない。
    /// stop か alsoStop（省略可）が true になったら Cancelled で戻る。
    Result solve(const shogi::Position& position, shogi::Color attacker, int timeLimitMs,
                 const std::atomic_bool& stop, const std::atomic_bool* alsoStop = nullptr);

private:
    static constexpr std::uint32_t kInfinite = 1000000000U;   ///< 証明数・反証数の無限大
    static constexpr int kMaxDepth = 256;
    static constexpr int kNoDepth = 1 << 30;
    static constexpr int kMaxDependencies = 4;
    static constexpr std::size_t kInitialSlots = std::size_t{1} << 16;
    static constexpr std::size_t kMaxSlots = std::size_t{1} << 22;            ///< 16 バイト × 約420万 = 64MB
    static constexpr std::size_t kMaxPathDisproofs = std::size_t{1} << 18;

    /// 証明数（詰みを示すのに要る局面の数の見積もり。0 は詰み）と反証数（不詰を示すのに要る数。0 は不詰）
    struct Numbers {
        std::uint32_t pn = 1;
        std::uint32_t dn = 1;
    };

    /// 不詰の結論が頼っている、手順の途中の局面（そこへ戻る手を、局面の繰り返しとして不詰に数えた）
    struct Dependencies {
        std::array<std::uint64_t, kMaxDependencies> keys{};
        int count = 0;
        bool incomplete = false;     ///< 覚えきれない数の局面に頼っている（keys は一部だけ）
        int shallowest = kNoDepth;   ///< 頼っている局面のうち、いちばん浅い深さ

        bool none() const { return count == 0 && !incomplete; }
        void add(std::uint64_t key, int depth);
        void merge(const Dependencies& other);
    };

    struct Slot {
        std::uint64_t key = 0;       ///< 0 は空き
        Numbers numbers;
    };

    struct Visit {
        Numbers numbers;
        Dependencies dependencies;   ///< 不詰（dn == 0）が頼っている局面。空なら、どの手順から来ても不詰
    };

    struct Child {
        std::uint64_t key = 0;
        bool pathDisproven = false;  ///< 今の手順に頼る不詰と分かった（親を調べている間だけ使う）
        Dependencies dependencies;
    };

    /// 子の今の値から求めた、局面の証明数・反証数と、次に調べる子
    struct Tally {
        Numbers numbers;
        std::size_t best = 0;        ///< 次に調べる子（攻め方は証明数、玉方は反証数が最小の子）
        Numbers bestNumbers;
        std::uint32_t second = 0;    ///< 2番目に小さい値
        std::size_t refuter = 0;     ///< 玉方の手番で不詰のとき: 不詰の子
        Dependencies dependencies;   ///< 不詰のとき: 頼っている局面
    };

    Visit visit(Numbers limit, int depth);
    Tally tally(const std::vector<Child>& children, bool attack, int depth) const;
    Numbers childNumbers(const Child& child, int childDepth, Dependencies& dependencies) const;
    Visit conclude(std::uint64_t key, int depth, bool attack, const Tally& tally);
    bool recall(std::uint64_t key, Dependencies& dependencies) const;
    void remember(std::uint64_t key, const Dependencies& dependencies);

    const Slot* find(std::uint64_t key) const;
    void store(std::uint64_t key, Numbers numbers);
    void insert(const Slot& slot);
    bool makeRoom();
    void checkLimits();

    shogi::Position m_position;
    shogi::Color m_attacker = shogi::Color::Black;
    /// 局面の結論（どの手順から来ても成り立つ詰み・不詰）と、深さごとの見積もり
    std::vector<Slot> m_slots;
    std::size_t m_used = 0;
    /// 今の手順にある局面と深さ
    std::unordered_map<std::uint64_t, int> m_onPath;
    /// 手順の途中の局面に頼る不詰と、頼っている局面
    std::unordered_map<std::uint64_t, Dependencies> m_pathDisproofs;
    /// 深さごとの作業領域
    std::vector<std::vector<shogi::Move>> m_moves;
    std::vector<std::vector<Child>> m_children;
    std::uint64_t m_nodes = 0;
    bool m_aborted = false;
    bool m_cancelled = false;
    bool m_assumedMate = false;      ///< 深さの上限で詰みと仮定した局面がある
    shogi::Move m_refutation{};
    std::chrono::steady_clock::time_point m_deadline;
    const std::atomic_bool* m_stop = nullptr;
    const std::atomic_bool* m_alsoStop = nullptr;
};

#endif // TSUMEPROOFNUMBERSEARCH_H
