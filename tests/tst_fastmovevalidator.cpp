#include <QtTest>

#include "fastmovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFastMoveValidator : public QObject
{
    Q_OBJECT

private slots:
    void legalMoves_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        FastMoveValidator validator;

        const int count = validator.generateLegalMoves(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand());
        QCOMPARE(count, 30);
    }

    void isLegalMove_pawnAdvance()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        FastMoveValidator validator;

        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QVERIFY(status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void twoPawn_illegal()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        FastMoveValidator validator;

        ShogiMove move(QPoint(FastMoveValidator::BLACK_HAND_FILE, 0), QPoint(4, 4),
                       Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void isLegalMove_pawnBackward_illegal()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        FastMoveValidator validator;

        ShogiMove move(QPoint(6, 6), QPoint(6, 7), Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void kingInCheck()
    {
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        FastMoveValidator validator;

        const int checks = validator.checkIfKingInCheck(
            FastMoveValidator::BLACK, board.boardData());
        QVERIFY(checks >= 1);
    }

    void promotionInEnemyTerritory()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        FastMoveValidator validator;

        ShogiMove move(QPoint(4, 3), QPoint(4, 2), Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QVERIFY(status.promotingMoveExists);
    }

    void mandatoryPromotion_pawnOnFirstRank()
    {
        QString sfen = QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        FastMoveValidator validator;

        ShogiMove move(QPoint(8, 1), QPoint(8, 0), Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(
            FastMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(status.promotingMoveExists);
    }
};

QTEST_MAIN(TestFastMoveValidator)
#include "tst_fastmovevalidator.moc"
