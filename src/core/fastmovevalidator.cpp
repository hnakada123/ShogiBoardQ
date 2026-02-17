#include "fastmovevalidator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <vector>

namespace {

constexpr char kEmpty = ' ';

enum class MoveKind {
    Board,
    Drop
};

struct InternalMove {
    int from = -1;
    int to = -1;
    char piece = kEmpty;
    bool promote = false;
    MoveKind kind = MoveKind::Board;
};

struct Position {
    std::array<char, FastMoveValidator::NUM_BOARD_SQUARES> board{};
    std::array<int, 14> hands{};
};

[[nodiscard]] int toIndex(const int x, const int y)
{
    return y * FastMoveValidator::BOARD_SIZE + x;
}

[[nodiscard]] bool inBoard(const int x, const int y)
{
    return x >= 0 && x < FastMoveValidator::BOARD_SIZE
        && y >= 0 && y < FastMoveValidator::BOARD_SIZE;
}

[[nodiscard]] bool isBlackPiece(const char piece)
{
    return piece >= 'A' && piece <= 'Z';
}

[[nodiscard]] bool isWhitePiece(const char piece)
{
    return piece >= 'a' && piece <= 'z';
}

[[nodiscard]] bool belongsToTurn(const FastMoveValidator::Turn turn, const char piece)
{
    if (turn == FastMoveValidator::BLACK) {
        return isBlackPiece(piece);
    }
    return isWhitePiece(piece);
}

[[nodiscard]] char toBasePiece(const char piece)
{
    switch (piece) {
    case 'Q': return 'P';
    case 'M': return 'L';
    case 'O': return 'N';
    case 'T': return 'S';
    case 'C': return 'B';
    case 'U': return 'R';
    case 'q': return 'p';
    case 'm': return 'l';
    case 'o': return 'n';
    case 't': return 's';
    case 'c': return 'b';
    case 'u': return 'r';
    default: return piece;
    }
}

[[nodiscard]] char promotePiece(const char piece)
{
    switch (piece) {
    case 'P': return 'Q';
    case 'L': return 'M';
    case 'N': return 'O';
    case 'S': return 'T';
    case 'B': return 'C';
    case 'R': return 'U';
    case 'p': return 'q';
    case 'l': return 'm';
    case 'n': return 'o';
    case 's': return 't';
    case 'b': return 'c';
    case 'r': return 'u';
    default: return piece;
    }
}

[[nodiscard]] bool isPromotable(const char piece)
{
    switch (toBasePiece(piece)) {
    case 'P':
    case 'L':
    case 'N':
    case 'S':
    case 'B':
    case 'R':
    case 'p':
    case 'l':
    case 'n':
    case 's':
    case 'b':
    case 'r':
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isPromotionZone(const FastMoveValidator::Turn turn, const int y)
{
    if (turn == FastMoveValidator::BLACK) {
        return y <= 2;
    }
    return y >= 6;
}

[[nodiscard]] bool canPromoteOnMove(const FastMoveValidator::Turn turn,
                                    const char piece,
                                    const int fromY,
                                    const int toY)
{
    return isPromotable(piece)
        && (isPromotionZone(turn, fromY) || isPromotionZone(turn, toY));
}

[[nodiscard]] bool isMandatoryPromotion(const FastMoveValidator::Turn turn,
                                        const char piece,
                                        const int toY)
{
    const char base = toBasePiece(piece);

    if (turn == FastMoveValidator::BLACK) {
        if ((base == 'P' || base == 'L') && toY == 0) {
            return true;
        }
        if (base == 'N' && toY <= 1) {
            return true;
        }
        return false;
    }

    if ((base == 'p' || base == 'l') && toY == 8) {
        return true;
    }
    if (base == 'n' && toY >= 7) {
        return true;
    }
    return false;
}

[[nodiscard]] int handIndex(const FastMoveValidator::Turn ownerTurn, const char rawPiece)
{
    const char piece = static_cast<char>(std::tolower(static_cast<unsigned char>(toBasePiece(rawPiece))));
    int offset = 0;
    if (ownerTurn == FastMoveValidator::WHITE) {
        offset = 7;
    }

    switch (piece) {
    case 'p': return offset + 0;
    case 'l': return offset + 1;
    case 'n': return offset + 2;
    case 's': return offset + 3;
    case 'g': return offset + 4;
    case 'b': return offset + 5;
    case 'r': return offset + 6;
    default: return -1;
    }
}

[[nodiscard]] int expectedHandRank(const FastMoveValidator::Turn ownerTurn, const char rawPiece)
{
    const char piece = toBasePiece(rawPiece);

    if (ownerTurn == FastMoveValidator::BLACK) {
        switch (piece) {
        case 'P': return 0;
        case 'L': return 1;
        case 'N': return 2;
        case 'S': return 3;
        case 'G': return 4;
        case 'B': return 5;
        case 'R': return 6;
        default: return -1;
        }
    }

    switch (piece) {
    case 'r': return 2;
    case 'b': return 3;
    case 'g': return 4;
    case 's': return 5;
    case 'n': return 6;
    case 'l': return 7;
    case 'p': return 8;
    default: return -1;
    }
}

[[nodiscard]] FastMoveValidator::Turn oppositeTurn(const FastMoveValidator::Turn turn)
{
    return (turn == FastMoveValidator::BLACK) ? FastMoveValidator::WHITE : FastMoveValidator::BLACK;
}

[[nodiscard]] char kingChar(const FastMoveValidator::Turn turn)
{
    return (turn == FastMoveValidator::BLACK) ? 'K' : 'k';
}

[[nodiscard]] bool pathClearLine(const Position& pos, const int from, const int to, const int step)
{
    for (int sq = from + step; sq != to; sq += step) {
        if (pos.board[static_cast<std::size_t>(sq)] != kEmpty) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool pathClearDiag(const Position& pos,
                                 const int from,
                                 const int to,
                                 const int stepX,
                                 const int stepY)
{
    int x = from % FastMoveValidator::BOARD_SIZE;
    int y = from / FastMoveValidator::BOARD_SIZE;
    const int toX = to % FastMoveValidator::BOARD_SIZE;
    const int toY = to / FastMoveValidator::BOARD_SIZE;

    x += stepX;
    y += stepY;
    while (x != toX || y != toY) {
        if (pos.board[static_cast<std::size_t>(toIndex(x, y))] != kEmpty) {
            return false;
        }
        x += stepX;
        y += stepY;
    }
    return true;
}

[[nodiscard]] bool attacksSquare(const Position& pos,
                                 const int from,
                                 const int to,
                                 const char piece)
{
    const int fx = from % FastMoveValidator::BOARD_SIZE;
    const int fy = from / FastMoveValidator::BOARD_SIZE;
    const int tx = to % FastMoveValidator::BOARD_SIZE;
    const int ty = to / FastMoveValidator::BOARD_SIZE;
    const int dx = tx - fx;
    const int dy = ty - fy;

    const bool black = isBlackPiece(piece);
    const int forward = black ? -1 : 1;

    switch (piece) {
    case 'P':
    case 'p':
        return dx == 0 && dy == forward;
    case 'L':
    case 'l':
        if (dx != 0 || ((black && dy >= 0) || (!black && dy <= 0))) {
            return false;
        }
        return pathClearLine(pos, from, to, forward * FastMoveValidator::BOARD_SIZE);
    case 'N':
    case 'n':
        return dy == (2 * forward) && (dx == -1 || dx == 1);
    case 'S':
    case 's':
        return (dy == forward && (dx >= -1 && dx <= 1))
            || (dy == -forward && (dx == -1 || dx == 1));
    case 'G':
    case 'g':
    case 'Q':
    case 'q':
    case 'M':
    case 'm':
    case 'O':
    case 'o':
    case 'T':
    case 't':
        return (dy == forward && (dx >= -1 && dx <= 1))
            || (dy == 0 && (dx == -1 || dx == 1))
            || (dy == -forward && dx == 0);
    case 'K':
    case 'k':
        return (dx >= -1 && dx <= 1) && (dy >= -1 && dy <= 1) && !(dx == 0 && dy == 0);
    case 'B':
    case 'b': {
        if (std::abs(dx) != std::abs(dy) || dx == 0) {
            return false;
        }
        const int stepX = (dx > 0) ? 1 : -1;
        const int stepY = (dy > 0) ? 1 : -1;
        return pathClearDiag(pos, from, to, stepX, stepY);
    }
    case 'R':
    case 'r': {
        if (dx == 0 && dy != 0) {
            const int step = (dy > 0) ? FastMoveValidator::BOARD_SIZE : -FastMoveValidator::BOARD_SIZE;
            return pathClearLine(pos, from, to, step);
        }
        if (dy == 0 && dx != 0) {
            const int step = (dx > 0) ? 1 : -1;
            return pathClearLine(pos, from, to, step);
        }
        return false;
    }
    case 'C':
    case 'c': {
        if (std::abs(dx) == std::abs(dy) && dx != 0) {
            const int stepX = (dx > 0) ? 1 : -1;
            const int stepY = (dy > 0) ? 1 : -1;
            return pathClearDiag(pos, from, to, stepX, stepY);
        }
        return (std::abs(dx) + std::abs(dy) == 1);
    }
    case 'U':
    case 'u': {
        if (dx == 0 && dy != 0) {
            const int step = (dy > 0) ? FastMoveValidator::BOARD_SIZE : -FastMoveValidator::BOARD_SIZE;
            return pathClearLine(pos, from, to, step);
        }
        if (dy == 0 && dx != 0) {
            const int step = (dx > 0) ? 1 : -1;
            return pathClearLine(pos, from, to, step);
        }
        return std::abs(dx) == 1 && std::abs(dy) == 1;
    }
    default:
        return false;
    }
}

[[nodiscard]] int findKingSquare(const Position& pos, const FastMoveValidator::Turn turn)
{
    const char king = kingChar(turn);
    for (int i = 0; i < FastMoveValidator::NUM_BOARD_SQUARES; ++i) {
        if (pos.board[static_cast<std::size_t>(i)] == king) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] int countChecks(const Position& pos, const FastMoveValidator::Turn turn)
{
    const int kingSq = findKingSquare(pos, turn);
    if (kingSq < 0) {
        return 0;
    }

    const FastMoveValidator::Turn attacker = oppositeTurn(turn);
    int checks = 0;

    for (int i = 0; i < FastMoveValidator::NUM_BOARD_SQUARES; ++i) {
        const char piece = pos.board[static_cast<std::size_t>(i)];
        if (piece == kEmpty || !belongsToTurn(attacker, piece)) {
            continue;
        }

        if (attacksSquare(pos, i, kingSq, piece)) {
            ++checks;
        }
    }

    return checks;
}

[[nodiscard]] bool ownKingInCheck(const Position& pos, const FastMoveValidator::Turn turn)
{
    return countChecks(pos, turn) > 0;
}

void loadPosition(Position& pos,
                  const QVector<QChar>& boardData,
                  const QMap<QChar, int>& pieceStand)
{
    pos.board.fill(kEmpty);
    pos.hands.fill(0);

    const int boardSize = std::min(static_cast<int>(boardData.size()), FastMoveValidator::NUM_BOARD_SQUARES);
    for (int i = 0; i < boardSize; ++i) {
        pos.board[static_cast<std::size_t>(i)] = boardData[static_cast<qsizetype>(i)].toLatin1();
    }

    for (auto it = pieceStand.constBegin(); it != pieceStand.constEnd(); ++it) {
        const char piece = it.key().toLatin1();
        const FastMoveValidator::Turn owner = isBlackPiece(piece) ? FastMoveValidator::BLACK : FastMoveValidator::WHITE;
        const int idx = handIndex(owner, piece);
        if (idx >= 0) {
            pos.hands[static_cast<std::size_t>(idx)] = std::max(0, it.value());
        }
    }
}

[[nodiscard]] bool isBoardMove(const ShogiMove& move)
{
    return move.fromSquare.x() >= 0 && move.fromSquare.x() < FastMoveValidator::BOARD_SIZE
        && move.fromSquare.y() >= 0 && move.fromSquare.y() < FastMoveValidator::BOARD_SIZE;
}

[[nodiscard]] bool hasHandPiece(const Position& pos,
                                const FastMoveValidator::Turn turn,
                                const char piece)
{
    const int idx = handIndex(turn, piece);
    return idx >= 0 && pos.hands[static_cast<std::size_t>(idx)] > 0;
}

void addHandPiece(Position& pos,
                  const FastMoveValidator::Turn turn,
                  const char captured)
{
    const char base = toBasePiece(captured);
    const char handPiece = (turn == FastMoveValidator::BLACK)
        ? static_cast<char>(std::toupper(static_cast<unsigned char>(base)))
        : static_cast<char>(std::tolower(static_cast<unsigned char>(base)));

    const int idx = handIndex(turn, handPiece);
    if (idx >= 0) {
        ++pos.hands[static_cast<std::size_t>(idx)];
    }
}

void removeHandPiece(Position& pos,
                     const FastMoveValidator::Turn turn,
                     const char piece)
{
    const int idx = handIndex(turn, piece);
    if (idx >= 0 && pos.hands[static_cast<std::size_t>(idx)] > 0) {
        --pos.hands[static_cast<std::size_t>(idx)];
    }
}

[[nodiscard]] bool hasPawnOnFile(const Position& pos,
                                 const FastMoveValidator::Turn turn,
                                 const int file)
{
    const char pawn = (turn == FastMoveValidator::BLACK) ? 'P' : 'p';
    for (int y = 0; y < FastMoveValidator::BOARD_SIZE; ++y) {
        if (pos.board[static_cast<std::size_t>(toIndex(file, y))] == pawn) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isDropDeadSquare(const FastMoveValidator::Turn turn,
                                    const char piece,
                                    const int toY)
{
    const char base = toBasePiece(piece);

    if (turn == FastMoveValidator::BLACK) {
        if ((base == 'P' || base == 'L') && toY == 0) {
            return true;
        }
        if (base == 'N' && toY <= 1) {
            return true;
        }
        return false;
    }

    if ((base == 'p' || base == 'l') && toY == 8) {
        return true;
    }
    if (base == 'n' && toY >= 7) {
        return true;
    }
    return false;
}

[[nodiscard]] bool applyMoveToPosition(Position& pos,
                                       const FastMoveValidator::Turn turn,
                                       const InternalMove& move)
{
    if (move.kind == MoveKind::Drop) {
        if (!hasHandPiece(pos, turn, move.piece)) {
            return false;
        }
        removeHandPiece(pos, turn, move.piece);
        pos.board[static_cast<std::size_t>(move.to)] = move.piece;
        return true;
    }

    const char piece = pos.board[static_cast<std::size_t>(move.from)];
    const char captured = pos.board[static_cast<std::size_t>(move.to)];

    if (piece == kEmpty) {
        return false;
    }

    pos.board[static_cast<std::size_t>(move.from)] = kEmpty;
    const char placedPiece = move.promote ? promotePiece(piece) : piece;
    pos.board[static_cast<std::size_t>(move.to)] = placedPiece;

    if (captured != kEmpty) {
        addHandPiece(pos, turn, captured);
    }

    return true;
}

[[nodiscard]] bool givesDirectPawnCheck(const Position& pos,
                                        const FastMoveValidator::Turn turn,
                                        const InternalMove& move)
{
    if (move.kind != MoveKind::Drop) {
        return false;
    }

    const char pawn = (turn == FastMoveValidator::BLACK) ? 'P' : 'p';
    if (move.piece != pawn) {
        return false;
    }

    const FastMoveValidator::Turn opponent = oppositeTurn(turn);
    const int kingSq = findKingSquare(pos, opponent);
    if (kingSq < 0) {
        return false;
    }

    const int toX = move.to % FastMoveValidator::BOARD_SIZE;
    const int toY = move.to / FastMoveValidator::BOARD_SIZE;
    const int kingX = kingSq % FastMoveValidator::BOARD_SIZE;
    const int kingY = kingSq / FastMoveValidator::BOARD_SIZE;
    const int expectedY = toY + ((turn == FastMoveValidator::BLACK) ? -1 : 1);

    return kingX == toX && kingY == expectedY;
}

[[nodiscard]] bool isPseudoLegalMove(const Position& pos,
                                     const FastMoveValidator::Turn turn,
                                     const InternalMove& move)
{
    if (move.to < 0 || move.to >= FastMoveValidator::NUM_BOARD_SQUARES) {
        return false;
    }

    const char dst = pos.board[static_cast<std::size_t>(move.to)];
    if (dst != kEmpty && belongsToTurn(turn, dst)) {
        return false;
    }

    const int toX = move.to % FastMoveValidator::BOARD_SIZE;
    const int toY = move.to / FastMoveValidator::BOARD_SIZE;

    if (move.kind == MoveKind::Drop) {
        if (move.promote) {
            return false;
        }
        if (!hasHandPiece(pos, turn, move.piece) || dst != kEmpty) {
            return false;
        }

        const char base = toBasePiece(move.piece);
        if ((base == 'P' || base == 'p') && hasPawnOnFile(pos, turn, toX)) {
            return false;
        }

        if (isDropDeadSquare(turn, move.piece, toY)) {
            return false;
        }

        return true;
    }

    if (move.from < 0 || move.from >= FastMoveValidator::NUM_BOARD_SQUARES) {
        return false;
    }

    const char piece = pos.board[static_cast<std::size_t>(move.from)];
    if (piece == kEmpty || !belongsToTurn(turn, piece)) {
        return false;
    }

    if (move.piece != piece) {
        return false;
    }

    if (!attacksSquare(pos, move.from, move.to, piece)) {
        return false;
    }

    const int fromY = move.from / FastMoveValidator::BOARD_SIZE;
    const bool canPromote = canPromoteOnMove(turn, piece, fromY, toY);

    if (move.promote) {
        return canPromote;
    }

    if (isMandatoryPromotion(turn, piece, toY)) {
        return false;
    }

    return true;
}

[[nodiscard]] bool hasAnyLegalMove(const FastMoveValidator::Turn turn,
                                   const Position& pos,
                                   const bool checkPawnDropMate);

[[nodiscard]] bool isLegalAfterSimulation(const FastMoveValidator::Turn turn,
                                          const Position& pos,
                                          const InternalMove& move,
                                          const bool checkPawnDropMate)
{
    if (!isPseudoLegalMove(pos, turn, move)) {
        return false;
    }

    Position next = pos;
    if (!applyMoveToPosition(next, turn, move)) {
        return false;
    }

    if (ownKingInCheck(next, turn)) {
        return false;
    }

    if (checkPawnDropMate && givesDirectPawnCheck(next, turn, move)) {
        const FastMoveValidator::Turn opponent = oppositeTurn(turn);
        if (!hasAnyLegalMove(opponent, next, true)) {
            return false;
        }
    }

    return true;
}

void collectDestinations(const Position& pos,
                         const int from,
                         const char piece,
                         std::vector<int>& destinations)
{
    destinations.clear();

    const int fx = from % FastMoveValidator::BOARD_SIZE;
    const int fy = from / FastMoveValidator::BOARD_SIZE;
    const bool black = isBlackPiece(piece);
    const int forward = black ? -1 : 1;

    const auto addStep = [&](const int dx, const int dy) {
        const int x = fx + dx;
        const int y = fy + dy;
        if (!inBoard(x, y)) {
            return;
        }
        destinations.push_back(toIndex(x, y));
    };

    const auto addRay = [&](const int dx, const int dy) {
        int x = fx + dx;
        int y = fy + dy;
        while (inBoard(x, y)) {
            const int idx = toIndex(x, y);
            destinations.push_back(idx);
            if (pos.board[static_cast<std::size_t>(idx)] != kEmpty) {
                break;
            }
            x += dx;
            y += dy;
        }
    };

    switch (piece) {
    case 'P':
    case 'p':
        addStep(0, forward);
        break;
    case 'L':
    case 'l':
        addRay(0, forward);
        break;
    case 'N':
    case 'n':
        addStep(-1, 2 * forward);
        addStep(1, 2 * forward);
        break;
    case 'S':
    case 's':
        addStep(-1, forward);
        addStep(0, forward);
        addStep(1, forward);
        addStep(-1, -forward);
        addStep(1, -forward);
        break;
    case 'G':
    case 'g':
    case 'Q':
    case 'q':
    case 'M':
    case 'm':
    case 'O':
    case 'o':
    case 'T':
    case 't':
        addStep(-1, forward);
        addStep(0, forward);
        addStep(1, forward);
        addStep(-1, 0);
        addStep(1, 0);
        addStep(0, -forward);
        break;
    case 'B':
    case 'b':
        addRay(-1, -1);
        addRay(1, -1);
        addRay(-1, 1);
        addRay(1, 1);
        break;
    case 'R':
    case 'r':
        addRay(0, -1);
        addRay(0, 1);
        addRay(-1, 0);
        addRay(1, 0);
        break;
    case 'K':
    case 'k':
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                addStep(dx, dy);
            }
        }
        break;
    case 'C':
    case 'c':
        addRay(-1, -1);
        addRay(1, -1);
        addRay(-1, 1);
        addRay(1, 1);
        addStep(0, -1);
        addStep(0, 1);
        addStep(-1, 0);
        addStep(1, 0);
        break;
    case 'U':
    case 'u':
        addRay(0, -1);
        addRay(0, 1);
        addRay(-1, 0);
        addRay(1, 0);
        addStep(-1, -1);
        addStep(1, -1);
        addStep(-1, 1);
        addStep(1, 1);
        break;
    default:
        break;
    }
}

[[nodiscard]] bool hasAnyLegalMove(const FastMoveValidator::Turn turn,
                                   const Position& pos,
                                   const bool checkPawnDropMate)
{
    std::vector<int> destinations;

    for (int from = 0; from < FastMoveValidator::NUM_BOARD_SQUARES; ++from) {
        const char piece = pos.board[static_cast<std::size_t>(from)];
        if (piece == kEmpty || !belongsToTurn(turn, piece)) {
            continue;
        }

        collectDestinations(pos, from, piece, destinations);
        for (const int to : destinations) {
            InternalMove move;
            move.from = from;
            move.to = to;
            move.piece = piece;
            move.kind = MoveKind::Board;

            const int fromY = from / FastMoveValidator::BOARD_SIZE;
            const int toY = to / FastMoveValidator::BOARD_SIZE;
            const bool canPromote = canPromoteOnMove(turn, piece, fromY, toY);
            const bool mandatory = isMandatoryPromotion(turn, piece, toY);

            if (!mandatory) {
                move.promote = false;
                if (isLegalAfterSimulation(turn, pos, move, checkPawnDropMate)) {
                    return true;
                }
            }

            if (canPromote) {
                move.promote = true;
                if (isLegalAfterSimulation(turn, pos, move, checkPawnDropMate)) {
                    return true;
                }
            }
        }
    }

    const std::array<char, 7> handPieces = (turn == FastMoveValidator::BLACK)
        ? std::array<char, 7>{'P', 'L', 'N', 'S', 'G', 'B', 'R'}
        : std::array<char, 7>{'p', 'l', 'n', 's', 'g', 'b', 'r'};

    for (const char piece : handPieces) {
        if (!hasHandPiece(pos, turn, piece)) {
            continue;
        }

        for (int to = 0; to < FastMoveValidator::NUM_BOARD_SQUARES; ++to) {
            InternalMove move;
            move.to = to;
            move.piece = piece;
            move.kind = MoveKind::Drop;
            move.promote = false;

            if (isLegalAfterSimulation(turn, pos, move, checkPawnDropMate)) {
                return true;
            }
        }
    }

    return false;
}

[[nodiscard]] InternalMove toInternalMove(const FastMoveValidator::Turn turn,
                                          const Position& pos,
                                          const ShogiMove& move,
                                          const bool promote)
{
    InternalMove internal;
    internal.promote = promote;

    const int toX = move.toSquare.x();
    const int toY = move.toSquare.y();
    if (!inBoard(toX, toY)) {
        return internal;
    }
    internal.to = toIndex(toX, toY);

    if (isBoardMove(move)) {
        internal.kind = MoveKind::Board;
        internal.from = toIndex(move.fromSquare.x(), move.fromSquare.y());
        internal.piece = pos.board[static_cast<std::size_t>(internal.from)];
        return internal;
    }

    const int handFile = (turn == FastMoveValidator::BLACK)
        ? FastMoveValidator::BLACK_HAND_FILE
        : FastMoveValidator::WHITE_HAND_FILE;
    if (move.fromSquare.x() != handFile) {
        return internal;
    }

    internal.kind = MoveKind::Drop;
    internal.from = -1;
    internal.piece = move.movingPiece.toLatin1();
    return internal;
}

[[nodiscard]] bool moveMetadataMatches(const FastMoveValidator::Turn turn,
                                       const Position& pos,
                                       const ShogiMove& move)
{
    const int toX = move.toSquare.x();
    const int toY = move.toSquare.y();
    if (!inBoard(toX, toY)) {
        return false;
    }

    const int to = toIndex(toX, toY);
    const char boardCaptured = pos.board[static_cast<std::size_t>(to)];
    const char captured = move.capturedPiece.toLatin1();
    if (captured != kEmpty && captured != boardCaptured) {
        return false;
    }

    if (isBoardMove(move)) {
        const int from = toIndex(move.fromSquare.x(), move.fromSquare.y());
        const char boardPiece = pos.board[static_cast<std::size_t>(from)];
        if (boardPiece == kEmpty || !belongsToTurn(turn, boardPiece)) {
            return false;
        }
        return boardPiece == move.movingPiece.toLatin1();
    }

    const int handFile = (turn == FastMoveValidator::BLACK)
        ? FastMoveValidator::BLACK_HAND_FILE
        : FastMoveValidator::WHITE_HAND_FILE;
    if (move.fromSquare.x() != handFile) {
        return false;
    }
    if (move.fromSquare.y() != expectedHandRank(turn, move.movingPiece.toLatin1())) {
        return false;
    }

    const char piece = move.movingPiece.toLatin1();
    return belongsToTurn(turn, piece) && hasHandPiece(pos, turn, piece);
}

} // namespace

LegalMoveStatus FastMoveValidator::isLegalMove(const Turn& turn,
                                               const QVector<QChar>& boardData,
                                               const QMap<QChar, int>& pieceStand,
                                               ShogiMove& currentMove) const
{
    Position pos;
    loadPosition(pos, boardData, pieceStand);

    if (!moveMetadataMatches(turn, pos, currentMove)) {
        return LegalMoveStatus(false, false);
    }

    bool nonPromoting = false;
    bool promoting = false;

    const InternalMove nonPromMove = toInternalMove(turn, pos, currentMove, false);
    nonPromoting = isLegalAfterSimulation(turn, pos, nonPromMove, true);

    const InternalMove promMove = toInternalMove(turn, pos, currentMove, true);
    promoting = isLegalAfterSimulation(turn, pos, promMove, true);

    return LegalMoveStatus(nonPromoting, promoting);
}

int FastMoveValidator::generateLegalMoves(const Turn& turn,
                                          const QVector<QChar>& boardData,
                                          const QMap<QChar, int>& pieceStand) const
{
    Position pos;
    loadPosition(pos, boardData, pieceStand);

    int count = 0;
    std::vector<int> destinations;

    for (int from = 0; from < NUM_BOARD_SQUARES; ++from) {
        const char piece = pos.board[static_cast<std::size_t>(from)];
        if (piece == kEmpty || !belongsToTurn(turn, piece)) {
            continue;
        }

        collectDestinations(pos, from, piece, destinations);
        for (const int to : destinations) {
            InternalMove move;
            move.kind = MoveKind::Board;
            move.from = from;
            move.to = to;
            move.piece = piece;

            const int fromY = from / BOARD_SIZE;
            const int toY = to / BOARD_SIZE;
            const bool canPromote = canPromoteOnMove(turn, piece, fromY, toY);
            const bool mandatory = isMandatoryPromotion(turn, piece, toY);

            if (!mandatory) {
                move.promote = false;
                if (isLegalAfterSimulation(turn, pos, move, true)) {
                    ++count;
                }
            }

            if (canPromote) {
                move.promote = true;
                if (isLegalAfterSimulation(turn, pos, move, true)) {
                    ++count;
                }
            }
        }
    }

    const std::array<char, 7> handPieces = (turn == BLACK)
        ? std::array<char, 7>{'P', 'L', 'N', 'S', 'G', 'B', 'R'}
        : std::array<char, 7>{'p', 'l', 'n', 's', 'g', 'b', 'r'};

    for (const char piece : handPieces) {
        if (!hasHandPiece(pos, turn, piece)) {
            continue;
        }

        for (int to = 0; to < NUM_BOARD_SQUARES; ++to) {
            InternalMove move;
            move.kind = MoveKind::Drop;
            move.to = to;
            move.piece = piece;
            move.promote = false;

            if (isLegalAfterSimulation(turn, pos, move, true)) {
                ++count;
            }
        }
    }

    return count;
}

int FastMoveValidator::checkIfKingInCheck(const Turn& turn,
                                          const QVector<QChar>& boardData) const
{
    Position pos;
    QMap<QChar, int> emptyStand;
    loadPosition(pos, boardData, emptyStand);
    return countChecks(pos, turn);
}
