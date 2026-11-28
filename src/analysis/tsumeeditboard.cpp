/// @file tsumeeditboard.cpp
/// @brief 詰将棋の候補局面を組み立てる盤と、盤面の規則・利きの計算の実装

#include "tsumeeditboard.h"

#include <cctype>
#include <cstdlib>

namespace TsumeEditBoard {

using shogi::Color;
using shogi::PieceType;

namespace {

constexpr std::array<int, 8> kPieceSetMax = {0, 18, 4, 4, 4, 4, 2, 2};
constexpr char kHandLetters[8] = {'?', 'P', 'L', 'N', 'S', 'G', 'B', 'R'};

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

} // namespace

bool loadStateFromSfen(const QString& sfen, State& out, shogi::Position* parsed)
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

/// 盤面ルール（二歩・行き所のない駒・駒枚数上限・Limits）を満たせば SFEN を構築する。
QString buildSfen(const State& state, Color sideToMove, const Limits& limits,
                  bool enforceLimits, bool requireAttackPiece)
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

} // namespace TsumeEditBoard
