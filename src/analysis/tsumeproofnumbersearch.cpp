/// @file tsumeproofnumbersearch.cpp
/// @brief 証明数探索（df-pn）による、王手の連続で詰むか・詰まないかの判定

#include "tsumeproofnumbersearch.h"

#include <algorithm>

namespace {

/// 表の鍵（0 は空きの印なので使わない）
std::uint64_t keyOf(const shogi::Position& position)
{
    const std::uint64_t key = position.position_key();
    return key == 0 ? 1 : key;
}

/// 結論の出ていない見積もりを覚える鍵。深さを混ぜるので、別の深さから見た同じ局面の見積もりは見えない
/// （手順が輪になる局面で、見積もりどうしが互いを数え合って際限なく増えるのを防ぐ）
std::uint64_t estimateKey(std::uint64_t key, int depth)
{
    const std::uint64_t mixed = key ^ ((static_cast<std::uint64_t>(depth) + 1) * 0x9E3779B97F4A7C15ULL);
    return mixed == 0 ? 1 : mixed;
}

std::uint32_t saturated(std::uint64_t value, std::uint32_t infinite)
{
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(value, infinite));
}

} // namespace

void TsumeProofNumberSearch::Dependencies::add(std::uint64_t key, int depth)
{
    shallowest = std::min(shallowest, depth);
    for (int i = 0; i < count; ++i) {
        if (keys[static_cast<std::size_t>(i)] == key) return;
    }
    if (count >= kMaxDependencies) {
        incomplete = true;
        return;
    }
    keys[static_cast<std::size_t>(count++)] = key;
}

void TsumeProofNumberSearch::Dependencies::merge(const Dependencies& other)
{
    incomplete |= other.incomplete;
    const int otherShallowest = other.shallowest;
    for (int i = 0; i < other.count; ++i) add(other.keys[static_cast<std::size_t>(i)], otherShallowest);
    shallowest = std::min(shallowest, otherShallowest);
}

TsumeProofNumberSearch::Result TsumeProofNumberSearch::solve(const shogi::Position& position, shogi::Color attacker,
                                                             int timeLimitMs, const std::atomic_bool& stop,
                                                             const std::atomic_bool* alsoStop)
{
    m_position = position;
    m_attacker = attacker;
    m_stop = &stop;
    m_alsoStop = alsoStop;
    m_deadline = timeLimitMs > 0 ? std::chrono::steady_clock::now() + std::chrono::milliseconds(timeLimitMs)
                                 : std::chrono::steady_clock::time_point::max();
    m_slots.assign(kInitialSlots, Slot{});
    m_used = 0;
    m_onPath.clear();
    m_pathDisproofs.clear();
    // 深さごとの作業領域は、再帰の途中で場所が変わらないよう先に全部用意する
    m_moves.resize(static_cast<std::size_t>(kMaxDepth) + 1);
    m_children.resize(static_cast<std::size_t>(kMaxDepth) + 1);
    m_nodes = 0;
    m_aborted = false;
    m_cancelled = false;
    m_assumedMate = false;
    m_refutation = shogi::Move{};

    Result result;
    checkLimits();
    const Visit root = m_aborted ? Visit{} : visit({kInfinite, kInfinite}, 0);
    result.nodes = m_nodes;
    if (m_cancelled) {
        result.status = Status::Cancelled;
    } else if (m_aborted) {
        result.status = Status::Unknown;
    } else if (root.numbers.dn == 0 && root.dependencies.none()) {
        // 根より上に手順は無いので、根の不詰は手順に頼らない（頼っていたら結論にしない）
        result.status = Status::NoMate;
        result.refutation = m_refutation;
    } else if (root.numbers.pn == 0 && !m_assumedMate) {
        result.status = Status::Mate;
    }
    return result;
}

void TsumeProofNumberSearch::checkLimits()
{
    if (m_stop->load(std::memory_order_relaxed)
        || (m_alsoStop != nullptr && m_alsoStop->load(std::memory_order_relaxed))) {
        m_aborted = true;
        m_cancelled = true;
    } else if (std::chrono::steady_clock::now() >= m_deadline) {
        m_aborted = true;
    }
}

TsumeProofNumberSearch::Visit TsumeProofNumberSearch::visit(Numbers limit, int depth)
{
    ++m_nodes;
    if ((m_nodes & 1023) == 0) checkLimits();
    if (m_aborted) return {};

    const bool attack = m_position.side_to_move() == m_attacker;
    const std::uint64_t key = keyOf(m_position);
    std::vector<shogi::Move>& moves = m_moves[static_cast<std::size_t>(depth)];
    if (attack) m_position.generate_checking_moves(moves);
    else m_position.generate_legal_moves(moves);

    if (moves.empty()) {
        // 攻め方に王手が無ければ不詰。玉方に指す手が無ければ（王手がかかっていれば）詰み
        const bool mated = !attack && m_position.is_in_check(m_position.side_to_move());
        const Numbers numbers = mated ? Numbers{0, kInfinite} : Numbers{kInfinite, 0};
        store(key, numbers);
        return {numbers, {}};
    }
    if (depth >= kMaxDepth) {
        // これより深くは調べない。詰みと仮定する（この深さの見積もりとして覚え、結論にはしない）
        m_assumedMate = true;
        store(estimateKey(key, depth), {0, kInfinite});
        return {{0, kInfinite}, {}};
    }

    std::vector<Child>& children = m_children[static_cast<std::size_t>(depth)];
    children.assign(moves.size(), Child{});
    for (std::size_t i = 0; i < moves.size(); ++i) {
        shogi::MoveUndo undo;
        m_position.make_move(moves[i], undo);
        children[i].key = keyOf(m_position);
        m_position.unmake_move(moves[i], undo);
    }

    m_onPath[key] = depth;
    Tally current;
    while (true) {
        current = tally(children, attack, depth);
        const Numbers numbers = current.numbers;
        if (numbers.pn == 0 || numbers.dn == 0 || numbers.pn >= limit.pn || numbers.dn >= limit.dn) break;

        // いちばん有望な子を、2番目の子より悪くなるか、この局面のしきい値を超えるまで調べる
        const std::uint32_t untilSecond = current.second >= kInfinite ? kInfinite : current.second + 1;
        Numbers childLimit;
        if (attack) {
            childLimit.pn = std::min(limit.pn, untilSecond);
            childLimit.dn = saturated(std::uint64_t{limit.dn} - numbers.dn + current.bestNumbers.dn, kInfinite);
        } else {
            childLimit.pn = saturated(std::uint64_t{limit.pn} - numbers.pn + current.bestNumbers.pn, kInfinite);
            childLimit.dn = std::min(limit.dn, untilSecond);
        }
        const shogi::Move move = moves[current.best];
        shogi::MoveUndo undo;
        m_position.make_move(move, undo);
        const Visit child = visit(childLimit, depth + 1);
        m_position.unmake_move(move, undo);
        if (m_aborted) break;
        if (child.numbers.dn == 0 && !child.dependencies.none()) {
            // 手順の途中の局面に頼る不詰は表の結論にならないので、この局面を調べている間は子に覚えておく
            children[current.best].pathDisproven = true;
            children[current.best].dependencies = child.dependencies;
        }
    }
    m_onPath.erase(key);
    if (m_aborted) return {};
    return conclude(key, depth, attack, current);
}

TsumeProofNumberSearch::Numbers TsumeProofNumberSearch::childNumbers(const Child& child, int childDepth,
                                                                      Dependencies& dependencies) const
{
    // どの手順から来ても成り立つ結論
    if (const Slot* slot = find(child.key)) return slot->numbers;
    if (child.pathDisproven) {
        dependencies = child.dependencies;
        return {kInfinite, 0};
    }
    // 手順の途中の局面に戻る手は、詰みに役立たない（局面の繰り返し）
    if (const auto onPath = m_onPath.find(child.key); onPath != m_onPath.end()) {
        dependencies.add(child.key, onPath->second);
        return {kInfinite, 0};
    }
    if (recall(child.key, dependencies)) return {kInfinite, 0};
    if (const Slot* slot = find(estimateKey(child.key, childDepth))) return slot->numbers;
    return {};
}

TsumeProofNumberSearch::Tally TsumeProofNumberSearch::tally(const std::vector<Child>& children, bool attack,
                                                             int depth) const
{
    Tally result;
    result.second = kInfinite + 1;
    std::uint64_t sumPn = 0;
    std::uint64_t sumDn = 0;
    std::uint32_t minPn = kInfinite;
    std::uint32_t minDn = kInfinite;
    std::uint32_t best = kInfinite + 1;
    bool anyDisproven = false;
    for (std::size_t i = 0; i < children.size(); ++i) {
        Dependencies dependencies;
        const Numbers numbers = childNumbers(children[i], depth + 1, dependencies);
        sumPn += numbers.pn;
        sumDn += numbers.dn;
        minPn = std::min(minPn, numbers.pn);
        minDn = std::min(minDn, numbers.dn);
        if (numbers.dn == 0) {
            if (attack) {
                // 攻め方の手番の不詰は、全部の王手の不詰に頼る
                result.dependencies.merge(dependencies);
            } else {
                // 玉方の手番の不詰は、1つの応手の不詰に頼る。手順に頼らないもの、次に浅い局面に頼らないものを選ぶ
                const Dependencies& chosen = result.dependencies;
                const bool better = !anyDisproven || (dependencies.none() && !chosen.none())
                    || (!dependencies.none() && !chosen.none() && dependencies.shallowest > chosen.shallowest);
                if (better) {
                    result.dependencies = dependencies;
                    result.refuter = i;
                }
            }
            anyDisproven = true;
        }
        const std::uint32_t value = attack ? numbers.pn : numbers.dn;
        if (value < best) {
            result.second = best;
            best = value;
            result.best = i;
            result.bestNumbers = numbers;
        } else if (value < result.second) {
            result.second = value;
        }
    }
    // 攻め方は1つの王手で詰めばよく、不詰には全部の王手の不詰が要る。玉方はその逆
    result.numbers = attack ? Numbers{minPn, saturated(sumDn, kInfinite)} : Numbers{saturated(sumPn, kInfinite), minDn};
    return result;
}

TsumeProofNumberSearch::Visit TsumeProofNumberSearch::conclude(std::uint64_t key, int depth, bool attack,
                                                                const Tally& tally)
{
    if (tally.numbers.pn == 0) {
        store(key, {0, kInfinite});
        return {{0, kInfinite}, {}};
    }
    if (tally.numbers.dn != 0) {
        store(estimateKey(key, depth), tally.numbers);
        return {tally.numbers, {}};
    }
    if (depth == 0 && !attack) m_refutation = m_moves[0][tally.refuter];

    // この局面へ戻る繰り返しは、この局面より下で閉じている。残るのは、この局面より上の局面に頼る分
    const Dependencies& all = tally.dependencies;
    Dependencies above;
    bool lost = false;
    for (int i = 0; i < all.count; ++i) {
        const std::uint64_t dependency = all.keys[static_cast<std::size_t>(i)];
        if (dependency == key) continue;
        const auto onPath = m_onPath.find(dependency);
        if (onPath == m_onPath.end()) lost = true;
        else above.add(dependency, onPath->second);
    }
    if (lost) {
        // 頼っている局面は手順の途中にあるはず。無ければ、根まで手順に頼るものとして扱う
        above.incomplete = true;
        above.shallowest = 0;
    } else if (all.incomplete && all.shallowest < depth) {
        // 覚えきれなかった分は、いちばん浅い深さだけが分かる。それがこの局面より上なら、上の局面に頼っている
        above.incomplete = true;
        above.shallowest = std::min(above.shallowest, all.shallowest);
    }
    if (above.none()) {
        store(key, {kInfinite, 0});
        return {{kInfinite, 0}, {}};
    }
    if (!above.incomplete) remember(key, above);
    return {{kInfinite, 0}, above};
}

bool TsumeProofNumberSearch::recall(std::uint64_t key, Dependencies& dependencies) const
{
    const auto found = m_pathDisproofs.find(key);
    if (found == m_pathDisproofs.end()) return false;
    Dependencies current;
    for (int i = 0; i < found->second.count; ++i) {
        const std::uint64_t dependency = found->second.keys[static_cast<std::size_t>(i)];
        const auto onPath = m_onPath.find(dependency);
        if (onPath == m_onPath.end()) return false;     // 頼っている局面が今の手順に無い: この覚えは使えない
        current.add(dependency, onPath->second);
    }
    dependencies = current;
    return true;
}

void TsumeProofNumberSearch::remember(std::uint64_t key, const Dependencies& dependencies)
{
    // 覚えは探索を速くするだけなので、増えすぎたら捨てる
    if (m_pathDisproofs.size() >= kMaxPathDisproofs) m_pathDisproofs.clear();
    m_pathDisproofs[key] = dependencies;
}

const TsumeProofNumberSearch::Slot* TsumeProofNumberSearch::find(std::uint64_t key) const
{
    const std::size_t mask = m_slots.size() - 1;
    for (std::size_t i = static_cast<std::size_t>(key) & mask;; i = (i + 1) & mask) {
        const Slot& slot = m_slots[i];
        if (slot.key == key) return &slot;
        if (slot.key == 0) return nullptr;
    }
}

void TsumeProofNumberSearch::insert(const Slot& slot)
{
    const std::size_t mask = m_slots.size() - 1;
    for (std::size_t i = static_cast<std::size_t>(slot.key) & mask;; i = (i + 1) & mask) {
        Slot& target = m_slots[i];
        if (target.key == slot.key) {
            target.numbers = slot.numbers;
            return;
        }
        if (target.key == 0) {
            target = slot;
            ++m_used;
            return;
        }
    }
}

void TsumeProofNumberSearch::store(std::uint64_t key, Numbers numbers)
{
    // 表の7割を超えないようにする（空きが必ずあるので、探す・入れるループは終わる）
    const bool known = find(key) != nullptr;
    if (!known && (m_used + 1) * 10 > m_slots.size() * 7 && !makeRoom()) {
        m_aborted = true;
        return;
    }
    insert({key, numbers});
}

bool TsumeProofNumberSearch::makeRoom()
{
    const bool atLimit = m_slots.size() >= kMaxSlots;
    std::vector<Slot> old = std::move(m_slots);
    m_slots.assign(atLimit ? old.size() : old.size() * 2, Slot{});
    m_used = 0;
    for (const Slot& slot : old) {
        if (slot.key == 0) continue;
        // 上限では、結論の出ていない見積もりを捨てる（探索し直せば同じ値になる）
        if (atLimit && slot.numbers.pn != 0 && slot.numbers.dn != 0) continue;
        insert(slot);
    }
    // 結論だけで半分を超えたら続けられない
    return m_used * 2 <= m_slots.size();
}
