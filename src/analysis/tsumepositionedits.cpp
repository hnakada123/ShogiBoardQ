/// @file tsumepositionedits.cpp
/// @brief 詰将棋の候補局面の盤面編集と余詰リペアの実装

#include "tsumepositionedits.h"

#include "tsumecandidatejob.h"
#include "tsumeeditboard.h"
#include "tsumeeditrandom.h"

#include <position.h>
#include <types.h>

#include <QSet>

#include <algorithm>
#include <vector>

namespace TsumePositionEdits {
namespace {

using shogi::Color;
using shogi::PieceType;
using TsumeEditBoard::State;
using TsumeEditBoard::kBoardSquares;
using TsumeEditBoard::buildSfen;
using TsumeEditBoard::findWhiteKing;
using TsumeEditBoard::isSlider;
using TsumeEditBoard::loadStateFromSfen;
using TsumeEditBoard::pieceReaches;
using TsumeEditBoard::squaresBetween;
using TsumeEditRandom::randChance;
using TsumeEditRandom::randInt;
using TsumeEditRandom::randomUsefulPiece;
using TsumeEditRandom::shuffleList;

} // namespace

QList<Candidate> generateMutations(const QString& sfen, const Limits& limits,
                                   int maxCandidates, std::mt19937_64& rng)
{
    QList<Candidate> out;
    if (maxCandidates <= 0) return out;
    State base;
    if (!loadStateFromSfen(sfen, base)) return out;
    const int kingSq = findWhiteKing(base);
    if (kingSq < 0) return out;

    QSet<QString> seen;
    seen.insert(sfen);

    auto tryAdd = [&](const State& mutated, const QString& tag) {
        const QString candSfen = buildSfen(mutated, Color::Black, limits);
        if (candSfen.isEmpty() || seen.contains(candSfen)) return false;
        shogi::Position pos;
        if (!pos.set_sfen(candSfen.toStdString(), true) || pos.is_in_check(Color::White)) return false;
        seen.insert(candSfen);
        out.append(Candidate{candSfen, tag, QString()});
        return true;
    };

    const int maxTries = maxCandidates * 12;
    for (int attempt = 0; attempt < maxTries && out.size() < maxCandidates; ++attempt) {
        State mutated = base;
        int curKingSq = kingSq;
        const int edits = randChance(rng, 60) ? 1 : randInt(rng, 2, 3);
        QString lastTag;
        bool anyEdit = false;

        for (int e = 0; e < edits; ++e) {
            std::vector<int> nonKings;
            std::vector<int> occupied;
            for (int sq = 0; sq < kBoardSquares; ++sq) {
                if (mutated.board[static_cast<std::size_t>(sq)] == 0) continue;
                occupied.push_back(sq);
                if (sq != curKingSq) nonKings.push_back(sq);
            }

            const int op = randInt(rng, 0, 99);
            if (op < 38 && !occupied.empty()) {
                // 盤上の駒（玉含む）を近傍（±2マス）の空きマスへ移動
                const int from = occupied[static_cast<std::size_t>(
                    randInt(rng, 0, static_cast<int>(occupied.size()) - 1))];
                const int r = shogi::square_row(from) + randInt(rng, -2, 2);
                const int c = shogi::square_col(from) + randInt(rng, -2, 2);
                if (!shogi::is_on_board(r, c)) continue;
                const int to = shogi::make_square(r, c);
                if (mutated.board[static_cast<std::size_t>(to)] != 0) continue;
                int piece = mutated.board[static_cast<std::size_t>(from)];
                const PieceType pt = shogi::piece_type(piece);
                const Color color = shogi::piece_color(piece);
                if (shogi::must_promote(pt, color, r)) {
                    if (shogi::can_promote(pt)) piece = shogi::encode_piece(color, shogi::promote(pt));
                    else continue;
                }
                mutated.board[static_cast<std::size_t>(to)] = piece;
                mutated.board[static_cast<std::size_t>(from)] = 0;
                if (from == curKingSq) curKingSq = to;
                lastTag = QStringLiteral("mutate_move");
                anyEdit = true;
            } else if (op < 60) {
                // 盤上に新しい攻め駒または守り駒を追加（玉周辺±3マス、遠方飛び駒なら盤全体）
                const bool isAttack = randChance(rng, 55);
                const Color color = isAttack ? Color::Black : Color::White;
                PieceType pt = randomUsefulPiece(rng);
                int r = 0;
                int c = 0;
                if (isAttack && isSlider(pt) && randChance(rng, 45)) {
                    r = randInt(rng, 0, 8);
                    c = randInt(rng, 0, 8);
                } else {
                    r = shogi::square_row(curKingSq) + randInt(rng, -3, 3);
                    c = shogi::square_col(curKingSq) + randInt(rng, -3, 3);
                }
                if (!shogi::is_on_board(r, c)) continue;
                const int to = shogi::make_square(r, c);
                if (mutated.board[static_cast<std::size_t>(to)] != 0) continue;
                if (shogi::can_promote(pt) && (shogi::must_promote(pt, color, r) || randChance(rng, 25))) {
                    pt = shogi::promote(pt);
                }
                if (shogi::must_promote(pt, color, r)) continue;
                mutated.board[static_cast<std::size_t>(to)] = shogi::encode_piece(color, pt);
                lastTag = isAttack ? QStringLiteral("mutate_add_atk") : QStringLiteral("mutate_add_def");
                anyEdit = true;
            } else if (op < 78 && !nonKings.empty()) {
                // 駒種・成不成の変更
                const int sq = nonKings[static_cast<std::size_t>(
                    randInt(rng, 0, static_cast<int>(nonKings.size()) - 1))];
                const Color color = shogi::piece_color(mutated.board[static_cast<std::size_t>(sq)]);
                const int r = shogi::square_row(sq);
                PieceType pt = randomUsefulPiece(rng);
                if (shogi::can_promote(pt) && (shogi::must_promote(pt, color, r) || randChance(rng, 30))) {
                    pt = shogi::promote(pt);
                }
                if (shogi::must_promote(pt, color, r)) continue;
                mutated.board[static_cast<std::size_t>(sq)] = shogi::encode_piece(color, pt);
                lastTag = QStringLiteral("mutate_kind");
                anyEdit = true;
            } else if (op < 90) {
                // 攻方持駒を1枚増減
                const int kind = randInt(rng, 1, 7);
                if (mutated.attackHand[static_cast<std::size_t>(kind)] > 0 && randChance(rng, 55)) {
                    --mutated.attackHand[static_cast<std::size_t>(kind)];
                    lastTag = QStringLiteral("mutate_hand_dec");
                } else {
                    ++mutated.attackHand[static_cast<std::size_t>(kind)];
                    lastTag = QStringLiteral("mutate_hand_inc");
                }
                anyEdit = true;
            } else if (!nonKings.empty()) {
                // 盤上駒を1枚削除（玉方持駒へ戻す）
                const int sq = nonKings[static_cast<std::size_t>(
                    randInt(rng, 0, static_cast<int>(nonKings.size()) - 1))];
                mutated.board[static_cast<std::size_t>(sq)] = 0;
                lastTag = QStringLiteral("mutate_remove");
                anyEdit = true;
            }
        }
        if (!anyEdit) continue;
        tryAdd(mutated, edits >= 2 ? QStringLiteral("mutate_multi") : lastTag);
    }

    return out;
}

QList<Candidate> generateCookRepairs(const QString& sfen, const QString& mainMove,
                                     const QString& cookMove, const Limits& limits,
                                     int maxCandidates, std::mt19937_64& rng)
{
    QList<Candidate> out;
    if (maxCandidates <= 0 || cookMove.isEmpty()) return out;

    State base;
    shogi::Position basePos;
    if (!loadStateFromSfen(sfen, base, &basePos)) return out;
    const int kingSq = findWhiteKing(base);
    if (kingSq < 0) return out;

    // cookMove と mainMove を解析
    auto parseSq = [](const QString& twoChar) -> int {
        const auto sq = shogi::square_from_usi(twoChar.toStdString());
        return sq ? *sq : -1;
    };
    const bool cookIsDrop = cookMove.size() >= 4 && cookMove[1] == QLatin1Char('*');
    const int cookFrom = cookIsDrop ? -1 : parseSq(cookMove.left(2));
    const int cookTo = cookIsDrop ? parseSq(cookMove.mid(2, 2)) : parseSq(cookMove.mid(2, 2));
    const auto cookDropPt = cookIsDrop ? shogi::piece_from_letter(cookMove[0].toLatin1()) : std::nullopt;

    const bool mainIsDrop = mainMove.size() >= 4 && mainMove[1] == QLatin1Char('*');
    const int mainFrom = mainIsDrop ? -1 : (mainMove.size() >= 4 ? parseSq(mainMove.left(2)) : -1);
    const int mainTo = mainMove.size() >= 4 ? parseSq(mainMove.mid(2, 2)) : -1;

    QSet<QString> seen;
    seen.insert(sfen);

    auto tryRepair = [&](const State& repaired, const QString& tag) {
        if (out.size() >= maxCandidates) return;
        const QString candSfen = buildSfen(repaired, Color::Black, limits);
        if (candSfen.isEmpty() || seen.contains(candSfen)) return;
        shogi::Position pos;
        if (!pos.set_sfen(candSfen.toStdString(), true) || pos.is_in_check(Color::White)) return;
        // 主手順が指定されていれば、主手順の初手が依然として合法王手である候補を優先する
        if (!mainMove.isEmpty()) {
            shogi::Position probe = pos;
            if (!probe.apply_usi_move(mainMove.toStdString()) || !probe.is_in_check(Color::White)) return;
        }
        seen.insert(candSfen);
        out.append(Candidate{candSfen, tag, mainMove});
    };

    // 戦略1: 余詰が駒打ちで、主手順と同じマスでなければ cookTo に玉方駒を置いて打ち場所を塞ぐ
    if (cookIsDrop && cookTo >= 0 && cookTo != mainTo && base.board[static_cast<std::size_t>(cookTo)] == 0) {
        static constexpr PieceType kBlockers[] = {
            PieceType::Pawn, PieceType::Knight, PieceType::Silver, PieceType::Gold, PieceType::Lance
        };
        for (const PieceType pt : kBlockers) {
            const int r = shogi::square_row(cookTo);
            if (shogi::must_promote(pt, Color::White, r)) continue;
            State s = base;
            s.board[static_cast<std::size_t>(cookTo)] = shogi::encode_piece(Color::White, pt);
            tryRepair(s, QStringLiteral("repair_occupy_drop"));
        }
        // また、その打駒を攻方持駒から1枚減らす（主手順で同じ駒を打たない場合）
        if (cookDropPt && (!mainIsDrop || mainMove[0] != cookMove[0])) {
            const int kind = shogi::hand_index(*cookDropPt);
            if (kind >= 1 && kind <= 7 && base.attackHand[static_cast<std::size_t>(kind)] > 0) {
                State s = base;
                --s.attackHand[static_cast<std::size_t>(kind)];
                tryRepair(s, QStringLiteral("repair_trim_hand"));
            }
        }
    }

    // 戦略2: cookTo に利く玉方守り駒を空きマスに1枚追加して、余詰の王手駒を取れるようにする
    if (cookTo >= 0) {
        std::vector<std::pair<int, PieceType>> guardCandidates;
        static constexpr PieceType kGuards[] = {
            PieceType::Pawn, PieceType::Silver, PieceType::Gold, PieceType::Knight,
            PieceType::Lance, PieceType::Bishop, PieceType::Rook
        };
        for (int sq = 0; sq < kBoardSquares; ++sq) {
            if (sq == cookTo || sq == mainTo || base.board[static_cast<std::size_t>(sq)] != 0) continue;
            const int r = shogi::square_row(sq);
            for (const PieceType pt : kGuards) {
                if (shogi::must_promote(pt, Color::White, r)) continue;
                if (pieceReaches(Color::White, pt, sq, cookTo, base)) {
                    // mainTo にも利いてしまうと主手順まで壊しやすいので、mainTo へ利かない配置を先に試す
                    guardCandidates.push_back({sq, pt});
                }
            }
        }
        shuffleList(guardCandidates, rng);
        std::stable_partition(guardCandidates.begin(), guardCandidates.end(),
                              [&](const auto& item) {
                                  return mainTo < 0 || !pieceReaches(Color::White, item.second, item.first, mainTo, base);
                              });
        for (std::size_t i = 0; i < guardCandidates.size() && i < 12; ++i) {
            State s = base;
            s.board[static_cast<std::size_t>(guardCandidates[i].first)] =
                shogi::encode_piece(Color::White, guardCandidates[i].second);
            tryRepair(s, QStringLiteral("repair_guard_cook"));
        }
    }

    // 戦略3: 余詰が遠距離移動なら、移動経路上の空きマスに玉方駒を置いて遮断する
    if (!cookIsDrop && cookFrom >= 0 && cookTo >= 0) {
        for (const int mid : squaresBetween(cookFrom, cookTo)) {
            if (mid == mainTo || base.board[static_cast<std::size_t>(mid)] != 0) continue;
            for (const PieceType pt : {PieceType::Pawn, PieceType::Silver, PieceType::Gold}) {
                if (shogi::must_promote(pt, Color::White, shogi::square_row(mid))) continue;
                State s = base;
                s.board[static_cast<std::size_t>(mid)] = shogi::encode_piece(Color::White, pt);
                tryRepair(s, QStringLiteral("repair_block_line"));
            }
        }
    }

    // 戦略4: 余詰で動いた攻方駒が主手順の駒と別なら、除去・弱体化・位置ずらしを試す
    if (!cookIsDrop && cookFrom >= 0 && cookFrom != mainFrom
        && base.board[static_cast<std::size_t>(cookFrom)] > 0) {
        State removed = base;
        removed.board[static_cast<std::size_t>(cookFrom)] = 0;
        tryRepair(removed, QStringLiteral("repair_remove_cook_piece"));

        const PieceType orig = shogi::piece_type(base.board[static_cast<std::size_t>(cookFrom)]);
        if (shogi::is_promoted(orig)) {
            const PieceType demoted = shogi::unpromote(orig);
            if (!shogi::must_promote(demoted, Color::Black, shogi::square_row(cookFrom))) {
                State weakened = base;
                weakened.board[static_cast<std::size_t>(cookFrom)] =
                    shogi::encode_piece(Color::Black, demoted);
                tryRepair(weakened, QStringLiteral("repair_demote_cook_piece"));
            }
        }
    }

    // 戦略5: 玉の周囲にある玉方壁駒を1枚除去して余詰時の逃げ道を開ける
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int r = shogi::square_row(kingSq) + dr;
            const int c = shogi::square_col(kingSq) + dc;
            if (!shogi::is_on_board(r, c)) continue;
            const int sq = shogi::make_square(r, c);
            if (base.board[static_cast<std::size_t>(sq)] < 0 && sq != mainTo) {
                State opened = base;
                opened.board[static_cast<std::size_t>(sq)] = 0;
                tryRepair(opened, QStringLiteral("repair_open_escape"));
            }
        }
    }

    // 戦略6: 汎用的な1枚除去（不要駒が余詰の原因になっているケースを解消）
    QStringList singleRemovals = TsumeCandidateJob::onePieceRemovedPositions(sfen);
    std::vector<QString> remVec(singleRemovals.cbegin(), singleRemovals.cend());
    shuffleList(remVec, rng);
    for (const QString& remSfen : remVec) {
        if (out.size() >= maxCandidates) break;
        if (seen.contains(remSfen)) continue;
        shogi::Position pos;
        if (!pos.set_sfen(remSfen.toStdString(), true) || pos.is_in_check(Color::White)) continue;
        if (!mainMove.isEmpty()) {
            shogi::Position probe = pos;
            if (!probe.apply_usi_move(mainMove.toStdString()) || !probe.is_in_check(Color::White)) continue;
        }
        seen.insert(remSfen);
        out.append(Candidate{remSfen, QStringLiteral("repair_trim_one"), mainMove});
    }

    return out;
}

} // namespace TsumePositionEdits
