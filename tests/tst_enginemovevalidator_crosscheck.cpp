#include <QtTest>
#include "position_test_helper.h"

#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

class TestEngineMoveValidatorCrosscheck : public QObject
{
    Q_OBJECT

    void verifyLegalMoveCount(const QString& sfen,
                              Turn turn,
                              int expectedCount)
    {
        ShogiBoard board;
        board.setSfen(sfen);
        int count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(board.position(), turn));
        QCOMPARE(count, expectedCount);
    }

    void verifyKingInCheck(const QString& sfen,
                           Turn turn,
                           int expectedChecks)
    {
        ShogiBoard board;
        board.setSfen(sfen);
        int checks = EngineMoveValidator::checkCount(EngineMoveValidator::load(PositionTestHelper::fromSquares(QList<Piece>(board.position().squares().cbegin(), board.position().squares().cend())), turn));
        QCOMPARE(checks, expectedChecks);
    }

private slots:
    void crosscheck_hirate_black()
    {
        verifyLegalMoveCount(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            Turn::Black, 30);
    }

    void crosscheck_hirate_white()
    {
        verifyLegalMoveCount(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            Turn::White, 30);
    }

    void crosscheck_midgame()
    {
        // 中盤局面
        QString sfen = QStringLiteral("ln1gk1snl/1r4gb1/p1pppp1pp/1p4p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        int count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(board.position(), Turn::Black));
        QVERIFY(count > 0);
    }

    void crosscheck_withHand()
    {
        // 持ち駒あり
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P2S 1");
        ShogiBoard board;
        board.setSfen(sfen);
        int count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(board.position(), Turn::Black));
        QVERIFY(count > 30);  // 持ち駒があるので平手より多い
    }

    void crosscheck_inCheck()
    {
        // 王手局面
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        verifyKingInCheck(sfen, Turn::Black, 1);

        ShogiBoard board;
        board.setSfen(sfen);
        int count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(board.position(), Turn::Black));
        QVERIFY(count > 0);  // 王手回避手が存在する
    }

    void crosscheck_noCheck_hirate()
    {
        verifyKingInCheck(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            Turn::Black, 0);
    }

    void crosscheck_isLegalMove()
    {
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        // 合法手: 7六歩
        auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"7g7f"));

        QCOMPARE(st.nonPromotingMoveExists, true);
        QCOMPARE(st.promotingMoveExists, false);

        // 不合法手: 7六歩→7七（後退）
        auto st2 = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"7g7h"));

        QCOMPARE(st2.nonPromotingMoveExists, false);
        QCOMPARE(st2.promotingMoveExists, false);
    }

    void crosscheck_promotionMoves()
    {
        // 成り可能局面
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"5d5c"));

        QCOMPARE(st.nonPromotingMoveExists, true);
        QCOMPARE(st.promotingMoveExists, true);
    }

    void crosscheck_whiteMoves()
    {
        // 後手番の局面
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2");
        verifyLegalMoveCount(sfen, Turn::White, 30);
    }

    // ---- 指し手と局面の食い違い ----

    void mismatch_opponentPiece()
    {
        // 先手番に後手の駒を動かす → 指せない
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        const auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"3c3d"));
        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void mismatch_emptySource()
    {
        // 移動元が空 → 指せない
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        const auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"5e5d"));
        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void mismatch_dropWithoutHand()
    {
        // 持ち駒がないのに打とうとする → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        // 銀を持っていないのに打つ
        auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"S*5e"));

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void mismatch_opponentPieceDrop()
    {
        // 相手の駒を打とうとする → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b s 1"));

        // 先手番だが後手の銀を打とうとする
        auto st = EngineMoveValidator::legalStatus(board.position(), Turn::Black, UsiMove::fromString(u"S*5e"));

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }
};

QTEST_MAIN(TestEngineMoveValidatorCrosscheck)
#include "tst_enginemovevalidator_crosscheck.moc"
