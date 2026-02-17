#ifndef FASTMOVEVALIDATOR_H
#define FASTMOVEVALIDATOR_H

#include <QChar>
#include <QMap>
#include <QVector>

#include "legalmovestatus.h"
#include "shogimove.h"

class FastMoveValidator
{
public:
    enum Turn {
        BLACK,
        WHITE,
        TURN_SIZE
    };

    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;
    static constexpr int BLACK_HAND_FILE = BOARD_SIZE;
    static constexpr int WHITE_HAND_FILE = BOARD_SIZE + 1;

    LegalMoveStatus isLegalMove(const Turn& turn,
                                const QVector<QChar>& boardData,
                                const QMap<QChar, int>& pieceStand,
                                ShogiMove& currentMove) const;

    int generateLegalMoves(const Turn& turn,
                           const QVector<QChar>& boardData,
                           const QMap<QChar, int>& pieceStand) const;

    int checkIfKingInCheck(const Turn& turn,
                           const QVector<QChar>& boardData) const;
};

#endif // FASTMOVEVALIDATOR_H
