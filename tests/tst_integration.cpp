#include <QtTest>
#include <QCoreApplication>
#include <QRandomGenerator>

#include "shogiboard.h"
#include "enginemovevalidator.h"
#include "shogimove.h"
#include "shogiutils.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "livegamesession.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestIntegration : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

    // Count all pieces on board + stand
    int countAllPieces(const ShogiBoard& board)
    {
        int count = 0;
        const auto& data = board.boardData();
        for (int i = 0; i < data.size(); ++i) {
            if (data[i] != Piece::None) {
                ++count;
            }
        }
        const auto& stand = board.getPieceStand();
        for (auto it = stand.constBegin(); it != stand.constEnd(); ++it) {
            count += it.value();
        }
        return count;
    }

private slots:
    // === Test 1: Random Legal Games ===

    void testRepeatedRandomGames()
    {
        for (int game = 0; game < 100; ++game) {
            ShogiBoard board;
            board.setSfen(kHirateSfen);
            EngineMoveValidator validator;

            EngineMoveValidator::Turn turn = EngineMoveValidator::BLACK;

            for (int moveNum = 0; moveNum < 300; ++moveNum) {
                int legalCount = validator.generateLegalMoves(
                    turn, board.boardData(), board.getPieceStand());

                if (legalCount == 0) break; // Checkmate or stalemate

                // We can't easily pick a random legal move without the internal list,
                // so just verify the count and invariant
                int totalPieces = countAllPieces(board);
                QCOMPARE(totalPieces, 40);

                // Make a simple move if possible: advance a pawn
                bool moved = false;
                if (turn == EngineMoveValidator::BLACK) {
                    // Try each file for a pawn advance
                    for (int file = 1; file <= 9 && !moved; ++file) {
                        for (int rank = 9; rank >= 2 && !moved; --rank) {
                            const auto& data = board.boardData();
                            int idx = (rank - 1) * 9 + (file - 1);
                            if (data[idx] == Piece::BlackPawn) {
                                ShogiMove m(QPoint(file - 1, rank - 1),
                                           QPoint(file - 1, rank - 2),
                                           Piece::BlackPawn, Piece::None, false);
                                auto status = validator.isLegalMove(
                                    turn, board.boardData(), board.getPieceStand(), m);
                                if (status.nonPromotingMoveExists) {
                                    board.movePieceToSquare(Piece::BlackPawn,
                                                          file, rank, file, rank - 1, false);
                                    moved = true;
                                }
                            }
                        }
                    }
                }

                if (!moved) break; // Can't easily make a random move

                turn = (turn == EngineMoveValidator::BLACK) ? EngineMoveValidator::WHITE : EngineMoveValidator::BLACK;

                // After each move, pieces should still total 40
                QCOMPARE(countAllPieces(board), 40);

                // Only do a few moves per game for speed
                if (moveNum >= 5) break;
            }
        }
    }

    // === Test 2: Kifu Load + Navigation ===

    void testKifuLoadAndNavigate()
    {
        // Load KIF with branches
        KifParseResult result;
        QString error;
        bool ok = KifToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_branch.kif")), result, &error);

        if (!ok) {
            QSKIP("test_branch.kif not found or parse failed");
        }

        // Build tree
        KifuBranchTree tree;
        SfenPositionTracer tracer;
        tracer.resetToStartpos();

        tree.setRootSfen(kHirateSfen);
        ShogiMove dummyMove;

        auto* current = tree.root();
        for (int i = 0; i < result.mainline.usiMoves.size(); ++i) {
            current = tree.addMove(current, dummyMove,
                                   result.mainline.disp[i].prettyMove,
                                   QStringLiteral("sfen_%1").arg(i));
        }

        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        // Random navigation
        auto* rng = QRandomGenerator::global();
        for (int i = 0; i < 1000; ++i) {
            int action = rng->bounded(4);
            switch (action) {
            case 0: controller.goToFirst(); break;
            case 1: controller.goToLast(); break;
            case 2: controller.goForward(); break;
            case 3: controller.goBack(); break;
            }

            QVERIFY(state.currentNode() != nullptr);
            QVERIFY(state.currentPly() >= 0);
        }
    }

    // === Test 3: Repeated LiveGameSessions ===

    void testRepeatedLiveGameSessions()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);

        int prevLineCount = tree.lineCount();

        for (int cycle = 0; cycle < 20; ++cycle) {
            session.startFromRoot();
            QVERIFY(session.isActive());

            ShogiMove move;
            for (int i = 0; i < 10; ++i) {
                session.addMove(move,
                               QStringLiteral("m%1_%2").arg(cycle).arg(i),
                               QStringLiteral("s%1_%2").arg(cycle).arg(i),
                               QString());
            }

            session.commit();
            QVERIFY(!session.isActive());

            // Line count should be monotonically increasing
            int newLineCount = tree.lineCount();
            QVERIFY(newLineCount >= prevLineCount);
            prevLineCount = newLineCount;
        }
    }

    // === Test 4: SFEN Consistency ===

    void testSfenConsistencyInvariant()
    {
        QStringList testSfens = {
            kHirateSfen,
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b 2P 1"),
        };

        for (const auto& sfen : std::as_const(testSfens)) {
            ShogiBoard board;
            for (int i = 0; i < 100; ++i) {
                board.setSfen(sfen);
                QString boardSfen = board.convertBoardToSfen();

                // Second round-trip
                ShogiBoard board2;
                // Need to reconstruct full SFEN with turn and stand
                QString fullSfen2 = boardSfen + QStringLiteral(" b ") +
                                    board.convertStandToSfen() + QStringLiteral(" 1");
                board2.setSfen(fullSfen2);
                QString boardSfen2 = board2.convertBoardToSfen();

                QCOMPARE(boardSfen, boardSfen2);
            }
        }
    }

    // === Test 5: Full Workflow ===

    void testFullWorkflow()
    {
        // 1. Load KIF
        QString error;
        QStringList moves = KifToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.kif")), &error);

        if (moves.isEmpty()) {
            QSKIP("test_basic.kif not found or parse failed");
        }

        QCOMPARE(moves.size(), 7);

        // 2. Build tree
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        SfenPositionTracer tracer;
        tracer.resetToStartpos();
        QStringList sfens = tracer.generateSfensForMoves(moves);

        ShogiMove dummyMove;
        auto* current = tree.root();
        for (int i = 0; i < moves.size(); ++i) {
            current = tree.addMove(current, dummyMove,
                                   QStringLiteral("move%1").arg(i + 1),
                                   sfens[i]);
        }
        QCOMPARE(tree.nodeCount(), 8); // root + 7

        // 3. Navigation
        KifuNavigationState state;
        state.setTree(&tree);
        state.goToRoot();

        KifuNavigationController controller;
        controller.setTreeAndState(&tree, &state);

        controller.goToLast();
        QCOMPARE(state.currentPly(), 7);

        controller.goToFirst();
        QCOMPARE(state.currentPly(), 0);

        // 4. LiveGameSession
        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        for (int i = 0; i < 5; ++i) {
            session.addMove(dummyMove,
                           QStringLiteral("live%1").arg(i),
                           QStringLiteral("livesfen%1").arg(i),
                           QString());
        }

        auto* committed = session.commit();
        QVERIFY(committed != nullptr);
        QVERIFY(tree.lineCount() >= 2); // Original + new line

        // 5. Navigate to committed moves
        controller.goToFirst();
        QCOMPARE(state.currentPly(), 0);

        // 6. SFEN verification
        QCOMPARE(tree.root()->sfen(), kHirateSfen);
    }
};

QTEST_MAIN(TestIntegration)
#include "tst_integration.moc"
