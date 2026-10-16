#include <QtTest>

#include "enginemovevalidator.h"
#include "shogiboard.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 盤を受け取る省略形（1回だけ問い合わせる）の判定
class TestEngineMoveValidatorBoard : public QObject
{
    Q_OBJECT

private slots:
    void legalMoves_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int count = static_cast<int>(EngineMoveValidator::legalMoves(board.position(), Turn::Black).size());
        QCOMPARE(count, 30);
    }

    void isLegalMove_pawnAdvance()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        auto status = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"7g7f"));

        QVERIFY(status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void twoPawn_illegal()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        auto status = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"P*5e"));

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void isLegalMove_pawnBackward_illegal()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        auto status = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"7g7h"));

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }

    void kingInCheck()
    {
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QVERIFY(EngineMoveValidator::isInCheck(board.position(), Turn::Black));
        QVERIFY(!EngineMoveValidator::isInCheck(board.position(), Turn::White));
    }

    void promotionInEnemyTerritory()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        auto status = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"5d5c"));

        QVERIFY(status.promotingMoveExists);
    }

    void mandatoryPromotion_pawnOnFirstRank()
    {
        QString sfen = QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        auto status = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"9b9a"));

        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(status.promotingMoveExists);
    }
};

QTEST_MAIN(TestEngineMoveValidatorBoard)
#include "tst_enginemovevalidator_board.moc"
