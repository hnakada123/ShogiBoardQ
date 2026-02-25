#include <QtTest>

#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestEngineMoveValidatorContext : public QObject
{
    Q_OBJECT

private slots:
    void syncContext_basic()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        bool ok = validator.syncContext(ctx, EngineMoveValidator::BLACK,
                                        board.boardData(), board.getPieceStand());
        QVERIFY(ok);
        QVERIFY(ctx.synced);
        QCOMPARE(ctx.turn, EngineMoveValidator::BLACK);
    }

    void contextApi_generateLegalMoves()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        validator.syncContext(ctx, EngineMoveValidator::BLACK,
                              board.boardData(), board.getPieceStand());

        int count = validator.generateLegalMoves(ctx);
        QCOMPARE(count, 30);

        // 再syncせずに同じContextで再クエリ可能
        int count2 = validator.generateLegalMoves(ctx);
        QCOMPARE(count2, 30);
    }

    void contextApi_isLegalMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        validator.syncContext(ctx, EngineMoveValidator::BLACK,
                              board.boardData(), board.getPieceStand());

        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        auto status = validator.isLegalMove(ctx, move);
        QVERIFY(status.nonPromotingMoveExists);
    }

    void tryApplyMove_undoLastMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        validator.syncContext(ctx, EngineMoveValidator::BLACK,
                              board.boardData(), board.getPieceStand());

        // 7六歩を進める
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        bool ok = validator.tryApplyMove(ctx, move);
        QVERIFY(ok);
        QCOMPARE(ctx.turn, EngineMoveValidator::WHITE); // 手番が変わった
        QCOMPARE(ctx.undoSize, 1);

        // 後手の合法手数を取得
        int whiteCount = validator.generateLegalMoves(ctx);
        QCOMPARE(whiteCount, 30); // 後手も30手

        // undo
        bool undone = validator.undoLastMove(ctx);
        QVERIFY(undone);
        QCOMPARE(ctx.turn, EngineMoveValidator::BLACK);
        QCOMPARE(ctx.undoSize, 0);

        // 元に戻った
        int count = validator.generateLegalMoves(ctx);
        QCOMPARE(count, 30);
    }

    void multipleApplyUndo()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        validator.syncContext(ctx, EngineMoveValidator::BLACK,
                              board.boardData(), board.getPieceStand());

        // 7六歩
        ShogiMove m1(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(ctx, m1));

        // 3四歩
        ShogiMove m2(QPoint(2, 2), QPoint(2, 3), Piece::WhitePawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(ctx, m2));

        QCOMPARE(ctx.undoSize, 2);
        QCOMPARE(ctx.turn, EngineMoveValidator::BLACK);

        // 2手undo
        QVERIFY(validator.undoLastMove(ctx));
        QVERIFY(validator.undoLastMove(ctx));
        QCOMPARE(ctx.undoSize, 0);

        int count = validator.generateLegalMoves(ctx);
        QCOMPARE(count, 30);
    }

    void undoLastMove_emptyStack()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        validator.syncContext(ctx, EngineMoveValidator::BLACK,
                              board.boardData(), board.getPieceStand());

        // スタック空でundoはfalse
        QVERIFY(!validator.undoLastMove(ctx));
    }

    void unsynced_context_returns_zero()
    {
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx; // synced=false

        QCOMPARE(validator.generateLegalMoves(ctx), 0);
        QCOMPARE(validator.checkIfKingInCheck(ctx), 0);

        ShogiMove move;
        auto status = validator.isLegalMove(ctx, move);
        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }
};

QTEST_MAIN(TestEngineMoveValidatorContext)
#include "tst_enginemovevalidator_context.moc"
