/// @file tsumelocalscreen.cpp
/// @brief 内蔵の詰み探索（Hayanagi）による、詰将棋の候補局面の事前ふるい分けの実装

#include "tsumelocalscreen.h"

#include <tsume.h>

#include <algorithm>
#include <atomic>
#include <vector>

namespace TsumeLocalScreen {

int defensePriority(const shogi::Position& beforeDefense, const shogi::Move& defense)
{
    if (defense.drop) return 400 + static_cast<int>(defense.piece);
    const bool isCapture = !shogi::is_empty(beforeDefense.piece_at(defense.to));
    const bool isKing = defense.piece == shogi::PieceType::King;
    if (isCapture && !isKing) return 100;
    if (isCapture && isKing) return 150;
    if (isKing) return 200;
    return 300;
}

bool hasImmediateMate(const shogi::Position& position)
{
    shogi::Position probe = position;
    std::vector<shogi::Move> checks;
    probe.generate_checking_moves(checks);
    for (const auto& mv : checks) {
        shogi::MoveUndo undo;
        probe.make_move(mv, undo);
        const bool mated = !probe.has_legal_move();
        probe.unmake_move(mv, undo);
        if (mated) return true;
    }
    return false;
}

QString quickCookCheck(const QString& sfen, const QString& mainMove, int plies, int perMoveBudgetMs)
{
    if (plies > 61) return {};
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return {};
    shogi::TsumeSearch search;
    const std::atomic_bool stop{false};
    const int depth = std::min(63, plies + 1);
    for (const auto& move : pos.generate_checking_moves()) {
        const QString usi = QString::fromStdString(pos.move_to_usi(move));
        if (usi == mainMove) continue;
        shogi::Position child = pos;
        child.do_move(move);
        if (!child.has_legal_move()) return usi;
        const auto res = search.solve(child, shogi::Color::Black, depth, perMoveBudgetMs, stop, 1);
        if (res.status == shogi::TsumeStatus::Mate) return usi;
    }
    return {};
}

MainLineCook quickMainLineCookCheck(const QString& sfen, const QStringList& pv, int perMoveBudgetMs)
{
    const int n = static_cast<int>(pv.size());
    if (n < 3 || n > 61 || n % 2 == 0) return {};
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return {};
    const std::atomic_bool stop{false};

    for (int i = 0; i < n - 1; ++i) {
        if (i % 2 == 0) {
            const int remaining = n - i;
            const QString& mainMove = pv.at(i);
            shogi::TsumeSearch search;
            const int depth = std::min(63, remaining);
            for (const auto& move : pos.generate_checking_moves()) {
                const QString usi = QString::fromStdString(pos.move_to_usi(move));
                if (usi == mainMove) continue;
                shogi::Position child = pos;
                child.do_move(move);
                if (!child.has_legal_move()) return {i, mainMove, usi};
                const auto res = search.solve(child, shogi::Color::Black, depth, perMoveBudgetMs, stop, 1);
                if (res.status == shogi::TsumeStatus::Mate) return {i, mainMove, usi};
            }
        }
        if (!pos.apply_usi_move(pv.at(i).toStdString())) break;
    }
    return {};
}

Outcome screenCandidate(const QString& sfen, int targetMoves, int minDesiredPlies,
                        int maxPliesReached, bool fromRepair, int stepTimeoutMs,
                        std::mt19937_64& rng, MateAnswer* solvedOut)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return Outcome::Reject;
    if (pos.side_to_move() != shogi::Color::Black || pos.is_in_check(shogi::Color::White)) {
        return Outcome::Reject;
    }

    // 親が高手数（例: 13手以上から15手狙い）の展開由来のときだけ、浅すぎる早詰ゴミを落とす
    const bool hasHighFrontier = (maxPliesReached >= std::max(3, targetMoves - 2));
    const bool dropShallow = (minDesiredPlies >= 9 && minDesiredPlies >= targetMoves - 2);
    const std::atomic_bool stop{false};
    shogi::TsumeSearch search;

    if (targetMoves <= 19) {
        const int baseBudget = (targetMoves <= 9) ? 5 : (targetMoves <= 15 ? 5 : (targetMoves <= 17 ? 9 : 15));
        const int budgetMs = std::clamp(baseBudget, 2, std::max(2, stepTimeoutMs));
        auto res = search.solve(pos, shogi::Color::Black, targetMoves, budgetMs, stop, 1);

        if (res.status == shogi::TsumeStatus::NoMate) {
            return Outcome::Reject;
        }
        if (res.status == shogi::TsumeStatus::Limit) {
            // targetMoves 以内では確実に不詰。15〜17手は既に親種があるなら外部へ送らず即次候補へ
            if (hasHighFrontier && targetMoves <= 17) return Outcome::Reject;
            const int deferPct = (dropShallow ? 4 : 15);
            std::uniform_int_distribution<int> pct(0, 99);
            return pct(rng) < deferPct ? Outcome::DeferToEngine : Outcome::Reject;
        }
        if (res.status == shogi::TsumeStatus::Mate) {
            const int plies = res.plies;
            if (dropShallow && plies < targetMoves - 2) {
                return Outcome::Reject;
            }
            // PV を抽出（置換表がすでに埋まっているのでほぼ0ms）
            QStringList pv;
            pv.reserve(plies);
            shogi::Position cur = pos;
            int rem = plies;
            while (rem > 0) {
                if (res.status != shogi::TsumeStatus::Mate || !res.move.is_valid()) break;
                pv.append(QString::fromStdString(cur.move_to_usi(res.move)));
                if (!cur.do_move(res.move)) break;
                --rem;
                if (rem > 0) {
                    res = search.solve(cur, shogi::Color::Black, rem, std::max(10, budgetMs), stop, 1);
                }
            }
            if (pv.size() == plies) {
                if (solvedOut) *solvedOut = MateAnswer{MateAnswer::Kind::Mate, pv};
                return Outcome::Solved;
            }
            return Outcome::Reject;
        }
        // Timeout：15〜17手で既に親種があるときは重い泥沼候補を外部エンジンに送らず即捨てる
        if (hasHighFrontier && targetMoves <= 17) {
            return Outcome::Reject;
        }
        if (!hasHighFrontier && fromRepair) return Outcome::DeferToEngine;
        const int deferPct = (targetMoves <= 17) ? 10 : 20;
        std::uniform_int_distribution<int> pct(0, 99);
        return pct(rng) < deferPct ? Outcome::DeferToEngine : Outcome::Reject;
    }

    // 21手以上：15手以下の早詰と確定不詰を 4ms で弾き、真の深手数候補だけを外部エンジンへ送る
    const int earlyDepth = std::min(15, std::max(3, targetMoves - 4));
    const int budgetMs = std::clamp(4, 1, std::max(1, stepTimeoutMs));
    auto res = search.solve(pos, shogi::Color::Black, earlyDepth, budgetMs, stop, 1);
    if (res.status == shogi::TsumeStatus::NoMate) {
        return Outcome::Reject;
    }
    if (res.status == shogi::TsumeStatus::Mate) {
        if (dropShallow && res.plies < targetMoves - 2) {
            return Outcome::Reject;
        }
        // ボトムアップ成長中の足場なら PV を抽出してそのまま使う
        QStringList pv;
        pv.reserve(res.plies);
        shogi::Position cur = pos;
        int rem = res.plies;
        while (rem > 0) {
            if (res.status != shogi::TsumeStatus::Mate || !res.move.is_valid()) break;
            pv.append(QString::fromStdString(cur.move_to_usi(res.move)));
            if (!cur.do_move(res.move)) break;
            --rem;
            if (rem > 0) {
                res = search.solve(cur, shogi::Color::Black, rem, 10, stop, 1);
            }
        }
        if (pv.size() == res.plies) {
            if (solvedOut) *solvedOut = MateAnswer{MateAnswer::Kind::Mate, pv};
            return Outcome::Solved;
        }
        return Outcome::Reject;
    }
    if (res.status == shogi::TsumeStatus::Timeout) {
        // 15手すら 4ms で読み切れない過密泥沼局面は間引いて外部エンジンの詰まりを防ぐ
        std::uniform_int_distribution<int> pct(0, 99);
        return pct(rng) < 35 ? Outcome::DeferToEngine : Outcome::Reject;
    }
    // Limit（earlyDepth以内では確実に詰まない）＝真の長手数候補！外部エンジンで判定する
    return Outcome::DeferToEngine;
}

} // namespace TsumeLocalScreen
