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
                                        board.boardData(), board.pieceStand());
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

        QVERIFY(validator.syncContext(ctx, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));

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

        QVERIFY(validator.syncContext(ctx, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));

        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        auto status = validator.isLegalMove(ctx, move);
        QVERIFY(status.nonPromotingMoveExists);
    }

    void tryApplyMove_undoLastMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;

        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));

        // 7六歩を進める
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        bool ok = validator.tryApplyMove(simulation, move);
        QVERIFY(ok);
        QCOMPARE(ctx.turn, EngineMoveValidator::WHITE); // 手番が変わった
        QCOMPARE(simulation.undoSize, 1);

        // 後手の合法手数を取得
        int whiteCount = validator.generateLegalMoves(ctx);
        QCOMPARE(whiteCount, 30); // 後手も30手

        // undo
        bool undone = validator.undoLastMove(simulation);
        QVERIFY(undone);
        QCOMPARE(ctx.turn, EngineMoveValidator::BLACK);
        QCOMPARE(simulation.undoSize, 0);

        // 元に戻った
        int count = validator.generateLegalMoves(ctx);
        QCOMPARE(count, 30);
    }

    void multipleApplyUndo()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;

        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));

        // 7六歩
        ShogiMove m1(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, m1));

        // 3四歩
        ShogiMove m2(QPoint(2, 2), QPoint(2, 3), Piece::WhitePawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, m2));

        QCOMPARE(simulation.undoSize, 2);
        QCOMPARE(ctx.turn, EngineMoveValidator::BLACK);

        // 2手undo
        QVERIFY(validator.undoLastMove(simulation));
        QVERIFY(validator.undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);

        int count = validator.generateLegalMoves(ctx);
        QCOMPARE(count, 30);
    }

    void undoLastMove_emptyStack()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;

        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));

        // スタック空でundoはfalse
        QVERIFY(!validator.undoLastMove(simulation));
    }

    void syncSimulation_discardsPreviousHistory()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::BLACK,
                                      board.boardData(), board.pieceStand()));
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, move));
        QCOMPARE(simulation.undoSize, 1);

        // 別局面に同期した後は、以前の局面への取り消しを許可しない。
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w P 1"));
        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::WHITE,
                                      board.boardData(), board.pieceStand()));
        const auto original = simulation.context.pos;
        QCOMPARE(simulation.undoSize, 0);
        QVERIFY(!validator.undoLastMove(simulation));
        QCOMPARE(simulation.context.turn, EngineMoveValidator::WHITE);
        QCOMPARE(simulation.context.pos.board, original.board);
        QCOMPARE(simulation.context.pos.hand[0], original.hand[0]);
        QCOMPARE(simulation.context.pos.hand[1], original.hand[1]);

        // 再同期後も新しい履歴を作成・取り消しできる。
        ShogiMove kingMove(QPoint(4, 0), QPoint(4, 1), Piece::WhiteKing, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, kingMove));
        QCOMPARE(simulation.undoSize, 1);
        QVERIFY(validator.undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);
        QCOMPARE(simulation.context.turn, EngineMoveValidator::WHITE);
        QCOMPARE(simulation.context.pos.board, original.board);
    }

    void unsynced_simulation_rejectsApplyAndUndo()
    {
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(!validator.tryApplyMove(simulation, move));
        QVERIFY(!validator.undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);
        QVERIFY(!simulation.context.synced);
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
