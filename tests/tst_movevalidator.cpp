#include <QtTest>

#include "movevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "shogiutils.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestMoveValidator : public QObject
{
    Q_OBJECT

private slots:
    void legalMoves_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        MoveValidator validator;

        int count = validator.generateLegalMoves(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand());
        // Hirate position: 30 legal moves for black
        QCOMPARE(count, 30);
    }

    void isLegalMove_pawnAdvance()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        MoveValidator validator;

        // 7g7f (0-indexed: from(6,6) to(6,5))
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), QLatin1Char('P'), QLatin1Char(' '), false);
        auto status = validator.isLegalMove(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);
        QVERIFY(status.nonPromotingMoveExists);
    }

    void isLegalMove_pawnBackward_illegal()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        MoveValidator validator;

        // Pawn backward (7g7h - going down) should be illegal
        ShogiMove move(QPoint(6, 6), QPoint(6, 7), QLatin1Char('P'), QLatin1Char(' '), false);
        auto status = validator.isLegalMove(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);
        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void twoPawn_illegal()
    {
        // Position with black pawn on 5th file, try to drop another
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        MoveValidator validator;

        // Try to drop P on 5e (file 5) - there's already a pawn on 5g
        ShogiMove move(QPoint(MoveValidator::BLACK_HAND_FILE, 0), QPoint(4, 4),
                       QLatin1Char('P'), QLatin1Char(' '), false);
        auto status = validator.isLegalMove(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);
        QVERIFY(!status.nonPromotingMoveExists);
    }

    void kingInCheck()
    {
        // Black king at 5i checked by gote rook at 5f (file 5 clear)
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        MoveValidator validator;

        int checks = validator.checkIfKingInCheck(
            MoveValidator::BLACK, board.boardData());
        QVERIFY(checks >= 1);
    }

    void promotionInEnemyTerritory()
    {
        // P at rank 4 file 5 entering enemy territory (rank 3)
        // Destination 5c is empty (pppp1pppp has gap at file 5)
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        MoveValidator validator;

        // Move P from 5d(4,3) to 5c(4,2) - destination is empty
        ShogiMove move(QPoint(4, 3), QPoint(4, 2), QLatin1Char('P'), QLatin1Char(' '), false);
        auto status = validator.isLegalMove(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);
        QVERIFY(status.promotingMoveExists);
    }

    void mandatoryPromotion_pawnOnFirstRank()
    {
        // P at rank 2 file 1 (9a is empty for target)
        // Rank 1: 1nsgkgsnl (file 9 empty, rest normal)
        // Rank 2: Pr5b1 (P at file 9, r at file 8, ...)
        QString sfen = QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        MoveValidator validator;

        // Move P from 9b(8,1) to 9a(8,0) - must promote
        ShogiMove move(QPoint(8, 1), QPoint(8, 0), QLatin1Char('P'), QLatin1Char(' '), false);
        auto status = validator.isLegalMove(
            MoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);
        // Non-promoting should not exist (mandatory promotion)
        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(status.promotingMoveExists);
    }
};

QTEST_MAIN(TestMoveValidator)
#include "tst_movevalidator.moc"
