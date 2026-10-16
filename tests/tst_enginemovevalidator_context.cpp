#include <QtTest>

#include <QSet>

#include "enginemovevalidator.h"
#include "shogiboard.h"

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
    static QSet<QString> legalMovesByQuery(const EngineMoveValidator::Context& ctx, const ShogiBoard& board)
    {
        QSet<QString> out;
        const Turn turn = ctx.turn;
        const auto pieceAt = [&board](BoardSquare square) {
            return board.position().squares().at((square.rank - 1) * 9 + (square.file - 1));
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
                        const LegalMoveStatus status = EngineMoveValidator::legalStatus(ctx, move);
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
                    if (EngineMoveValidator::legalStatus(ctx, move).nonPromotingMoveExists) out.insert(move.toString());
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
        const auto ctx = EngineMoveValidator::load(board.position(), board.currentPlayer());
        QVERIFY(ctx.synced);

        const QStringList listed = toStrings(EngineMoveValidator::legalMoves(ctx));
        const QSet<QString> unique(listed.cbegin(), listed.cend());
        QCOMPARE(unique.size(), listed.size());
        QCOMPARE(listed.size(), EngineMoveValidator::legalMoveCount(ctx));
        QCOMPARE(unique, legalMovesByQuery(ctx, board));
    }

    /// ピンされた駒・王手の回避・打ち歩詰め・成りと不成の両方
    void legalMoves_rules()
    {
        ShogiBoard board;

        QVERIFY(board.trySetSfen(QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1")));
        QStringList moves = toStrings(EngineMoveValidator::legalMoves(board.position(), Turn::Black));
        QVERIFY(!moves.contains(QStringLiteral("5h4g")));  // ピンされた金は筋から外れられない
        QVERIFY(moves.contains(QStringLiteral("5h5g")));
        QVERIFY(moves.contains(QStringLiteral("3h4g")));
        QVERIFY(moves.contains(QStringLiteral("G*6g")));

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K4 b G 1")));
        moves = toStrings(EngineMoveValidator::legalMoves(board.position(), Turn::Black));
        moves.sort();
        QCOMPARE(moves, QStringList({"5i4h", "5i4i", "5i6h", "5i6i", "G*5f", "G*5g", "G*5h"}));

        QVERIFY(board.trySetSfen(QStringLiteral("kn7/9/1G7/9/9/9/9/9/4K4 b P 1")));
        moves = toStrings(EngineMoveValidator::legalMoves(board.position(), Turn::Black));
        QVERIFY(!moves.contains(QStringLiteral("P*9b")));  // 打ち歩詰め
        QVERIFY(moves.contains(QStringLiteral("P*9c")));

        QVERIFY(board.trySetSfen(QStringLiteral("9/k8/9/S1S6/4+R3+R/9/9/9/+B3+B3K b - 1")));
        moves = toStrings(EngineMoveValidator::legalMoves(board.position(), Turn::Black));
        QVERIFY(moves.contains(QStringLiteral("9d8c")));
        QVERIFY(moves.contains(QStringLiteral("9d8c+")));
        QVERIFY(moves.contains(QStringLiteral("5e3e")));
        QVERIFY(!moves.contains(QStringLiteral("5e3e+")));  // 成駒は成れない
    }

    void legalMoves_unsyncedContextIsEmpty()
    {
        const EngineMoveValidator::Context ctx;
        QVERIFY(EngineMoveValidator::legalMoves(ctx).isEmpty());
        QVERIFY(EngineMoveValidator::pseudoLegalMoves(ctx).isEmpty());
    }

    /// 駒の動きだけで決めた手は合法手を含み、自玉の安全・二歩・行き所のない駒を見ない
    void pseudoLegalMoves_includeIllegalMoves()
    {
        ShogiBoard board;
        for (const QString& sfen : {kHirateSfen, QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1"),
                                    QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K4 b G 1")}) {
            QVERIFY(board.trySetSfen(sfen));
            const QStringList legal = toStrings(EngineMoveValidator::legalMoves(board.position(), board.currentPlayer()));
            const QStringList pseudo = toStrings(EngineMoveValidator::pseudoLegalMoves(board.position(), board.currentPlayer()));
            for (const QString& move : legal) QVERIFY2(pseudo.contains(move), qPrintable(sfen + " " + move));
        }

        QVERIFY(board.trySetSfen(QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1")));
        QStringList pseudo = toStrings(EngineMoveValidator::pseudoLegalMoves(board.position(), Turn::Black));
        QVERIFY(pseudo.contains(QStringLiteral("5h4g")));   // ピンされた金
        QVERIFY(!pseudo.contains(QStringLiteral("5h5i")));  // 味方の駒（玉）のマスへは動かない
        QVERIFY(!pseudo.contains(QStringLiteral("3h3f")));  // 金は2マス進めない

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1")));
        pseudo = toStrings(EngineMoveValidator::pseudoLegalMoves(board.position(), Turn::Black));
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
        const auto ctx = EngineMoveValidator::load(board.position(), Turn::Black);
        QVERIFY(ctx.synced);
        QCOMPARE(ctx.turn, Turn::Black);
    }

    void contextApi_generateLegalMoves()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        const auto ctx = EngineMoveValidator::load(board.position(), Turn::Black);
        QVERIFY(ctx.synced);

        int count = EngineMoveValidator::legalMoveCount(ctx);
        QCOMPARE(count, 30);

        // 読み込み直さずに同じ局面へ再び問い合わせられる
        int count2 = EngineMoveValidator::legalMoveCount(ctx);
        QCOMPARE(count2, 30);
    }

    void contextApi_isLegalMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        const auto ctx = EngineMoveValidator::load(board.position(), Turn::Black);
        QVERIFY(ctx.synced);

        const auto status = EngineMoveValidator::legalStatus(ctx, UsiMove::fromString(u"7g7f"));
        QVERIFY(status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
        QVERIFY(EngineMoveValidator::isLegal(ctx, UsiMove::fromString(u"7g7f")));
        QVERIFY(!EngineMoveValidator::isLegal(ctx, UsiMove::fromString(u"7g7e")));
    }

    void tryApplyMove_undoLastMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;

        QVERIFY(EngineMoveValidator::syncContext(simulation, board.position(), Turn::Black));

        // 7六歩を進める
        const UsiMove move = UsiMove::fromString(u"7g7f");
        bool ok = EngineMoveValidator::tryApplyMove(simulation, move);
        QVERIFY(ok);
        QCOMPARE(ctx.turn, Turn::White); // 手番が変わった
        QCOMPARE(simulation.undoSize, 1);

        // 後手の合法手数を取得
        int whiteCount = EngineMoveValidator::legalMoveCount(ctx);
        QCOMPARE(whiteCount, 30); // 後手も30手

        // undo
        bool undone = EngineMoveValidator::undoLastMove(simulation);
        QVERIFY(undone);
        QCOMPARE(ctx.turn, Turn::Black);
        QCOMPARE(simulation.undoSize, 0);

        // 元に戻った
        int count = EngineMoveValidator::legalMoveCount(ctx);
        QCOMPARE(count, 30);
    }

    void multipleApplyUndo()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;

        QVERIFY(EngineMoveValidator::syncContext(simulation, board.position(), Turn::Black));

        // 7六歩
        const UsiMove m1 = UsiMove::fromString(u"7g7f");
        QVERIFY(EngineMoveValidator::tryApplyMove(simulation, m1));

        // 3四歩
        const UsiMove m2 = UsiMove::fromString(u"3c3d");
        QVERIFY(EngineMoveValidator::tryApplyMove(simulation, m2));

        QCOMPARE(simulation.undoSize, 2);
        QCOMPARE(ctx.turn, Turn::Black);

        // 2手undo
        QVERIFY(EngineMoveValidator::undoLastMove(simulation));
        QVERIFY(EngineMoveValidator::undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);

        int count = EngineMoveValidator::legalMoveCount(ctx);
        QCOMPARE(count, 30);
    }

    void undoLastMove_emptyStack()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator::SimulationContext simulation;

        QVERIFY(EngineMoveValidator::syncContext(simulation, board.position(), Turn::Black));

        // スタック空でundoはfalse
        QVERIFY(!EngineMoveValidator::undoLastMove(simulation));
    }

    void syncSimulation_discardsPreviousHistory()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        EngineMoveValidator::SimulationContext simulation;
        QVERIFY(EngineMoveValidator::syncContext(simulation, board.position(), Turn::Black));
        const UsiMove move = UsiMove::fromString(u"7g7f");
        QVERIFY(EngineMoveValidator::tryApplyMove(simulation, move));
        QCOMPARE(simulation.undoSize, 1);

        // 別局面に同期した後は、以前の局面への取り消しを許可しない。
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w P 1"));
        QVERIFY(EngineMoveValidator::syncContext(simulation, board.position(), Turn::White));
        const auto original = simulation.context.pos;
        QCOMPARE(simulation.undoSize, 0);
        QVERIFY(!EngineMoveValidator::undoLastMove(simulation));
        QCOMPARE(simulation.context.turn, Turn::White);
        QCOMPARE(simulation.context.pos.board, original.board);
        QCOMPARE(simulation.context.pos.hand[0], original.hand[0]);
        QCOMPARE(simulation.context.pos.hand[1], original.hand[1]);

        // 再同期後も新しい履歴を作成・取り消しできる。
        const UsiMove kingMove = UsiMove::fromString(u"5a5b");
        QVERIFY(EngineMoveValidator::tryApplyMove(simulation, kingMove));
        QCOMPARE(simulation.undoSize, 1);
        QVERIFY(EngineMoveValidator::undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);
        QCOMPARE(simulation.context.turn, Turn::White);
        QCOMPARE(simulation.context.pos.board, original.board);
    }

    void unsynced_simulation_rejectsApplyAndUndo()
    {
        EngineMoveValidator::SimulationContext simulation;
        const UsiMove move = UsiMove::fromString(u"7g7f");
        QVERIFY(!EngineMoveValidator::tryApplyMove(simulation, move));
        QVERIFY(!EngineMoveValidator::undoLastMove(simulation));
        QCOMPARE(simulation.undoSize, 0);
        QVERIFY(!simulation.context.synced);
    }

    void unsynced_context_returns_zero()
    {
        const EngineMoveValidator::Context ctx; // synced=false

        QCOMPARE(EngineMoveValidator::legalMoveCount(ctx), 0);
        QCOMPARE(EngineMoveValidator::checkCount(ctx), 0);

        const auto status = EngineMoveValidator::legalStatus(ctx, UsiMove::fromString(u"7g7f"));
        QVERIFY(!status.nonPromotingMoveExists);
        QVERIFY(!status.promotingMoveExists);
    }
};

QTEST_MAIN(TestEngineMoveValidatorContext)
#include "tst_enginemovevalidator_context.moc"
