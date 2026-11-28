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
/// （手順が輪になる局面で、見積もりどうしが互いを数え合って際限なく増えるのを防ぐ。残りの手数も深さで決まる）
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
                                                             int maxPlies, int timeLimitMs, const std::atomic_bool& stop,
                                                             const std::atomic_bool* alsoStop)
{
    return search(position, attacker, maxPlies, timeLimitMs, stop, alsoStop, false);
}

TsumeProofNumberSearch::Result TsumeProofNumberSearch::solveLine(const shogi::Position& position, shogi::Color attacker,
                                                                 int maxPlies, int timeLimitMs,
                                                                 const std::atomic_bool& stop,
                                                                 const std::atomic_bool* alsoStop)
{
    return search(position, attacker, maxPlies, timeLimitMs, stop, alsoStop, true);
}

TsumeProofNumberSearch::Result TsumeProofNumberSearch::search(const shogi::Position& position, shogi::Color attacker,
                                                              int maxPlies, int timeLimitMs, const std::atomic_bool& stop,
                                                              const std::atomic_bool* alsoStop, bool wantLine)
{
    using Clock = std::chrono::steady_clock;
    m_position = position;
    m_attacker = attacker;
    m_stop = &stop;
    m_alsoStop = alsoStop;
    m_verdicts.reset(kInitialSlots);
    m_estimates.reset(kInitialSlots);
    // 深さごとの作業領域は、再帰の途中で場所が変わらないよう先に全部用意する
    m_moves.resize(static_cast<std::size_t>(kMaxDepth) + 1);
    m_children.resize(static_cast<std::size_t>(kMaxDepth) + 1);
    m_nodes = 0;

    const Clock::time_point start = Clock::now();
    const Clock::time_point deadline = timeLimitMs > 0 ? start + std::chrono::milliseconds(timeLimitMs)
                                                       : Clock::time_point::max();
    const bool bounded = maxPlies >= 0 && maxPlies <= kMaxDepth;

    Result result;
    if (bounded && maxPlies == 0) {
        // 手数が残っていない: 今の局面が詰んでいるかだけ
        result = run(0, deadline);
    } else {
        // 1) 手数を限らずに調べる（何手かけても詰まないことは、こちらが早く分かる）。
        //    手数を限るときは、判定時間の一部だけを使う
        Clock::time_point firstDeadline = deadline;
        if (bounded && timeLimitMs > 0) {
            const int share = std::max(timeLimitMs / kUnlimitedShare, kUnlimitedMinMs);
            firstDeadline = std::min(deadline, start + std::chrono::milliseconds(share));
        }
        result = run(kUnlimited, firstDeadline);
        const bool tooLongProof = result.status == Status::Mate && maxPlies >= 0 && result.plies > maxPlies;
        if (bounded && (tooLongProof || result.status == Status::Unknown)) {
            // 2) 指定の手数以内に詰むかを調べる。ここまでの詰み・不詰の結論は引き継ぐ
            result = run(maxPlies, deadline);
        } else if (tooLongProof) {
            // 深さの上限より長い手数の指定: 証明した手順がそれより長ければ、指定の手数で詰むかは分からない
            result = Result{};
        }
    }
    if (result.status == Status::Mate) {
        // 3) 最短の手数と、最初の手を確かめる
        const Settled settled = settle(result.plies, result.move, deadline);
        if (settled == Settled::Cancelled) result.status = Status::Cancelled;
        result.exact = settled == Settled::Exact;
        // 4) 詰みまでの手順をたどる
        if (wantLine && result.exact) followLine(result, deadline);
    }
    result.nodes = m_nodes;
    return result;
}

TsumeProofNumberSearch::Result TsumeProofNumberSearch::run(int maxPlies, std::chrono::steady_clock::time_point deadline)
{
    m_maxPlies = maxPlies;
    m_deadline = deadline;
    // 見積もりと、手順に頼る不詰の覚えは、根と手数の上限ごとのもの
    m_estimates.clear();
    m_onPath.clear();
    m_pathDisproofs.clear();
    m_aborted = false;
    m_cancelled = false;
    m_rootReply = shogi::Move{};

    Result result;
    checkLimits();
    const Visit root = m_aborted ? Visit{} : visit({kInfinite, kInfinite}, 0);
    if (m_cancelled) {
        result.status = Status::Cancelled;
    } else if (m_aborted) {
        result.status = Status::Unknown;
    } else if (root.numbers.dn == 0 && root.disproof.dependencies.none()) {
        // 根より上に手順は無いので、根の不詰は手順に頼らない（頼っていたら結論にしない）
        result.status = root.disproof.limited ? Status::TooLong : Status::NoMate;
        result.move = m_rootReply;
        if (!result.move.is_valid() && m_position.side_to_move() != m_attacker) {
            // 手数が残っていない玉方の手番: どの応手でも詰みを防げる
            std::vector<shogi::Move> replies;
            m_position.generate_legal_moves(replies);
            if (!replies.empty()) result.move = replies.front();
        }
    } else if (root.numbers.pn == 0 && root.length < kAssumedLength) {
        result.status = Status::Mate;
        result.plies = root.length;
        result.move = m_rootReply;
    }
    return result;
}

TsumeProofNumberSearch::Settled TsumeProofNumberSearch::settle(int& plies, shogi::Move& move,
                                                               std::chrono::steady_clock::time_point deadline)
{
    return m_position.side_to_move() == m_attacker ? settleAttack(plies, move, deadline)
                                                   : settleDefense(plies, move, deadline);
}

TsumeProofNumberSearch::Settled TsumeProofNumberSearch::settleAttack(int& plies, shogi::Move& move,
                                                                     std::chrono::steady_clock::time_point deadline)
{
    // 2手短い手数では詰まないことを確かめる。詰んだら、その手数からやり直す（手数は減るだけなので終わる）
    Settled settled = Settled::Exact;
    while (plies > 1) {
        const Result shorter = run(plies - 2, deadline);
        if (shorter.status == Status::TooLong || shorter.status == Status::NoMate) break;
        if (shorter.status != Status::Mate) {
            settled = shorter.status == Status::Cancelled ? Settled::Cancelled : Settled::Unverified;
            break;
        }
        plies = shorter.plies;
    }
    if (!findMatingMove(plies, move) && settled == Settled::Exact) settled = Settled::Unverified;
    return settled;
}

TsumeProofNumberSearch::Settled TsumeProofNumberSearch::settleDefense(int& plies, shogi::Move& move,
                                                                      std::chrono::steady_clock::time_point deadline)
{
    std::vector<shogi::Move> replies;
    m_position.generate_legal_moves(replies);
    if (replies.empty()) {
        plies = 0;      // もう詰んでいる
        return Settled::Exact;
    }
    // 応手ごとの、証明した手順の長さ（どの応手の後も詰みを証明してある）
    std::vector<int> lengths;
    for (const shogi::Move& reply : replies) {
        shogi::MoveUndo undo;
        m_position.make_move(reply, undo);
        const Verdict* verdict = m_verdicts.find(keyOf(m_position));
        m_position.unmake_move(reply, undo);
        if (!verdict || verdict->mate >= kAssumedLength) return Settled::Unverified;
        lengths.push_back(verdict->mate);
    }
    // 手順がいちばん長い応手を選び、その後の局面がそれより短くは詰まないことを確かめる。
    // 短く詰んだら、その応手の長さを直して選び直す（長さは減るだけなので終わる）
    while (true) {
        const std::size_t longest = static_cast<std::size_t>(
            std::max_element(lengths.begin(), lengths.end()) - lengths.begin());
        move = replies[longest];
        plies = 1 + lengths[longest];
        if (lengths[longest] <= 1) return Settled::Exact;   // どの応手にも次の1手で詰む
        shogi::MoveUndo undo;
        m_position.make_move(replies[longest], undo);
        const Result shorter = run(lengths[longest] - 2, deadline);
        m_position.unmake_move(replies[longest], undo);
        if (shorter.status == Status::TooLong || shorter.status == Status::NoMate) return Settled::Exact;
        if (shorter.status == Status::Cancelled) return Settled::Cancelled;
        if (shorter.status != Status::Mate) return Settled::Unverified;     // 時間切れなど
        lengths[longest] = shorter.plies;
    }
}

bool TsumeProofNumberSearch::findMatingMove(int plies, shogi::Move& move)
{
    std::vector<shogi::Move> checks;
    m_position.generate_checking_moves(checks);
    int shortest = plies;   // 後の局面の手数は plies - 1 以下
    bool found = false;
    for (const shogi::Move& check : checks) {
        shogi::MoveUndo undo;
        m_position.make_move(check, undo);
        const Verdict* verdict = m_verdicts.find(keyOf(m_position));
        m_position.unmake_move(check, undo);
        if (!verdict || static_cast<int>(verdict->mate) >= shortest) continue;
        shortest = verdict->mate;
        move = check;
        found = true;
    }
    return found;
}

void TsumeProofNumberSearch::followLine(Result& result, std::chrono::steady_clock::time_point deadline)
{
    // 攻め方の手番の局面は、最短の手数を確かめてあるので、証明してある手を指すだけ。
    // 玉方の手番の局面は、いちばん長く粘る応手を確かめる（残りの手数ちょうどのはず）
    std::vector<shogi::Move> line;
    int plies = result.plies;
    shogi::Move move = result.move;
    while (true) {
        if (!move.is_valid()) return;
        line.push_back(move);
        m_position.do_move(move);
        if (--plies <= 0) break;
        if (m_position.side_to_move() == m_attacker) {
            if (!findMatingMove(plies, move)) return;
            continue;
        }
        int length = plies;
        const Settled settled = settleDefense(length, move, deadline);
        if (settled == Settled::Cancelled) result.status = Status::Cancelled;
        if (settled != Settled::Exact || length != plies) return;
    }
    result.line = std::move(line);
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
    const int remaining = remainingAt(depth);
    Visit outOfPlies;
    outOfPlies.numbers = {kInfinite, 0};
    outOfPlies.disproof.limited = true;
    // 手数の上限: 攻め方はもう指せない
    if (attack && remaining <= 0) return outOfPlies;

    const std::uint64_t key = keyOf(m_position);
    // 親が子の値を引いた後に、別の手順からこの局面の結論が出ていることがある（根は応手を決めるので調べる）
    Visit known;
    if (depth > 0 && concluded(key, remaining, known)) return known;

    std::vector<shogi::Move>& moves = m_moves[static_cast<std::size_t>(depth)];
    if (attack) m_position.generate_checking_moves(moves);
    else m_position.generate_legal_moves(moves);

    if (moves.empty()) {
        // 攻め方に王手が無ければ不詰。玉方に指す手が無ければ（王手がかかっていれば）詰み
        Visit leaf;
        if (!attack && m_position.is_in_check(m_position.side_to_move())) {
            storeMate(key, 0);
            leaf.numbers = {0, kInfinite};
        } else {
            storeRefutation(key, false, remaining);
            leaf.numbers = {kInfinite, 0};
        }
        return leaf;
    }
    if (remaining <= 0) {
        // 手数の上限: 玉方に応手があり、攻め方の手数は残っていない
        storeRefutation(key, true, 0);
        return outOfPlies;
    }
    if (depth >= kMaxDepth) {
        // これより深くは調べない。詰みと仮定する（この深さの見積もりとして覚え、手数は「分からない」にする）
        storeEstimate(key, depth, {0, kInfinite});
        Visit assumed;
        assumed.numbers = {0, kInfinite};
        assumed.length = kAssumedLength;
        return assumed;
    }

    std::vector<Child>& children = m_children[static_cast<std::size_t>(depth)];
    children.assign(moves.size(), Child{});
    for (std::size_t i = 0; i < moves.size(); ++i) {
        shogi::MoveUndo undo;
        m_position.make_move(moves[i], undo);
        children[i].key = keyOf(m_position);
        m_position.unmake_move(moves[i], undo);
        lookUp(children[i], !attack, depth + 1);
    }

    m_onPath[key] = depth;
    Tally current;
    while (true) {
        current = tally(children, attack);
        const Numbers numbers = current.numbers;
        if (numbers.pn == 0 || numbers.dn == 0 || numbers.pn >= limit.pn || numbers.dn >= limit.dn) break;

        // いちばん有望な子を、2番目の子より悪くなるか、この局面のしきい値を超えるまで調べる。
        // 2番目の値をわずかに超えただけで戻ると、子の間の行き来で同じ局面を何度も調べ直すので、余裕を持たせる
        const std::uint32_t untilSecond = current.second >= kInfinite
            ? kInfinite : saturated(std::uint64_t{current.second} + 1 + current.second / 4, kInfinite);
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
        Child& visited = children[current.best];
        visited.numbers = child.numbers;
        visited.length = child.length;
        visited.disproof = child.disproof;
    }
    m_onPath.erase(key);
    if (m_aborted) return {};
    return conclude(key, depth, attack, current);
}

bool TsumeProofNumberSearch::concluded(std::uint64_t key, int remaining, Visit& result) const
{
    // どの手順から来ても成り立つ結論（詰みは残りの手数に収まるもの、不詰は残りの手数まで確かめたもの）
    const Verdict* verdict = m_verdicts.find(key);
    if (!verdict) return false;
    if (verdict->mate != kNoLength && static_cast<int>(verdict->mate) <= remaining) {
        result.numbers = {0, kInfinite};
        result.length = verdict->mate;
        return true;
    }
    const bool always = verdict->refuted == kRefutedAlways;
    if (always || (verdict->refuted != 0 && static_cast<int>(verdict->refuted) > remaining)) {
        result.numbers = {kInfinite, 0};
        result.disproof.limited = !always;
        return true;
    }
    return false;
}

void TsumeProofNumberSearch::lookUp(Child& child, bool childAttacks, int childDepth) const
{
    const int remaining = remainingAt(childDepth);
    Visit known;
    if (concluded(child.key, remaining, known)) {
        child.numbers = known.numbers;
        child.length = known.length;
        child.disproof = known.disproof;
        return;
    }
    child.numbers = {kInfinite, 0};
    if (childAttacks && remaining <= 0) {
        child.disproof.limited = true;      // 手数の上限: 攻め方はもう指せない
        return;
    }
    // 手順の途中の局面に戻る手は、詰みに役立たない（局面の繰り返し）
    if (const auto onPath = m_onPath.find(child.key); onPath != m_onPath.end()) {
        child.disproof.dependencies.add(child.key, onPath->second);
        return;
    }
    if (recall(child.key, remaining, child.disproof)) return;
    child.numbers = {};
    if (const Numbers* estimate = m_estimates.find(estimateKey(child.key, childDepth))) {
        child.numbers = *estimate;
        if (estimate->pn == 0) child.length = kAssumedLength;   // 深さの上限で詰みと仮定した局面
    }
}

TsumeProofNumberSearch::Tally TsumeProofNumberSearch::tally(const std::vector<Child>& children, bool attack) const
{
    Tally result;
    result.second = kInfinite + 1;
    std::uint64_t sumPn = 0;
    std::uint64_t sumDn = 0;
    std::uint32_t minPn = kInfinite;
    std::uint32_t minDn = kInfinite;
    std::uint32_t best = kInfinite + 1;
    bool anyProven = false;
    bool anyDisproven = false;
    int shortest = 0;
    int longest = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
        const Numbers numbers = children[i].numbers;
        const int length = children[i].length;
        const Disproof& disproof = children[i].disproof;
        sumPn += numbers.pn;
        sumDn += numbers.dn;
        minPn = std::min(minPn, numbers.pn);
        minDn = std::min(minDn, numbers.dn);
        if (numbers.pn == 0) {
            if (!anyProven || length < shortest) shortest = length;
            if (!anyProven || length > longest) {
                longest = length;
                result.longest = i;
            }
            anyProven = true;
        }
        if (numbers.dn == 0) {
            if (attack) {
                // 攻め方の手番の不詰は、全部の王手の不詰に頼る
                result.disproof.dependencies.merge(disproof.dependencies);
                result.disproof.limited |= disproof.limited;
            } else {
                // 玉方の手番の不詰は、1つの応手の不詰に頼る。
                // 手順に頼らないもの、手数の上限に頼らないもの、浅い局面に頼らないものの順に選ぶ
                const Disproof& chosen = result.disproof;
                const bool free = disproof.dependencies.none();
                const bool chosenFree = chosen.dependencies.none();
                bool better = !anyDisproven;
                if (!better && free != chosenFree) better = free;
                else if (!better && disproof.limited != chosen.limited) better = !disproof.limited;
                else if (!better && !free) better = disproof.dependencies.shallowest > chosen.dependencies.shallowest;
                if (better) {
                    result.disproof = disproof;
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
    // 詰みの手順の長さ: 攻め方はいちばん短い手、玉方はいちばん長い応手
    result.length = std::min<int>(kAssumedLength, 1 + (attack ? shortest : longest));
    return result;
}

TsumeProofNumberSearch::Visit TsumeProofNumberSearch::conclude(std::uint64_t key, int depth, bool attack,
                                                                const Tally& tally)
{
    Visit result;
    result.numbers = tally.numbers;
    if (tally.numbers.pn == 0) {
        if (depth == 0 && !attack) m_rootReply = m_moves[0][tally.longest];
        storeMate(key, tally.length);
        result.length = tally.length;
        return result;
    }
    if (tally.numbers.dn != 0) {
        storeEstimate(key, depth, tally.numbers);
        return result;
    }
    if (depth == 0 && !attack) m_rootReply = m_moves[0][tally.refuter];

    // この局面へ戻る繰り返しは、この局面より下で閉じている。残るのは、この局面より上の局面に頼る分
    const Dependencies& all = tally.disproof.dependencies;
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
    const bool limited = tally.disproof.limited;
    const int remaining = remainingAt(depth);
    result.disproof.limited = limited;
    if (above.none()) {
        // 頼っている局面がこの局面より上に無ければ、どの手順から来ても不詰
        storeRefutation(key, limited, remaining);
        return result;
    }
    if (!above.incomplete) remember(key, above, limited ? remaining : kUnlimited);
    result.disproof.dependencies = above;
    return result;
}

bool TsumeProofNumberSearch::recall(std::uint64_t key, int remaining, Disproof& disproof) const
{
    const auto found = m_pathDisproofs.find(key);
    if (found == m_pathDisproofs.end() || found->second.validUpTo < remaining) return false;
    Dependencies current;
    for (int i = 0; i < found->second.dependencies.count; ++i) {
        const std::uint64_t dependency = found->second.dependencies.keys[static_cast<std::size_t>(i)];
        const auto onPath = m_onPath.find(dependency);
        if (onPath == m_onPath.end()) return false;     // 頼っている局面が今の手順に無い: この覚えは使えない
        current.add(dependency, onPath->second);
    }
    disproof.dependencies = current;
    disproof.limited = found->second.validUpTo != kUnlimited;
    return true;
}

void TsumeProofNumberSearch::remember(std::uint64_t key, const Dependencies& dependencies, int validUpTo)
{
    // 覚えは探索を速くするだけなので、増えすぎたら捨てる
    if (m_pathDisproofs.size() >= kMaxPathDisproofs) m_pathDisproofs.clear();
    m_pathDisproofs[key] = {dependencies, validUpTo};
}

void TsumeProofNumberSearch::storeMate(std::uint64_t key, int length)
{
    Verdict* verdict = m_verdicts.obtain(key, kMaxSlots);
    if (!verdict) {
        m_aborted = true;   // 結論を覚えられなければ続けられない
        return;
    }
    const auto value = static_cast<std::uint16_t>(std::min<int>(length, kAssumedLength));
    verdict->mate = std::min(verdict->mate, value);
}

void TsumeProofNumberSearch::storeRefutation(std::uint64_t key, bool limited, int remaining)
{
    Verdict* verdict = m_verdicts.obtain(key, kMaxSlots);
    if (!verdict) {
        m_aborted = true;
        return;
    }
    if (!limited) {
        verdict->refuted = kRefutedAlways;
    } else if (verdict->refuted != kRefutedAlways) {
        verdict->refuted = std::max(verdict->refuted, static_cast<std::uint16_t>(std::max(remaining, 0) + 1));
    }
}

void TsumeProofNumberSearch::storeEstimate(std::uint64_t key, int depth, Numbers numbers)
{
    const std::uint64_t slotKey = estimateKey(key, depth);
    Numbers* estimate = m_estimates.obtain(slotKey, kMaxSlots);
    if (!estimate) {
        // 一杯になったら見積もりを捨てる（探索し直せば同じ値になる）
        m_estimates.clear();
        estimate = m_estimates.obtain(slotKey, kMaxSlots);
        if (!estimate) {
            m_aborted = true;
            return;
        }
    }
    *estimate = numbers;
}
