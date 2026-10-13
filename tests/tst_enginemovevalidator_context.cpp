#include <QtTest>

#include <QSet>

#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestEngineMoveValidatorContext : public QObject
{
    Q_OBJECT

    static QStringList toStrings(const QList<UsiMove>& moves)
    {
        QStringList out;
        for (const UsiMove& move : moves) out << move.toString();
        return out;
    }

    /// 1手ずつ isLegalMove に問い合わせて、合法な指し手（成り・不成は別の手）を集める
    static QSet<QString> legalMovesByQuery(const EngineMoveValidator& validator,
                                           EngineMoveValidator::Context& ctx, const ShogiBoard& board)
    {
        QSet<QString> out;
        const Turn turn = ctx.turn;
        const auto pieceAt = [&board](BoardSquare square) {
            return board.boardData().at((square.rank - 1) * 9 + (square.file - 1));
        };
        for (int fromFile = 1; fromFile <= 9; ++fromFile) {
            for (int fromRank = 1; fromRank <= 9; ++fromRank) {
                const BoardSquare from{fromFile, fromRank};
                const Piece moving = pieceAt(from);
                if (moving == Piece::None || isBlackPiece(moving) != (turn == Turn::Black)) continue;
                for (int toFile = 1; toFile <= 9; ++toFile) {
                    for (int toRank = 1; toRank <= 9; ++toRank) {
                        UsiMove move;
                        move.from = from;
                        move.to = BoardSquare{toFile, toRank};
                        if (!move.isValid()) continue;
                        ShogiMove query = ShogiMove::fromUsiMove(move, turn, moving, Piece::None);
                        const LegalMoveStatus status = validator.isLegalMove(ctx, query);
                        if (status.nonPromotingMoveExists) out.insert(move.toString());
                        move.promote = true;
                        if (status.promotingMoveExists) out.insert(move.toString());
                    }
                }
            }
        }
        for (const Piece base : {Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
                                 Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook}) {
            const Piece inHand = (turn == Turn::Black) ? base : toWhite(base);
            if (board.pieceStandCount(inHand) <= 0) continue;
            for (int toFile = 1; toFile <= 9; ++toFile) {
                for (int toRank = 1; toRank <= 9; ++toRank) {
                    UsiMove move;
                    move.drop = base;
                    move.to = BoardSquare{toFile, toRank};
                    if (pieceAt(move.to) != Piece::None) continue;
                    ShogiMove query = ShogiMove::fromUsiMove(move, turn, inHand, Piece::None);
                    if (validator.isLegalMove(ctx, query).nonPromotingMoveExists) out.insert(move.toString());
                }
            }
        }
        return out;
    }

private slots:
    /// 合法手の一覧は、1手ずつの合法手判定と同じ手の集まりになる
    void legalMoves_matchSingleMoveQueries_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("hirate") << kHirateSfen;
        QTest::newRow("white-to-move")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        QTest::newRow("pinned-gold") << QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1");
        QTest::newRow("in-check") << QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K4 b G 1");
        QTest::newRow("pawn-drop-mate") << QStringLiteral("kn7/9/1G7/9/9/9/9/9/4K4 b P 1");
        QTest::newRow("promotion-variants") << QStringLiteral("9/k8/9/S1S6/4+R3+R/9/9/9/+B3+B3K b - 1");
        QTest::newRow("midgame-with-hands")
            << QStringLiteral("ln1g3nl/1r1sk1gs1/p1pppp1pp/6p2/1p7/2P1P4/PPSP1PPPP/2G2S1R1/LN2KG1NL w Bb 20");
    }

    void legalMoves_matchSingleMoveQueries()
    {
        QFETCH(QString, sfen);
        ShogiBoard board;
        QVERIFY(board.trySetSfen(sfen));
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;
        QVERIFY(validator.syncContext(ctx, board.currentPlayer(), board.boardData(), board.pieceStand()));

        const QStringList listed = toStrings(validator.legalMoves(ctx));
        const QSet<QString> unique(listed.cbegin(), listed.cend());
        QCOMPARE(unique.size(), listed.size());
        QCOMPARE(listed.size(), validator.generateLegalMoves(ctx));
        QCOMPARE(unique, legalMovesByQuery(validator, ctx, board));
    }

    /// ピンされた駒・王手の回避・打ち歩詰め・成りと不成の両方
    void legalMoves_rules()
    {
        EngineMoveValidator validator;
        ShogiBoard board;

        QVERIFY(board.trySetSfen(QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1")));
        QStringList moves = toStrings(validator.legalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        QVERIFY(!moves.contains(QStringLiteral("5h4g")));  // ピンされた金は筋から外れられない
        QVERIFY(moves.contains(QStringLiteral("5h5g")));
        QVERIFY(moves.contains(QStringLiteral("3h4g")));
        QVERIFY(moves.contains(QStringLiteral("G*6g")));

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K4 b G 1")));
        moves = toStrings(validator.legalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        moves.sort();
        QCOMPARE(moves, QStringList({"5i4h", "5i4i", "5i6h", "5i6i", "G*5f", "G*5g", "G*5h"}));

        QVERIFY(board.trySetSfen(QStringLiteral("kn7/9/1G7/9/9/9/9/9/4K4 b P 1")));
        moves = toStrings(validator.legalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        QVERIFY(!moves.contains(QStringLiteral("P*9b")));  // 打ち歩詰め
        QVERIFY(moves.contains(QStringLiteral("P*9c")));

        QVERIFY(board.trySetSfen(QStringLiteral("9/k8/9/S1S6/4+R3+R/9/9/9/+B3+B3K b - 1")));
        moves = toStrings(validator.legalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        QVERIFY(moves.contains(QStringLiteral("9d8c")));
        QVERIFY(moves.contains(QStringLiteral("9d8c+")));
        QVERIFY(moves.contains(QStringLiteral("5e3e")));
        QVERIFY(!moves.contains(QStringLiteral("5e3e+")));  // 成駒は成れない
    }

    void legalMoves_unsyncedContextIsEmpty()
    {
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;
        QVERIFY(validator.legalMoves(ctx).isEmpty());
        QVERIFY(validator.pseudoLegalMoves(ctx).isEmpty());
    }

    /// 駒の動きだけで決めた手は合法手を含み、自玉の安全・二歩・行き所のない駒を見ない
    void pseudoLegalMoves_includeIllegalMoves()
    {
        EngineMoveValidator validator;
        ShogiBoard board;
        for (const QString& sfen : {kHirateSfen, QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1"),
                                    QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K4 b G 1")}) {
            QVERIFY(board.trySetSfen(sfen));
            const QStringList legal = toStrings(validator.legalMoves(board.currentPlayer(), board.boardData(),
                                                                     board.pieceStand()));
            const QStringList pseudo = toStrings(validator.pseudoLegalMoves(board.currentPlayer(),
                                                                            board.boardData(), board.pieceStand()));
            for (const QString& move : legal) QVERIFY2(pseudo.contains(move), qPrintable(sfen + " " + move));
        }

        QVERIFY(board.trySetSfen(QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1")));
        QStringList pseudo = toStrings(validator.pseudoLegalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        QVERIFY(pseudo.contains(QStringLiteral("5h4g")));   // ピンされた金
        QVERIFY(!pseudo.contains(QStringLiteral("5h5i")));  // 味方の駒（玉）のマスへは動かない
        QVERIFY(!pseudo.contains(QStringLiteral("3h3f")));  // 金は2マス進めない

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1")));
        pseudo = toStrings(validator.pseudoLegalMoves(Turn::Black, board.boardData(), board.pieceStand()));
        QVERIFY(pseudo.contains(QStringLiteral("P*5e")));   // 二歩
        QVERIFY(pseudo.contains(QStringLiteral("P*1a")));   // 行き所のない駒
        QVERIFY(!pseudo.contains(QStringLiteral("P*5a")));  // 駒のあるマスには打てない
        QVERIFY(pseudo.contains(QStringLiteral("5g5f")));
        QVERIFY(!pseudo.contains(QStringLiteral("5g5f+"))); // 成れる段ではない
    }

    void syncContext_basic()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        bool ok = validator.syncContext(ctx, Turn::Black,
                                        board.boardData(), board.pieceStand());
        QVERIFY(ok);
        QVERIFY(ctx.synced);
        QCOMPARE(ctx.turn, Turn::Black);
    }

    void contextApi_generateLegalMoves()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;

        QVERIFY(validator.syncContext(ctx, Turn::Black,
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

        QVERIFY(validator.syncContext(ctx, Turn::Black,
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

        QVERIFY(validator.syncContext(simulation, Turn::Black,
                                      board.boardData(), board.pieceStand()));

        // 7六歩を進める
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        bool ok = validator.tryApplyMove(simulation, move);
        QVERIFY(ok);
        QCOMPARE(ctx.turn, Turn::White); // 手番が変わった
        QCOMPARE(simulation.undoSize, 1);

        // 後手の合法手数を取得
        int whiteCount = validator.generateLegalMoves(ctx);
        QCOMPARE(whiteCount, 30); // 後手も30手

        // undo
        bool undone = validator.undoLastMove(simulation);
        QVERIFY(undone);
        QCOMPARE(ctx.turn, Turn::Black);
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

        QVERIFY(validator.syncContext(simulation, Turn::Black,
                                      board.boardData(), board.pieceStand()));

        // 7六歩
        ShogiMove m1(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, m1));

        // 3四歩
        ShogiMove m2(QPoint(2, 2), QPoint(2, 3), Piece::WhitePawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, m2));

        QCOMPARE(simulation.undoSize, 2);
        QCOMPARE(ctx.turn, Turn::Black);

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

        QVERIFY(validator.syncContext(simulation, Turn::Black,
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
        QVERIFY(validator.syncContext(simulation, Turn::Black,
                                      board.boardData(), board.pieceStand()));
        ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, move));
        QCOMPARE(simulation.undoSize, 1);

        // 別局面に同期した後は、以前の局面への取り消しを許可しない。
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w P 1"));
        QVERIFY(validator.syncContext(simulation, Turn::White,
                                      board.boardData(), board.pieceStand()));
        const auto original = simulation.context.pos;
        QCOMPARE(simulation.undoSize, 0);
        QVERIFY(!validator.undoLastMove(simulation));
        QCOMPARE(simulation.context.turn, Turn::White);
        QCOMPARE(simulation.context.pos.board, original.board);
        QCOMPARE(simulation.context.pos.hand[0], original.hand[0]);
        QCOMPARE(simulation.context.pos.hand[1], original.hand[1]);

        // 再同期後も新しい履歴を作成・取り消しできる。
        ShogiMove kingMove(QPoint(4, 0), QPoint(4, 1), Piece::WhiteKing, Piece::None, false);
        QVERIFY(validator.tryApplyMove(simulation, kingMove));
        QCOMPARE(simulation.undoSize, 1);
        QVERIFY(validator.undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);
        QCOMPARE(simulation.context.turn, Turn::White);
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
