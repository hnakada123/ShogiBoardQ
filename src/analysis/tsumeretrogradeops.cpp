/// @file tsumeretrogradeops.cpp
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・長手数向け評価の実装

#include "tsumeretrogradeops.h"

#include "tsumecandidatejob.h"

#include <position.h>
#include <types.h>

#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace TsumeRetrogradeOps {
namespace {

using shogi::Color;
using shogi::PieceType;

constexpr int kBoardSquares = 81;
constexpr std::array<int, 8> kPieceSetMax = {0, 18, 4, 4, 4, 4, 2, 2};
constexpr char kHandLetters[8] = {'?', 'P', 'L', 'N', 'S', 'G', 'B', 'R'};

/// コンパクトな盤面状態（Hayanagi の符号付き駒コード・座標系と完全一致）
struct State {
    std::array<int, kBoardSquares> board{};
    std::array<int, 8> attackHand{}; ///< index 1..7 (Pawn..Rook)
};

int randInt(std::mt19937_64& rng, int lo, int hi)
{
    if (lo >= hi) return lo;
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

bool randChance(std::mt19937_64& rng, int percent)
{
    return randInt(rng, 0, 99) < percent;
}

template <typename T>
void shuffleList(std::vector<T>& items, std::mt19937_64& rng)
{
    std::shuffle(items.begin(), items.end(), rng);
}

/// shogi::Position からコンパクト表現へ読み取る（後手玉が1枚・先手玉0枚を前提）
bool loadState(const shogi::Position& pos, State& out)
{
    out = State{};
    int whiteKings = 0;
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = pos.piece_at(sq);
        if (piece == 0) continue;
        if (piece == shogi::encode_piece(Color::Black, PieceType::King)) return false;
        if (piece == shogi::encode_piece(Color::White, PieceType::King)) ++whiteKings;
        out.board[static_cast<std::size_t>(sq)] = piece;
    }
    if (whiteKings != 1) return false;
    for (int kind = 1; kind <= 7; ++kind) {
        out.attackHand[static_cast<std::size_t>(kind)] =
            pos.hand_count(Color::Black, static_cast<PieceType>(kind));
    }
    return true;
}

bool loadStateFromSfen(const QString& sfen, State& out, shogi::Position* parsed = nullptr)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return false;
    if (!loadState(pos, out)) return false;
    if (parsed) *parsed = pos;
    return true;
}

int findWhiteKing(const State& state)
{
    const int wk = shogi::encode_piece(Color::White, PieceType::King);
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        if (state.board[static_cast<std::size_t>(sq)] == wk) return sq;
    }
    return -1;
}

/// 盤上＋攻方持駒から、各駒種の残り（玉方持駒に回せる枚数）を計算する。枚数超過なら false。
bool computeRemaining(const State& state, std::array<int, 8>& remaining,
                      int* attackPiecesOut = nullptr, int* defendBoardOut = nullptr,
                      int* boardCountOut = nullptr)
{
    remaining = kPieceSetMax;
    int attackPieces = 0;
    int defendBoard = 0;
    int boardCount = 0;
    int whiteKings = 0;

    for (int kind = 1; kind <= 7; ++kind) {
        const int count = state.attackHand[static_cast<std::size_t>(kind)];
        if (count < 0) return false;
        remaining[static_cast<std::size_t>(kind)] -= count;
        if (remaining[static_cast<std::size_t>(kind)] < 0) return false;
        attackPieces += count;
    }

    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = state.board[static_cast<std::size_t>(sq)];
        if (piece == 0) continue;
        ++boardCount;
        const PieceType pt = shogi::piece_type(piece);
        const Color color = shogi::piece_color(piece);
        if (pt == PieceType::King) {
            if (color == Color::Black) return false;
            ++whiteKings;
            continue;
        }
        const int kind = shogi::hand_index(pt);
        if (kind < 1 || kind > 7) return false;
        if (--remaining[static_cast<std::size_t>(kind)] < 0) return false;
        if (color == Color::Black) ++attackPieces;
        else ++defendBoard;
    }

    if (whiteKings != 1) return false;
    if (attackPiecesOut) *attackPiecesOut = attackPieces;
    if (defendBoardOut) *defendBoardOut = defendBoard;
    if (boardCountOut) *boardCountOut = boardCount;
    return true;
}

/// 盤面ルール（二歩・行き所のない駒・駒枚数上限・Limits）を満たせば SFEN を構築する。
QString buildSfen(const State& state, Color sideToMove, const Limits& limits,
                  bool enforceLimits = true, bool requireAttackPiece = true)
{
    std::array<int, 8> remaining{};
    int attackPieces = 0;
    int defendBoard = 0;
    int boardCount = 0;
    if (!computeRemaining(state, remaining, &attackPieces, &defendBoard, &boardCount)) return {};
    if (requireAttackPiece && attackPieces < 1) return {};
    if (enforceLimits) {
        if (attackPieces > limits.maxAttackPieces || defendBoard > limits.maxDefendPieces
            || boardCount > limits.maxTotalBoardPieces) {
            return {};
        }
    }

    bool pawnOnCol[2][9] = {};
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = state.board[static_cast<std::size_t>(sq)];
        if (piece == 0) continue;
        const PieceType pt = shogi::piece_type(piece);
        const Color color = shogi::piece_color(piece);
        const int row = shogi::square_row(sq);
        const int col = shogi::square_col(sq);
        if (shogi::must_promote(pt, color, row)) return {};
        if (pt == PieceType::Pawn) {
            const int ci = color == Color::Black ? 0 : 1;
            if (pawnOnCol[ci][col]) return {};
            pawnOnCol[ci][col] = true;
        }
    }

    QString boardStr;
    boardStr.reserve(96);
    for (int row = 0; row < 9; ++row) {
        if (row > 0) boardStr += QLatin1Char('/');
        int empty = 0;
        for (int col = 0; col < 9; ++col) {
            const int piece = state.board[static_cast<std::size_t>(shogi::make_square(row, col))];
            if (piece == 0) {
                ++empty;
                continue;
            }
            if (empty > 0) {
                boardStr += QString::number(empty);
                empty = 0;
            }
            const PieceType pt = shogi::piece_type(piece);
            const Color color = shogi::piece_color(piece);
            if (shogi::is_promoted(pt)) boardStr += QLatin1Char('+');
            const char base = shogi::piece_letter(pt);
            boardStr += (color == Color::Black)
                ? QLatin1Char(base)
                : QLatin1Char(static_cast<char>(std::tolower(static_cast<unsigned char>(base))));
        }
        if (empty > 0) boardStr += QString::number(empty);
    }

    // 持駒は飛・角・金・銀・桂・香・歩の順（先手→後手）
    static constexpr int kOrder[7] = {7, 6, 5, 4, 3, 2, 1};
    QString handStr;
    for (const int kind : kOrder) {
        const int cnt = state.attackHand[static_cast<std::size_t>(kind)];
        if (cnt <= 0) continue;
        if (cnt > 1) handStr += QString::number(cnt);
        handStr += QLatin1Char(kHandLetters[kind]);
    }
    if (limits.addRemainingToDefenderHand) {
        for (const int kind : kOrder) {
            const int cnt = remaining[static_cast<std::size_t>(kind)];
            if (cnt <= 0) continue;
            if (cnt > 1) handStr += QString::number(cnt);
            handStr += QLatin1Char(static_cast<char>(std::tolower(static_cast<unsigned char>(kHandLetters[kind]))));
        }
    }
    if (handStr.isEmpty()) handStr = QStringLiteral("-");

    return boardStr + (sideToMove == Color::Black ? QStringLiteral(" b ") : QStringLiteral(" w "))
        + handStr + QStringLiteral(" 1");
}

/// color の駒 pt が盤面 state 上で from から to へ移動（利き）できるか
bool pieceReaches(Color color, PieceType pt, int from, int to, const State& state)
{
    if (from == to || from < 0 || from >= kBoardSquares || to < 0 || to >= kBoardSquares) return false;
    const int dx = shogi::square_col(to) - shogi::square_col(from);
    const int dy = shogi::square_row(to) - shogi::square_row(from);
    // fdy < 0 が color にとっての前進方向（Black は row 減少、White は row 増加）
    const int fdy = (color == Color::Black) ? dy : -dy;

    bool sliding = false;
    switch (pt) {
    case PieceType::Pawn:
        return dx == 0 && fdy == -1;
    case PieceType::Lance:
        if (dx == 0 && fdy < 0) sliding = true;
        else return false;
        break;
    case PieceType::Knight:
        return std::abs(dx) == 1 && fdy == -2;
    case PieceType::Silver:
        return (fdy == -1 && std::abs(dx) <= 1) || (fdy == 1 && std::abs(dx) == 1);
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        return (fdy == -1 && std::abs(dx) <= 1) || (fdy == 0 && std::abs(dx) == 1)
            || (fdy == 1 && dx == 0);
    case PieceType::King:
        return std::abs(dx) <= 1 && std::abs(dy) <= 1;
    case PieceType::Horse:
        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) return true;
        [[fallthrough]];
    case PieceType::Bishop:
        if (std::abs(dx) == std::abs(dy)) sliding = true;
        else return false;
        break;
    case PieceType::Dragon:
        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) return true;
        [[fallthrough]];
    case PieceType::Rook:
        if (dx == 0 || dy == 0) sliding = true;
        else return false;
        break;
    case PieceType::Empty:
        return false;
    }

    if (!sliding) return false;
    const int stepCol = (dx > 0) ? 1 : (dx < 0 ? -1 : 0);
    const int stepRow = (dy > 0) ? 1 : (dy < 0 ? -1 : 0);
    int r = shogi::square_row(from) + stepRow;
    int c = shogi::square_col(from) + stepCol;
    const int toR = shogi::square_row(to);
    const int toC = shogi::square_col(to);
    while (r != toR || c != toC) {
        if (state.board[static_cast<std::size_t>(shogi::make_square(r, c))] != 0) return false;
        r += stepRow;
        c += stepCol;
    }
    return true;
}

bool isSlider(PieceType pt)
{
    return pt == PieceType::Lance || pt == PieceType::Bishop || pt == PieceType::Rook
        || pt == PieceType::Horse || pt == PieceType::Dragon;
}

/// from と to が一直線上にあるとき、間のマス一覧を返す（直線上でなければ空）
std::vector<int> squaresBetween(int from, int to)
{
    std::vector<int> out;
    const int dr = shogi::square_row(to) - shogi::square_row(from);
    const int dc = shogi::square_col(to) - shogi::square_col(from);
    if (dr == 0 && dc == 0) return out;
    if (dr != 0 && dc != 0 && std::abs(dr) != std::abs(dc)) return out;
    const int stepR = (dr > 0) ? 1 : (dr < 0 ? -1 : 0);
    const int stepC = (dc > 0) ? 1 : (dc < 0 ? -1 : 0);
    int r = shogi::square_row(from) + stepR;
    int c = shogi::square_col(from) + stepC;
    const int toR = shogi::square_row(to);
    const int toC = shogi::square_col(to);
    while (r != toR || c != toC) {
        out.push_back(shogi::make_square(r, c));
        r += stepR;
        c += stepC;
    }
    return out;
}

/// from -> to の動きが color の成条件を満たすか
bool canPromoteMove(Color color, PieceType unpromoted, int from, int to)
{
    if (!shogi::can_promote(unpromoted)) return false;
    return shogi::is_in_promotion_zone(color, shogi::square_row(from))
        || shogi::is_in_promotion_zone(color, shogi::square_row(to));
}

/// USI の指し手文字列を組み立てる
QString makeMoveUsi(int from, int to, bool promoted)
{
    QString usi = QString::fromStdString(shogi::square_to_usi(from))
        + QString::fromStdString(shogi::square_to_usi(to));
    if (promoted) usi += QLatin1Char('+');
    return usi;
}

QString makeDropUsi(PieceType pt, int to)
{
    return QString(QLatin1Char(shogi::piece_letter(pt))) + QLatin1Char('*')
        + QString::fromStdString(shogi::square_to_usi(to));
}

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

/// State 上で後手玉に王手している攻方駒のマス一覧
std::vector<int> findCheckers(const State& state, int kingSq)
{
    std::vector<int> checkers;
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = state.board[static_cast<std::size_t>(sq)];
        if (piece <= 0) continue;
        if (pieceReaches(Color::Black, shogi::piece_type(piece), sq, kingSq, state)) {
            checkers.push_back(sq);
        }
    }
    return checkers;
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

/// 玉に王手をかけられる攻方駒の種類候補（復活・捨駒逆算用）
std::vector<PieceType> checkingPieceTypesFor(int pieceSq, int kingSq, const State& state)
{
    static constexpr PieceType kCandidates[] = {
        PieceType::Gold, PieceType::Silver, PieceType::Rook, PieceType::Bishop,
        PieceType::Dragon, PieceType::Horse, PieceType::Knight, PieceType::Lance,
        PieceType::Pawn, PieceType::ProPawn, PieceType::ProSilver
    };
    std::vector<PieceType> result;
    const int row = shogi::square_row(pieceSq);
    for (const PieceType pt : kCandidates) {
        if (shogi::must_promote(pt, Color::Black, row)) continue;
        if (pieceReaches(Color::Black, pt, pieceSq, kingSq, state)) result.push_back(pt);
    }
    return result;
}

/// ランダムに駒種（歩〜飛）を選ぶ（詰将棋で使いやすい駒にやや重み付け）
PieceType randomUsefulPiece(std::mt19937_64& rng)
{
    static constexpr PieceType kWeighted[] = {
        PieceType::Gold, PieceType::Gold, PieceType::Gold,
        PieceType::Silver, PieceType::Silver, PieceType::Silver,
        PieceType::Rook, PieceType::Rook,
        PieceType::Bishop, PieceType::Bishop,
        PieceType::Knight, PieceType::Knight,
        PieceType::Lance, PieceType::Lance,
        PieceType::Pawn, PieceType::Pawn
    };
    return kWeighted[static_cast<std::size_t>(randInt(rng, 0, static_cast<int>(std::size(kWeighted)) - 1))];
}

int countImmediateMatesInPos(const shogi::Position& position)
{
    if (position.side_to_move() != Color::Black || position.is_in_check(Color::White)) return 0;
    shogi::Position probe = position;
    std::vector<shogi::Move> checks;
    probe.generate_checking_moves(checks);
    int mates = 0;
    for (const auto& move : checks) {
        shogi::MoveUndo undo;
        probe.make_move(move, undo);
        const bool mated = !probe.has_legal_move();
        probe.unmake_move(move, undo);
        if (mated) ++mates;
    }
    return mates;
}

} // namespace

int countImmediateMates(const QString& sfen)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return 0;
    return countImmediateMatesInPos(pos);
}

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
            if (countImmediateMates(cand.sfen) == 1) {
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
        std::vector<int> nonKings;
        std::vector<int> occupied;
        for (int sq = 0; sq < kBoardSquares; ++sq) {
            if (mutated.board[static_cast<std::size_t>(sq)] == 0) continue;
            occupied.push_back(sq);
            if (sq != kingSq) nonKings.push_back(sq);
        }

        const int op = randInt(rng, 0, 99);
        QString tag;
        if (op < 35 && !occupied.empty()) {
            // 盤上の駒を近傍（±2マス）または盤上空きマスへ移動
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
            tag = QStringLiteral("mutate_move");
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
                r = shogi::square_row(kingSq) + randInt(rng, -3, 3);
                c = shogi::square_col(kingSq) + randInt(rng, -3, 3);
            }
            if (!shogi::is_on_board(r, c)) continue;
            const int to = shogi::make_square(r, c);
            if (mutated.board[static_cast<std::size_t>(to)] != 0) continue;
            if (shogi::can_promote(pt) && (shogi::must_promote(pt, color, r) || randChance(rng, 25))) {
                pt = shogi::promote(pt);
            }
            if (shogi::must_promote(pt, color, r)) continue;
            mutated.board[static_cast<std::size_t>(to)] = shogi::encode_piece(color, pt);
            tag = isAttack ? QStringLiteral("mutate_add_atk") : QStringLiteral("mutate_add_def");
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
            tag = QStringLiteral("mutate_kind");
        } else if (op < 90) {
            // 攻方持駒を1枚増減
            const int kind = randInt(rng, 1, 7);
            if (mutated.attackHand[static_cast<std::size_t>(kind)] > 0 && randChance(rng, 55)) {
                --mutated.attackHand[static_cast<std::size_t>(kind)];
                tag = QStringLiteral("mutate_hand_dec");
            } else {
                ++mutated.attackHand[static_cast<std::size_t>(kind)];
                tag = QStringLiteral("mutate_hand_inc");
            }
        } else if (!nonKings.empty()) {
            // 盤上駒を1枚削除（玉方持駒へ戻す）
            const int sq = nonKings[static_cast<std::size_t>(
                randInt(rng, 0, static_cast<int>(nonKings.size()) - 1))];
            mutated.board[static_cast<std::size_t>(sq)] = 0;
            tag = QStringLiteral("mutate_remove");
        } else {
            continue;
        }
        tryAdd(mutated, tag);
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
    if (!analyzeLine(slicedSfen, slicedPv)) return std::nullopt;
    return SuffixSlice{slicedSfen, slicedPv};
}

std::optional<Metrics> analyzeLine(const QString& sfen, const QStringList& pv)
{
    if (pv.isEmpty() || pv.size() % 2 == 0) return std::nullopt;
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return std::nullopt;
    if (pos.side_to_move() != Color::Black || pos.is_in_check(Color::White)) return std::nullopt;
    const int initialKing = pos.find_king(Color::White);
    if (initialKing < 0 || pos.find_king(Color::Black) >= 0) return std::nullopt;

    Metrics m;
    m.plies = static_cast<int>(pv.size());

    // 開始局面の駒数カウント
    for (int sq = 0; sq < kBoardSquares; ++sq) {
        const int piece = pos.piece_at(sq);
        if (piece > 0) ++m.attackPieces;
        else if (piece < 0 && shogi::piece_type(piece) != PieceType::King) ++m.defendBoardPieces;
    }
    for (int kind = 1; kind <= 7; ++kind) {
        m.attackPieces += pos.hand_count(Color::Black, static_cast<PieceType>(kind));
    }

    QSet<int> kingSquares;
    kingSquares.insert(initialKing);
    int prevKing = initialKing;

    for (int i = 0; i < pv.size(); ++i) {
        if (i == pv.size() - 1) {
            m.finalImmediateMates = countImmediateMatesInPos(pos);
        }
        const QString& moveStr = pv.at(i);
        const bool isAttack = (i % 2 == 0);
        const bool isDrop = moveStr.size() >= 4 && moveStr[1] == QLatin1Char('*');
        const auto toSq = moveStr.size() >= 4 ? shogi::square_from_usi(moveStr.mid(2, 2).toStdString())
                                              : std::nullopt;
        const bool defenderCapturesAttacker = !isAttack && !isDrop && toSq && pos.piece_at(*toSq) > 0;

        if (!pos.apply_usi_move(moveStr.toStdString())) return std::nullopt;

        if (isAttack) {
            if (!pos.is_in_check(Color::White)) return std::nullopt;
        } else {
            if (pos.is_in_check(Color::White)) return std::nullopt;
            if (isDrop) ++m.interpositions;
            if (defenderCapturesAttacker) ++m.sacrifices;
            const int curKing = pos.find_king(Color::White);
            if (curKing != prevKing && curKing >= 0) {
                ++m.kingMoves;
                m.kingDistance += std::abs(shogi::square_row(curKing) - shogi::square_row(prevKing))
                    + std::abs(shogi::square_col(curKing) - shogi::square_col(prevKing));
                kingSquares.insert(curKing);
                prevKing = curKing;
            }
        }
    }

    if (pos.side_to_move() != Color::White || !pos.is_in_check(Color::White) || pos.has_legal_move()) {
        return std::nullopt;
    }

    m.distinctKingSquares = static_cast<int>(kingSquares.size());
    for (int kind = 1; kind <= 7; ++kind) {
        m.surplusCount += pos.hand_count(Color::Black, static_cast<PieceType>(kind));
    }
    return m;
}

double scoreNode(const Metrics& metrics, int targetMoves, int expansionCount,
                 bool firstMoveUnique, bool strictNoSurplus, bool strictFinalMove)
{
    // 1. 手数の進捗が最優先（目標に近づくほど高評価、超過も接尾辞切り出しで有用）
    const int cappedPlies = std::min(metrics.plies, targetMoves);
    double score = static_cast<double>(cappedPlies) * 14.0;

    // 2. 駒余りなしを強く優遇（厳密条件を満たす系統を育てる）
    if (metrics.surplusCount == 0) {
        score += strictNoSurplus ? 35.0 : 12.0;
    } else {
        score -= static_cast<double>(metrics.surplusCount) * (strictNoSurplus ? 22.0 : 6.0);
    }

    // 2b. 最終手一意を強く優遇（最終手余詰の系統を育てても厳密証明で棄却されるため）
    if (strictFinalMove) {
        if (metrics.finalImmediateMates == 1) score += 25.0;
        else if (metrics.finalImmediateMates > 1) score -= 55.0;
    }

    // 3. 初手一意ボーナス
    if (firstMoveUnique) score += 20.0;

    // 4. 長手数になりやすい構造特徴（案B：玉の広がり・捨駒・合駒）
    score += std::min(metrics.distinctKingSquares, 25) * 2.2;
    score += std::min(metrics.kingDistance, 60) * 0.6;
    score += std::min(metrics.sacrifices, 20) * 2.5;
    score += std::min(metrics.interpositions, 20) * 2.0;

    // 5. 盤上の過密ペナルティ（手数に対して駒が多すぎると余詰爆発・探索低速化しやすい）
    const int idealTotal = std::clamp(4 + metrics.plies / 3, 5, 24);
    const int actualTotal = metrics.attackPieces + metrics.defendBoardPieces;
    if (actualTotal > idealTotal) {
        score -= static_cast<double>(actualTotal - idealTotal) * 1.4;
    }

    // 6. 展開回数ペナルティ（行き詰まった枝に固執せず自動バックトラック）
    score -= std::log1p(static_cast<double>(std::max(0, expansionCount))) * 12.0;
    return score;
}

QString structuralSignature(const QString& sfen, const QStringList& pv)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return sfen;
    const int kingSq = pos.find_king(Color::White);
    const int kr = kingSq >= 0 ? shogi::square_row(kingSq) : -1;
    const int kc = kingSq >= 0 ? shogi::square_col(kingSq) : -1;

    // 玉の領域（3x3ブロック）と先頭最大4手の座標・駒種で系統をまとめる
    QString sig = QStringLiteral("k%1%2|p%3|").arg(kr / 2).arg(kc / 2).arg(pv.size());
    const int prefix = std::min<int>(static_cast<int>(pv.size()), 4);
    for (int i = 0; i < prefix; ++i) {
        if (i > 0) sig += QLatin1Char(',');
        sig += pv.at(i);
    }
    return sig;
}

} // namespace TsumeRetrogradeOps
