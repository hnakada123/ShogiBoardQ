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
 * 深さを延ばしながら全部の手を調べる探索（shogi::TsumeSearch）は、長手数の局面を判定時間内に調べきれない。
 * この探索は結論に近い手から調べるので、長手数の詰み・不詰を短い時間で確かめられる。
 *
 * solve() は次の順に調べる:
 * 1. 手数を限らずに調べる（判定時間の一部）。何手かけても詰まないこと（NoMate）は、こちらが早く分かる
 * 2. 指定の手数以内に詰むかを調べる。詰む（Mate）か、その手数では詰まない（TooLong）
 * 3. 詰みで、調べた局面が玉方の手番なら、いちばん長く粘る応手と最短の手数を確かめる（Result::exact）
 *
 * 結論の根拠:
 * - 詰みは、玉方の全部の応手に攻め方の詰む手があること。証明した手順の長さ（手数の上限）を一緒に覚える
 * - 不詰は「攻め方に王手が無い」「手数の上限に達した」「手順の途中の局面に戻る」だけから組み立てる
 *   （詰みがあるなら、同じ局面を繰り返さない詰みの手順がある）
 * - 局面に戻ることに頼った不詰は、頼った局面が今の手順にある間だけ使う（別の手順から来た同じ局面には使わない）
 * - 手数の上限に頼った不詰は、残りの手数がそれ以下のときだけ使う
 * - 深さの上限に達した局面は詰みと仮定し、手数を「分からない」とする。仮定に頼った詰みは結論にしない
 *
 * 1つのインスタンスを同時に複数のスレッドから使わない。
 */
class TsumeProofNumberSearch
{
public:
    enum class Status {
        NoMate,     ///< 何手かけても詰まない
        Mate,       ///< 詰む（Result::plies 手以内）
        TooLong,    ///< 指定の手数以内には詰まない（それより長い詰みがあるかは分からない）
        Unknown,    ///< 時間・記憶域の上限までに分からなかった
        Cancelled,  ///< 止められた
    };

    struct Result {
        Status status = Status::Unknown;
        int plies = 0;              ///< Mate: 証明した手順の長さ（この手数以内に詰む）
        bool exact = false;         ///< Mate: plies が最短の手数（玉方が最も長く粘ったとき）で、reply がその応手だと確かめた
        shogi::Move reply{};        ///< 調べた局面が玉方の手番のとき: Mate は長く粘る応手、NoMate・TooLong は詰みを防ぐ応手
        std::uint64_t nodes = 0;    ///< 調べた局面の数
    };

    /// 手数を限らない
    static constexpr int kNoPlyLimit = -1;

    /// position から attacker が王手を続けて、maxPlies 手以内に詰ませられるかを調べる（kNoPlyLimit なら手数を限らない）。
    /// position は攻め方の手番か、王手のかかった玉方の手番。timeLimitMs が 0 なら時間で打ち切らない。
    /// stop か alsoStop（省略可）が true になったら Cancelled で戻る。
    Result solve(const shogi::Position& position, shogi::Color attacker, int maxPlies, int timeLimitMs,
                 const std::atomic_bool& stop, const std::atomic_bool* alsoStop = nullptr);

private:
    static constexpr std::uint32_t kInfinite = 1000000000U;   ///< 証明数・反証数の無限大
    static constexpr int kMaxDepth = 256;
    static constexpr int kUnlimited = 1 << 20;                ///< 手数を限らないときの残りの手数
    static constexpr int kNoDepth = 1 << 30;
    static constexpr int kMaxDependencies = 4;
    static constexpr std::uint16_t kNoLength = 0xFFFF;        ///< 詰みを証明していない
    static constexpr std::uint16_t kAssumedLength = 0xFFFE;   ///< 深さの上限で詰みと仮定した（手数は分からない）
    static constexpr std::uint16_t kRefutedAlways = 0xFFFF;   ///< 何手かけても詰まない
    static constexpr std::size_t kInitialSlots = std::size_t{1} << 16;
    static constexpr std::size_t kMaxSlots = std::size_t{1} << 21;            ///< 表1つの上限（16 バイト × 約210万 = 32MB）
    static constexpr std::size_t kMaxPathDisproofs = std::size_t{1} << 18;
    /// 手数を限らない探索に使う時間（判定時間をこの数で割った長さ。ただし kUnlimitedMinMs 以上）
    static constexpr int kUnlimitedShare = 4;
    static constexpr int kUnlimitedMinMs = 100;

    /// 証明数（詰みを示すのに要る局面の数の見積もり。0 は詰み）と反証数（不詰を示すのに要る数。0 は不詰）
    struct Numbers {
        std::uint32_t pn = 1;
        std::uint32_t dn = 1;
    };

    /// 局面の結論（どの手順から来ても成り立つ）
    struct Verdict {
        std::uint16_t mate = kNoLength;   ///< この手数以内に詰む
        std::uint16_t refuted = 0;        ///< 0: なし。kRefutedAlways: 何手でも詰まない。それ以外: (値 - 1) 手以内には詰まない
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

    /// 不詰の結論が頼っているもの
    struct Disproof {
        bool limited = false;        ///< 手数の上限に頼っている（残りの手数がそのとき以下の間だけ成り立つ）
        Dependencies dependencies;   ///< 手順の途中の局面に頼っている。空なら、どの手順から来ても成り立つ
    };

    /// 手順の途中の局面に頼る不詰の覚え
    struct PathDisproof {
        Dependencies dependencies;
        int validUpTo = kUnlimited;  ///< 残りの手数がこれ以下なら成り立つ
    };

    struct Visit {
        Numbers numbers;
        int length = 0;              ///< 詰み（pn == 0）: 証明した手順の長さ
        Disproof disproof;           ///< 不詰（dn == 0）が頼っているもの
    };

    /// 親を調べている間の、子の今の値。表を引くのは親に入ったときだけで、後は調べた子の結果で書き換える
    /// （見積もりは深さごとなので、ほかの子の見積もりが親の知らないうちに変わることはない）
    struct Child {
        std::uint64_t key = 0;
        Numbers numbers;
        int length = 0;              ///< 詰み（pn == 0）: 証明した手順の長さ
        Disproof disproof;           ///< 不詰（dn == 0）が頼っているもの
    };

    /// 子の今の値から求めた、局面の証明数・反証数と、次に調べる子
    struct Tally {
        Numbers numbers;
        std::size_t best = 0;        ///< 次に調べる子（攻め方は証明数、玉方は反証数が最小の子）
        Numbers bestNumbers;
        std::uint32_t second = 0;    ///< 2番目に小さい値
        int length = 0;              ///< 詰みのとき: 証明した手順の長さ
        std::size_t longest = 0;     ///< 玉方の手番で詰みのとき: 手順がいちばん長い応手
        std::size_t refuter = 0;     ///< 玉方の手番で不詰のとき: 不詰の応手
        Disproof disproof;           ///< 不詰のとき: 頼っているもの
    };

    /// 局面の鍵から値を引く表（空き番地法。7割を超えたら2倍にする）
    template <typename Value>
    class Table
    {
    public:
        void reset(std::size_t size)
        {
            // 引数の名前に slots は使えない（Qt のマクロ）
            m_slots.assign(size, Slot{});
            m_used = 0;
        }
        /// 値を全部捨てる（大きさは変えない）
        void clear() { reset(m_slots.size()); }
        const Value* find(std::uint64_t key) const
        {
            const std::size_t mask = m_slots.size() - 1;
            for (std::size_t i = static_cast<std::size_t>(key) & mask;; i = (i + 1) & mask) {
                if (m_slots[i].key == key) return &m_slots[i].value;
                if (m_slots[i].key == 0) return nullptr;
            }
        }
        /// key の値（無ければ既定の値で作る）。表が上限まで埋まっていて作れなければ nullptr
        Value* obtain(std::uint64_t key, std::size_t maxSlots)
        {
            if (const Value* found = find(key)) return const_cast<Value*>(found);
            if ((m_used + 1) * 10 > m_slots.size() * 7) {
                if (m_slots.size() >= maxSlots) return nullptr;
                std::vector<Slot> old = std::move(m_slots);
                m_slots.assign(old.size() * 2, Slot{});
                m_used = 0;
                for (const Slot& slot : old) {
                    if (slot.key != 0) *place(slot.key) = slot.value;
                }
            }
            return place(key);
        }

    private:
        struct Slot {
            std::uint64_t key = 0;   ///< 0 は空き
            Value value{};
        };
        Value* place(std::uint64_t key)
        {
            const std::size_t mask = m_slots.size() - 1;
            for (std::size_t i = static_cast<std::size_t>(key) & mask;; i = (i + 1) & mask) {
                if (m_slots[i].key == 0) {
                    m_slots[i].key = key;
                    ++m_used;
                    return &m_slots[i].value;
                }
            }
        }
        std::vector<Slot> m_slots;
        std::size_t m_used = 0;
    };

    Result run(int maxPlies, std::chrono::steady_clock::time_point deadline);
    void refine(Result& result, std::chrono::steady_clock::time_point deadline);
    Visit visit(Numbers limit, int depth);
    Tally tally(const std::vector<Child>& children, bool attack) const;
    /// 表から引いた子の今の値（結論・手順の途中の局面への戻り・手順に頼る不詰の覚え・見積もりの順）
    void lookUp(Child& child, bool childAttacks, int childDepth) const;
    /// 局面の結論が残りの手数で使えるなら result に入れる
    bool concluded(std::uint64_t key, int remaining, Visit& result) const;
    Visit conclude(std::uint64_t key, int depth, bool attack, const Tally& tally);
    bool recall(std::uint64_t key, int remaining, Disproof& disproof) const;
    void remember(std::uint64_t key, const Dependencies& dependencies, int validUpTo);
    void storeMate(std::uint64_t key, int length);
    void storeRefutation(std::uint64_t key, bool limited, int remaining);
    void storeEstimate(std::uint64_t key, int depth, Numbers numbers);
    void checkLimits();
    int remainingAt(int depth) const { return m_maxPlies - depth; }

    shogi::Position m_position;
    shogi::Color m_attacker = shogi::Color::Black;
    int m_maxPlies = kUnlimited;     ///< 今の探索の手数の上限（根からの手数）
    /// 局面の結論。探索の段階（手数を限らない・限る・応手の確かめ）をまたいで引き継ぐ
    Table<Verdict> m_verdicts;
    /// 結論の出ていない見積もり（深さごと）。一杯になったら捨てる
    Table<Numbers> m_estimates;
    /// 今の手順にある局面と深さ
    std::unordered_map<std::uint64_t, int> m_onPath;
    std::unordered_map<std::uint64_t, PathDisproof> m_pathDisproofs;
    /// 深さごとの作業領域
    std::vector<std::vector<shogi::Move>> m_moves;
    std::vector<std::vector<Child>> m_children;
    std::uint64_t m_nodes = 0;
    bool m_aborted = false;
    bool m_cancelled = false;
    shogi::Move m_rootReply{};
    std::chrono::steady_clock::time_point m_deadline;
    const std::atomic_bool* m_stop = nullptr;
    const std::atomic_bool* m_alsoStop = nullptr;
};

#endif // TSUMEPROOFNUMBERSEARCH_H
