#include <QtTest>

#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

class TestEngineMoveValidatorCrosscheck : public QObject
{
    Q_OBJECT

    void verifyLegalMoveCount(const QString& sfen,
                              EngineMoveValidator::Turn turn,
                              int expectedCount)
    {
        ShogiBoard board;
        board.setSfen(sfen);

        EngineMoveValidator emv;
        int count = emv.generateLegalMoves(turn, board.boardData(), board.getPieceStand());
        QCOMPARE(count, expectedCount);
    }

    void verifyKingInCheck(const QString& sfen,
                           EngineMoveValidator::Turn turn,
                           int expectedChecks)
    {
        ShogiBoard board;
        board.setSfen(sfen);

        EngineMoveValidator emv;
        int checks = emv.checkIfKingInCheck(turn, board.boardData());
        QCOMPARE(checks, expectedChecks);
    }

private slots:
    void crosscheck_hirate_black()
    {
        verifyLegalMoveCount(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            EngineMoveValidator::BLACK, 30);
    }

    void crosscheck_hirate_white()
    {
        verifyLegalMoveCount(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),
            EngineMoveValidator::WHITE, 30);
    }

    void crosscheck_midgame()
    {
        // 中盤局面
        QString sfen = QStringLiteral("ln1gk1snl/1r4gb1/p1pppp1pp/1p4p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        EngineMoveValidator emv;
        int count = emv.generateLegalMoves(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand());
        QVERIFY(count > 0);
    }

    void crosscheck_withHand()
    {
        // 持ち駒あり
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P2S 1");
        ShogiBoard board;
        board.setSfen(sfen);
        EngineMoveValidator emv;
        int count = emv.generateLegalMoves(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand());
        QVERIFY(count > 30);  // 持ち駒があるので平手より多い
    }

    void crosscheck_inCheck()
    {
        // 王手局面
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        verifyKingInCheck(sfen, EngineMoveValidator::BLACK, 1);

        ShogiBoard board;
        board.setSfen(sfen);
        EngineMoveValidator emv;
        int count = emv.generateLegalMoves(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand());
        QVERIFY(count > 0);  // 王手回避手が存在する
    }

    void crosscheck_noCheck_hirate()
    {
        verifyKingInCheck(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            EngineMoveValidator::BLACK, 0);
    }

    void crosscheck_isLegalMove()
    {
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        EngineMoveValidator emv;

        // 合法手: 7六歩
        ShogiMove legalMove(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), legalMove);

        QCOMPARE(st.nonPromotingMoveExists, true);
        QCOMPARE(st.promotingMoveExists, false);

        // 不合法手: 7六歩→7七（後退）
        ShogiMove illegalMove(QPoint(6, 6), QPoint(6, 7), Piece::BlackPawn, Piece::None, false);
        auto st2 = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), illegalMove);

        QCOMPARE(st2.nonPromotingMoveExists, false);
        QCOMPARE(st2.promotingMoveExists, false);
    }

    void crosscheck_promotionMoves()
    {
        // 成り可能局面
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        EngineMoveValidator emv;

        ShogiMove move(QPoint(4, 3), QPoint(4, 2), Piece::BlackPawn, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), move);

        QCOMPARE(st.nonPromotingMoveExists, true);
        QCOMPARE(st.promotingMoveExists, true);
    }

    void crosscheck_whiteMoves()
    {
        // 後手番の局面
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2");
        verifyLegalMoveCount(sfen, EngineMoveValidator::WHITE, 30);
    }

    // ---- メタデータ互換性テスト ----

    void metadata_wrongMovingPiece()
    {
        // movingPiece が盤上の駒と不一致 → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        EngineMoveValidator emv;

        // 7七歩の位置にPawnがあるが、movingPieceをSilverと偽る
        ShogiMove badPiece(QPoint(6, 6), QPoint(6, 5), Piece::BlackSilver, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), badPiece);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void metadata_wrongCapturedPiece()
    {
        // capturedPiece が盤面と不一致 → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        EngineMoveValidator emv;

        // 6五マスは空だが capturedPiece に BlackPawn を指定
        ShogiMove badCapture(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::BlackPawn, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), badCapture);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void metadata_wrongHandFile()
    {
        // 先手なのに後手の駒台ファイル座標(10)を使う → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b S 1"));

        EngineMoveValidator emv;

        // 先手の銀打ちだが fromX=10（後手側）
        ShogiMove badHand(QPoint(10, 3), QPoint(4, 4), Piece::BlackSilver, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), badHand);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void metadata_wrongHandRank()
    {
        // 駒台Rank座標が不正 → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b S 1"));

        EngineMoveValidator emv;

        // 先手の銀（期待Rank=3）だが Rank=0 を指定
        ShogiMove badRank(QPoint(9, 0), QPoint(4, 4), Piece::BlackSilver, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), badRank);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void metadata_dropWithoutHand()
    {
        // 持ち駒がないのに打とうとする → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

        EngineMoveValidator emv;

        // 銀を持っていないのに打つ
        ShogiMove noPiece(QPoint(9, 3), QPoint(4, 4), Piece::BlackSilver, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), noPiece);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }

    void metadata_opponentPieceDrop()
    {
        // 相手の駒を打とうとする → false を返すこと
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b s 1"));

        EngineMoveValidator emv;

        // 先手番だが後手の銀を打とうとする
        ShogiMove oppPiece(QPoint(9, 3), QPoint(4, 4), Piece::WhiteSilver, Piece::None, false);
        auto st = emv.isLegalMove(EngineMoveValidator::BLACK, board.boardData(), board.getPieceStand(), oppPiece);

        QCOMPARE(st.nonPromotingMoveExists, false);
        QCOMPARE(st.promotingMoveExists, false);
    }
};

QTEST_MAIN(TestEngineMoveValidatorCrosscheck)
#include "tst_enginemovevalidator_crosscheck.moc"
