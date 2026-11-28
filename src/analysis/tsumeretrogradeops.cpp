/// @file tsumeretrogradeops.cpp
/// @brief 詰み局面からの逆算による候補局面の生成の実装

#include "tsumeretrogradeops.h"

#include "tsumeeditboard.h"
#include "tsumeeditrandom.h"
#include "tsumelinemetrics.h"

#include <position.h>
#include <types.h>

#include <QSet>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace TsumeRetrogradeOps {
namespace {

using shogi::Color;
using shogi::PieceType;
using TsumeEditBoard::State;
using TsumeEditBoard::kBoardSquares;
using TsumeEditBoard::buildSfen;
using TsumeEditBoard::canPromoteMove;
using TsumeEditBoard::checkingPieceTypesFor;
using TsumeEditBoard::findCheckers;
using TsumeEditBoard::findWhiteKing;
using TsumeEditBoard::isSlider;
using TsumeEditBoard::loadStateFromSfen;
using TsumeEditBoard::makeDropUsi;
using TsumeEditBoard::makeMoveUsi;
using TsumeEditBoard::pieceReaches;
using TsumeEditBoard::squaresBetween;
using TsumeEditRandom::randChance;
using TsumeEditRandom::randInt;
using TsumeEditRandom::randomUsefulPiece;
using TsumeEditRandom::shuffleList;

/// 盤面が完全一致しているか（手番・盤・両者持駒）
bool samePosition(const shogi::Position& a, const shogi::Position& b)
{
    if (a.side_to_move() != b.side_to_move()) return false;
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        if (a.piece_at(sq) != b.piece_at(sq)) return false;
    }
    for (int kind = 1; kind <= 7; ++kind) {
        const auto pt = static_cast<PieceType>(kind);
        if (a.hand_count(Color::Black, pt) != b.hand_count(Color::Black, pt)) return false;
        if (a.hand_count(Color::White, pt) != b.hand_count(Color::White, pt)) return false;
    }
    return true;
}

/// 逆算で得た攻方局面 candidateSfen から、順方向に attackUsi（および任意で defenseUsi）を
/// 適用して targetPos と厳密一致し、途中の王手条件も満たすことを完全検証する。
bool verifyForwardRoundtrip(const QString& candidateSfen, const QString& attackUsi,
                            const QString& defenseUsi, const shogi::Position& targetPos)
{
    shogi::Position pos;
    if (!pos.set_sfen(candidateSfen.toStdString(), true)) return false;
    if (pos.side_to_move() != Color::Black) return false;
    if (pos.find_king(Color::White) < 0 || pos.is_in_check(Color::White)) return false;

    if (!pos.apply_usi_move(attackUsi.toStdString())) return false;
    if (!pos.is_in_check(Color::White)) return false;

    if (!defenseUsi.isEmpty()) {
        if (!pos.apply_usi_move(defenseUsi.toStdString())) return false;
        if (pos.is_in_check(Color::White)) return false;
    }
    return samePosition(pos, targetPos);
}

/// 玉方手番・王手中の局面 checkedState から攻方の直前王手1手を逆算し、
/// 合法性が検証された候補を out に追加する。
void unwindAttackCheck(const State& checkedState, const shogi::Position& targetPos,
                       const QString& defenseUsi, const QString& baseTag,
                       const Limits& limits, int maxOut, std::mt19937_64& rng,
                       QSet<QString>& seen, QList<Candidate>& out)
{
    if (out.size() >= maxOut) return;
    const int kingSq = findWhiteKing(checkedState);
    if (kingSq < 0) return;

    const std::vector<int> checkers = findCheckers(checkedState, kingSq);
    if (checkers.empty() || checkers.size() > 2) return;

    auto tryEmit = [&](const State& prevState, const QString& attackUsi, const QString& tag) {
        if (out.size() >= maxOut) return;
        const QString sfen = buildSfen(prevState, Color::Black, limits);
        if (sfen.isEmpty() || seen.contains(sfen)) return;
        if (!verifyForwardRoundtrip(sfen, attackUsi, defenseUsi, targetPos)) return;
        seen.insert(sfen);
        out.append(Candidate{sfen, tag, attackUsi});
    };

    // 攻方の捕獲戻し用：攻方持駒にある生駒種の一覧
    std::vector<int> heldKinds;
    for (int k = 1; k <= 7; ++k) {
        if (checkedState.attackHand[static_cast<std::size_t>(k)] > 0) heldKinds.push_back(k);
    }

    // 移動先 dst から盤上移動で戻す共通ヘルパ（capture 復活も含む）
    auto unwindPieceMove = [&](int dst, const QString& tagPrefix, bool mustBeSliderBlocker,
                               int sliderSq) {
        const int movedCode = checkedState.board[static_cast<std::size_t>(dst)];
        if (movedCode <= 0) return;
        const PieceType landedType = shogi::piece_type(movedCode);
        std::vector<std::pair<PieceType, bool>> originVariants;
        originVariants.push_back({landedType, false});
        if (shogi::is_promoted(landedType)) {
            originVariants.push_back({shogi::unpromote(landedType), true});
        }

        std::vector<int> candidateFroms;
        if (mustBeSliderBlocker) {
            // 開き王手／両王手：飛び駒 sliderSq と玉 kingSq の間の空きマスに戻す
            for (const int mid : squaresBetween(sliderSq, kingSq)) {
                if (checkedState.board[static_cast<std::size_t>(mid)] == 0) candidateFroms.push_back(mid);
            }
        } else {
            for (int sq = 0; sq < kBoardSquares; ++sq) {
                if (sq != dst && checkedState.board[static_cast<std::size_t>(sq)] == 0) {
                    candidateFroms.push_back(sq);
                }
            }
        }
        shuffleList(candidateFroms, rng);

        State base = checkedState;
        base.board[static_cast<std::size_t>(dst)] = 0;

        int triedForPiece = 0;
        for (const int from : candidateFroms) {
            if (out.size() >= maxOut || triedForPiece >= 12) break;
            for (const auto& [origType, wasPromoted] : originVariants) {
                if (wasPromoted && !canPromoteMove(Color::Black, origType, from, dst)) continue;
                if (shogi::must_promote(origType, Color::Black, shogi::square_row(from))) continue;
                if (!pieceReaches(Color::Black, origType, from, dst, base)) continue;

                ++triedForPiece;
                const QString attackUsi = makeMoveUsi(from, dst, wasPromoted);

                // (1) 非捕獲移動の戻し
                State nonCap = base;
                nonCap.board[static_cast<std::size_t>(from)] = shogi::encode_piece(Color::Black, origType);
                tryEmit(nonCap, attackUsi, tagPrefix + QStringLiteral("_move"));

                // (2) 捕獲を伴う移動の戻し（攻方持駒を1枚玉方駒として dst へ戻す）
                if (!heldKinds.empty()) {
                    std::vector<int> capOrder = heldKinds;
                    shuffleList(capOrder, rng);
                    for (std::size_t ci = 0; ci < capOrder.size() && ci < 2; ++ci) {
                        const int capKind = capOrder[ci];
                        const auto capPt = static_cast<PieceType>(capKind);
                        State capState = nonCap;
                        --capState.attackHand[static_cast<std::size_t>(capKind)];

                        // 生駒として戻す
                        if (!shogi::must_promote(capPt, Color::White, shogi::square_row(dst))) {
                            capState.board[static_cast<std::size_t>(dst)] = shogi::encode_piece(Color::White, capPt);
                            tryEmit(capState, attackUsi, tagPrefix + QStringLiteral("_cap"));
                        }
                        // 成駒として戻す（玉方の成駒を取ったケース）
                        if (shogi::can_promote(capPt) && randChance(rng, 35)) {
                            capState.board[static_cast<std::size_t>(dst)] =
                                shogi::encode_piece(Color::White, shogi::promote(capPt));
                            tryEmit(capState, attackUsi, tagPrefix + QStringLiteral("_cap_pro"));
                        }
                    }
                }
            }
        }
    };

    if (checkers.size() == 2) {
        // 両王手：チェッカーのうち飛び駒でない方（両方飛び駒なら両方試す）を、もう一方の飛び駒の線上に戻す
        for (int idx = 0; idx < 2; ++idx) {
            const int sliderSq = checkers[static_cast<std::size_t>(idx)];
            const int moverSq = checkers[static_cast<std::size_t>(1 - idx)];
            const PieceType sliderType = shogi::piece_type(checkedState.board[static_cast<std::size_t>(sliderSq)]);
            if (!isSlider(sliderType)) continue;
            if (squaresBetween(sliderSq, kingSq).empty()) continue;
            unwindPieceMove(moverSq, baseTag + QStringLiteral("_double"), true, sliderSq);
        }
        return;
    }

    // 単一王手
    const int checkerSq = checkers[0];
    const int checkerCode = checkedState.board[static_cast<std::size_t>(checkerSq)];
    const PieceType checkerType = shogi::piece_type(checkerCode);

    // 1. 駒打ちの戻し（生駒のみ）
    if (!shogi::is_promoted(checkerType) && checkerType != PieceType::King) {
        State dropPrev = checkedState;
        dropPrev.board[static_cast<std::size_t>(checkerSq)] = 0;
        ++dropPrev.attackHand[static_cast<std::size_t>(shogi::hand_index(checkerType))];
        tryEmit(dropPrev, makeDropUsi(checkerType, checkerSq), baseTag + QStringLiteral("_drop"));
    }

    // 2. チェッカー自身の盤上移動の戻し
    unwindPieceMove(checkerSq, baseTag, false, -1);

    // 3. 開き王手（空き王手）の戻し：チェッカーが飛び駒で玉との間に空きマスがあるとき、別の攻方駒をそこに戻す
    if (isSlider(checkerType) && !squaresBetween(checkerSq, kingSq).empty()) {
        std::vector<int> otherAttackers;
        for (int sq = 0; sq < kBoardSquares; ++sq) {
            if (sq != checkerSq && checkedState.board[static_cast<std::size_t>(sq)] > 0) {
                otherAttackers.push_back(sq);
            }
        }
        shuffleList(otherAttackers, rng);
        for (std::size_t i = 0; i < otherAttackers.size() && i < 4; ++i) {
            unwindPieceMove(otherAttackers[i], baseTag + QStringLiteral("_disc"), true, checkerSq);
        }
    }
}

} // namespace

bool isMateEnding(const QString& sfen)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return false;
    if (pos.side_to_move() != Color::White) return false;
    if (pos.find_king(Color::Black) >= 0 || pos.find_king(Color::White) < 0) return false;
    return pos.is_in_check(Color::White) && !pos.has_legal_move();
}

QString normalizeProblemSfen(const QString& sfen, bool addRemainingToDefenderHand)
{
    State state;
    shogi::Position pos;
    if (!loadStateFromSfen(sfen, state, &pos)) return {};
    Limits unlimited;
    unlimited.maxAttackPieces = 40;
    unlimited.maxDefendPieces = 40;
    unlimited.maxTotalBoardPieces = 40;
    unlimited.addRemainingToDefenderHand = addRemainingToDefenderHand;
    const QString normalized = buildSfen(state, Color::Black, unlimited, false, true);
    if (normalized.isEmpty()) return {};
    shogi::Position checkPos;
    if (!checkPos.set_sfen(normalized.toStdString(), true)) return {};
    if (checkPos.is_in_check(Color::White)) return {};
    return normalized;
}

QStringList generateMateEndings(const Limits& limits, int count, std::mt19937_64& rng)
{
    QStringList results;
    QSet<QString> seen;
    const int maxAttempts = std::max(count * 80, 200);

    for (int attempt = 0; attempt < maxAttempts && results.size() < count; ++attempt) {
        State state;
        // 玉位置：盤端・隅（約55%）と中段・中央寄り（約45%）を混ぜる
        int kingRow = 0;
        int kingCol = 0;
        if (randChance(rng, 55)) {
            const int edge = randInt(rng, 0, 3);
            if (edge == 0) {
                kingRow = randInt(rng, 0, 1);
                kingCol = randInt(rng, 0, 8);
            } else if (edge == 1) {
                kingRow = randInt(rng, 7, 8);
                kingCol = randInt(rng, 0, 8);
            } else if (edge == 2) {
                kingRow = randInt(rng, 0, 8);
                kingCol = randInt(rng, 0, 1);
            } else {
                kingRow = randInt(rng, 0, 8);
                kingCol = randInt(rng, 7, 8);
            }
        } else {
            kingRow = randInt(rng, 1, 7);
            kingCol = randInt(rng, 1, 7);
        }
        const int kingSq = shogi::make_square(kingRow, kingCol);
        state.board[static_cast<std::size_t>(kingSq)] = shogi::encode_piece(Color::White, PieceType::King);

        // 1. 王手をかける主力攻め駒を1枚配置
        std::vector<std::pair<int, PieceType>> checkerPlacements;
        for (int dr = -2; dr <= 2; ++dr) {
            for (int dc = -2; dc <= 2; ++dc) {
                if (dr == 0 && dc == 0) continue;
                const int r = kingRow + dr;
                const int c = kingCol + dc;
                if (!shogi::is_on_board(r, c)) continue;
                const int sq = shogi::make_square(r, c);
                for (const PieceType pt : checkingPieceTypesFor(sq, kingSq, state)) {
                    checkerPlacements.push_back({sq, pt});
                }
            }
        }
        if (checkerPlacements.empty()) continue;
        const auto chosenChecker = checkerPlacements[static_cast<std::size_t>(
            randInt(rng, 0, static_cast<int>(checkerPlacements.size()) - 1))];
        state.board[static_cast<std::size_t>(chosenChecker.first)] =
            shogi::encode_piece(Color::Black, chosenChecker.second);

        // 2. 王手駒を支える・玉の逃げ道を塞ぐ攻め駒を 1〜4 枚配置
        const int extraAttackers = randInt(rng, 1, std::min(4, std::max(1, limits.maxAttackPieces - 1)));
        for (int i = 0; i < extraAttackers; ++i) {
            for (int tries = 0; tries < 20; ++tries) {
                const int r = kingRow + randInt(rng, -2, 2);
                const int c = kingCol + randInt(rng, -2, 2);
                if (!shogi::is_on_board(r, c)) continue;
                const int sq = shogi::make_square(r, c);
                if (state.board[static_cast<std::size_t>(sq)] != 0) continue;
                PieceType pt = randomUsefulPiece(rng);
                if (shogi::can_promote(pt) && (shogi::must_promote(pt, Color::Black, r) || randChance(rng, 30))) {
                    pt = shogi::promote(pt);
                }
                if (shogi::must_promote(pt, Color::Black, r)) continue;
                state.board[static_cast<std::size_t>(sq)] = shogi::encode_piece(Color::Black, pt);
                break;
            }
        }

        // 3. 玉の周囲に残った逃げマスの一部を玉方壁駒で埋める（自然な雪隠・囲い収束を作る）
        const int maxWall = std::min(4, limits.maxDefendPieces);
        const int walls = maxWall > 0 ? randInt(rng, 0, maxWall) : 0;
        std::vector<int> adjEmpty;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                const int r = kingRow + dr;
                const int c = kingCol + dc;
                if (!shogi::is_on_board(r, c)) continue;
                const int sq = shogi::make_square(r, c);
                if (state.board[static_cast<std::size_t>(sq)] == 0) adjEmpty.push_back(sq);
            }
        }
        shuffleList(adjEmpty, rng);
        for (int w = 0; w < walls && static_cast<std::size_t>(w) < adjEmpty.size(); ++w) {
            const int sq = adjEmpty[static_cast<std::size_t>(w)];
            const int r = shogi::square_row(sq);
            static constexpr PieceType kWallPieces[] = {
                PieceType::Pawn, PieceType::Lance, PieceType::Knight, PieceType::Silver, PieceType::Gold
            };
            PieceType pt = kWallPieces[static_cast<std::size_t>(
                randInt(rng, 0, static_cast<int>(std::size(kWallPieces)) - 1))];
            if (shogi::must_promote(pt, Color::White, r)) {
                if (shogi::can_promote(pt)) pt = shogi::promote(pt);
                else continue;
            }
            state.board[static_cast<std::size_t>(sq)] = shogi::encode_piece(Color::White, pt);
        }

        // 攻方持駒は常にゼロ（駒余りなし収束を構造上保証）
        const QString sfen = buildSfen(state, Color::White, limits);
        if (sfen.isEmpty() || seen.contains(sfen)) continue;
        if (!isMateEnding(sfen)) continue;
        // 1手逆算でき、かつ最終手が一意な1手詰へ戻せる収束形を優先採用する
        const auto reversed = reverseFromMateEnding(sfen, limits, 6, rng);
        if (reversed.isEmpty()) continue;
        bool hasUniqueOnePly = false;
        for (const auto& cand : reversed) {
            if (TsumeLineMetrics::countImmediateMates(cand.sfen) == 1) {
                hasUniqueOnePly = true;
                break;
            }
        }
        if (!hasUniqueOnePly && attempt < maxAttempts / 2) continue;

        seen.insert(sfen);
        results.append(sfen);
    }
    return results;
}

QList<Candidate> reverseFromMateEnding(const QString& mateEndingSfen, const Limits& limits,
                                       int maxCandidates, std::mt19937_64& rng)
{
    QList<Candidate> out;
    if (maxCandidates <= 0) return out;
    State state;
    shogi::Position targetPos;
    if (!loadStateFromSfen(mateEndingSfen, state, &targetPos)) return out;
    if (targetPos.side_to_move() != Color::White || !targetPos.is_in_check(Color::White)
        || targetPos.has_legal_move()) {
        return out;
    }

    QSet<QString> seen;
    unwindAttackCheck(state, targetPos, QString(), QStringLiteral("ending"), limits,
                      maxCandidates, rng, seen, out);
    return out;
}

QList<Candidate> generatePredecessors(const QString& sfen, const Limits& limits,
                                      int maxCandidates, std::mt19937_64& rng)
{
    QList<Candidate> out;
    if (maxCandidates <= 0) return out;
    State current;
    shogi::Position targetPos;
    if (!loadStateFromSfen(sfen, current, &targetPos)) return out;
    if (targetPos.side_to_move() != Color::Black || targetPos.is_in_check(Color::White)) return out;

    const int kingSq = findWhiteKing(current);
    if (kingSq < 0) return out;

    struct DefenseUnwind {
        State checkedState;
        QString defenseUsi;
        QString tag;
    };
    std::vector<DefenseUnwind> defenseUnwinds;

    // --- 1. 玉の移動（単純移動 ＋ 玉による攻方駒取り＝捨駒の逆算） ---
    const int kr = shogi::square_row(kingSq);
    const int kc = shogi::square_col(kingSq);
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            const int pr = kr + dr;
            const int pc = kc + dc;
            if (!shogi::is_on_board(pr, pc)) continue;
            const int prevKingSq = shogi::make_square(pr, pc);
            if (current.board[static_cast<std::size_t>(prevKingSq)] != 0) continue;

            const QString defUsi = makeMoveUsi(prevKingSq, kingSq, false);

            // 1a. 玉の単純移動（元位置で王手がかかっていた）
            State escapeState = current;
            escapeState.board[static_cast<std::size_t>(kingSq)] = 0;
            escapeState.board[static_cast<std::size_t>(prevKingSq)] =
                shogi::encode_piece(Color::White, PieceType::King);
            if (!findCheckers(escapeState, prevKingSq).empty()) {
                defenseUnwinds.push_back({escapeState, defUsi, QStringLiteral("retro_escape")});
            }

            // 1b. 玉による駒取り（捨駒の逆算）：kingSq に攻め駒を復活させる
            // 復活駒が prevKingSq に王手をかける形（焦点への捨駒）を優先的に生成
            std::vector<PieceType> sacTypes = checkingPieceTypesFor(kingSq, prevKingSq, escapeState);
            shuffleList(sacTypes, rng);
            for (std::size_t i = 0; i < sacTypes.size() && i < 4; ++i) {
                State sacState = escapeState;
                sacState.board[static_cast<std::size_t>(kingSq)] =
                    shogi::encode_piece(Color::Black, sacTypes[i]);
                // 復活駒により王手数は1〜2個になる（3個以上は無効）
                const auto checkers = findCheckers(sacState, prevKingSq);
                if (!checkers.empty() && checkers.size() <= 2) {
                    defenseUnwinds.push_back({sacState, defUsi, QStringLiteral("retro_kingsac")});
                }
            }
        }
    }

    // --- 2. 玉方の合駒打ちの逆算 ---
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = current.board[static_cast<std::size_t>(sq)];
        if (piece >= 0) continue;
        const PieceType pt = shogi::piece_type(piece);
        if (pt == PieceType::King || shogi::is_promoted(pt)) continue;

        State noInterpose = current;
        noInterpose.board[static_cast<std::size_t>(sq)] = 0;
        const auto checkers = findCheckers(noInterpose, kingSq);
        if (!checkers.empty() && checkers.size() <= 2) {
            defenseUnwinds.push_back({noInterpose, makeDropUsi(pt, sq), QStringLiteral("retro_dropint")});
        }
    }

    // --- 3. 玉方盤上駒の移動（移動合 ＋ 玉方駒による攻め駒の取り返し＝捨駒の逆算） ---
    std::vector<int> defenderSquares;
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = current.board[static_cast<std::size_t>(sq)];
        if (piece < 0 && shogi::piece_type(piece) != PieceType::King) {
            defenderSquares.push_back(sq);
        }
    }
    shuffleList(defenderSquares, rng);

    for (const int dstSq : defenderSquares) {
        const int defCode = current.board[static_cast<std::size_t>(dstSq)];
        const PieceType landedType = shogi::piece_type(defCode);
        std::vector<std::pair<PieceType, bool>> defOrigins;
        defOrigins.push_back({landedType, false});
        if (shogi::is_promoted(landedType)) {
            defOrigins.push_back({shogi::unpromote(landedType), true});
        }

        State cleared = current;
        cleared.board[static_cast<std::size_t>(dstSq)] = 0;

        // 移動元の候補マスを集める
        std::vector<int> emptySquares;
        for (int sq = 0; sq < kBoardSquares; ++sq) {
            if (sq != dstSq && cleared.board[static_cast<std::size_t>(sq)] == 0) emptySquares.push_back(sq);
        }
        shuffleList(emptySquares, rng);

        int addedForDef = 0;
        for (const int srcSq : emptySquares) {
            if (addedForDef >= 6) break;
            for (const auto& [origType, wasPromoted] : defOrigins) {
                if (wasPromoted && !canPromoteMove(Color::White, origType, srcSq, dstSq)) continue;
                if (shogi::must_promote(origType, Color::White, shogi::square_row(srcSq))) continue;
                if (!pieceReaches(Color::White, origType, srcSq, dstSq, cleared)) continue;

                State movedBack = cleared;
                movedBack.board[static_cast<std::size_t>(srcSq)] =
                    shogi::encode_piece(Color::White, origType);
                const QString defUsi = makeMoveUsi(srcSq, dstSq, wasPromoted);

                // 3a. 移動合の逆算（dstSq が空いて王手が通る）
                const auto checkers = findCheckers(movedBack, kingSq);
                if (!checkers.empty() && checkers.size() <= 2) {
                    defenseUnwinds.push_back({movedBack, defUsi, QStringLiteral("retro_moveint")});
                    ++addedForDef;
                }

                // 3b. 玉方駒による攻め駒の取り返し（捨駒の逆算）：dstSq に王手する攻め駒を復活
                std::vector<PieceType> sacTypes = checkingPieceTypesFor(dstSq, kingSq, movedBack);
                shuffleList(sacTypes, rng);
                for (std::size_t si = 0; si < sacTypes.size() && si < 2; ++si) {
                    State recapState = movedBack;
                    recapState.board[static_cast<std::size_t>(dstSq)] =
                        shogi::encode_piece(Color::Black, sacTypes[si]);
                    const auto recapCheckers = findCheckers(recapState, kingSq);
                    if (!recapCheckers.empty() && recapCheckers.size() <= 2) {
                        defenseUnwinds.push_back({recapState, defUsi, QStringLiteral("retro_recap")});
                        ++addedForDef;
                    }
                }
            }
        }
    }

    if (defenseUnwinds.empty()) return out;
    shuffleList(defenseUnwinds, rng);

    QSet<QString> seen;
    seen.insert(sfen);
    // 各応手逆算からバランスよく攻方王手を逆算して多様性を確保する
    const int perDefenseCap = std::max(2, (maxCandidates + static_cast<int>(defenseUnwinds.size()) - 1)
                                          / static_cast<int>(defenseUnwinds.size()));
    for (const auto& du : defenseUnwinds) {
        if (out.size() >= maxCandidates) break;
        const int targetSize = std::min(maxCandidates, static_cast<int>(out.size()) + perDefenseCap);
        unwindAttackCheck(du.checkedState, targetPos, du.defenseUsi, du.tag, limits,
                          targetSize, rng, seen, out);
    }
    // 枠が余っていたら残りの応手逆算からも補充する
    if (out.size() < maxCandidates) {
        for (const auto& du : defenseUnwinds) {
            if (out.size() >= maxCandidates) break;
            unwindAttackCheck(du.checkedState, targetPos, du.defenseUsi, du.tag, limits,
                              maxCandidates, rng, seen, out);
        }
    }
    return out;
}

std::optional<SuffixSlice> sliceToTargetLength(const QString& sfen, const QStringList& pv, int targetMoves)
{
    if (targetMoves < 1 || targetMoves % 2 == 0) return std::nullopt;
    const int pvSize = static_cast<int>(pv.size());
    if (pvSize < targetMoves || pvSize % 2 == 0) return std::nullopt;
    const int skip = pvSize - targetMoves;
    if (skip % 2 != 0) return std::nullopt;

    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return std::nullopt;
    if (pos.side_to_move() != Color::Black || pos.is_in_check(Color::White)) return std::nullopt;

    for (int i = 0; i < skip; ++i) {
        const bool isAttack = (i % 2 == 0);
        if (!pos.apply_usi_move(pv.at(i).toStdString())) return std::nullopt;
        if (isAttack && !pos.is_in_check(Color::White)) return std::nullopt;
        if (!isAttack && pos.is_in_check(Color::White)) return std::nullopt;
    }

    const QString slicedSfen = normalizeProblemSfen(QString::fromStdString(pos.to_sfen()), true);
    if (slicedSfen.isEmpty()) return std::nullopt;
    const QStringList slicedPv = pv.mid(skip);
    if (!TsumeLineMetrics::analyzeLine(slicedSfen, slicedPv)) return std::nullopt;
    return SuffixSlice{slicedSfen, slicedPv};
}

} // namespace TsumeRetrogradeOps
