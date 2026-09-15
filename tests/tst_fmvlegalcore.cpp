#include <QtTest>

#include "enginemovevalidator.h"
#include "fmvconverter.h"
#include "fmvlegalcore.h"
#include "fmvposition.h"
#include "shogiboard.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFmvLegalCore : public QObject
{
    Q_OBJECT

    void verifyPositionUnchanged(const fmv::EnginePosition& actual,
                                 const fmv::EnginePosition& expected)
    {
        QCOMPARE(actual.board, expected.board);
        QCOMPARE(actual.occupied, expected.occupied);
        QCOMPARE(actual.zobristKey, expected.zobristKey);
        for (int c = 0; c < 2; ++c) {
            QCOMPARE(actual.colorOcc[c], expected.colorOcc[c]);
            QCOMPARE(actual.kingSq[c], expected.kingSq[c]);
            QCOMPARE(actual.hand[c], expected.hand[c]);
            for (int pt = 0; pt < static_cast<int>(fmv::PieceType::PieceTypeNb); ++pt) {
                QCOMPARE(actual.pieceOcc[c][pt], expected.pieceOcc[c][pt]);
            }
        }
    }

private slots:
    void countLegalMoves_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.pieceStand());

        fmv::LegalCore core;
        int count = core.countLegalMoves(pos, fmv::Color::Black);
        QCOMPARE(count, 30);
    }

    void countChecksToKing_inCheck()
    {
        // 飛車で王手されている局面
        QString sfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.pieceStand());

        fmv::LegalCore core;
        int checks = core.countChecksToKing(pos, fmv::Color::Black);
        QVERIFY(checks >= 1);
    }

    void countChecksToKing_noCheck()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.pieceStand());

        fmv::LegalCore core;
        int checks = core.countChecksToKing(pos, fmv::Color::Black);
        QCOMPARE(checks, 0);
    }

    void drop_legal()
    {
        // 銀を持っていて空きマスに打てる
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b S 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.pieceStand());

        fmv::LegalCore core;
        fmv::Move drop;
        drop.kind = fmv::MoveKind::Drop;
        drop.to = fmv::toSquare(4, 4);
        drop.piece = fmv::PieceType::Silver;
        drop.promote = false;

        // do/undoで合法性チェック
        fmv::UndoState undo;
        bool ok = core.tryApplyLegalMove(pos, fmv::Color::Black, drop, undo);
        QVERIFY(ok);
        core.undoAppliedMove(pos, fmv::Color::Black, undo);
    }

    void pawnDropLegality_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<bool>("white");
        QTest::addColumn<bool>("legal");

        // 先手は1二歩打、後手は上下左右反転した局面の9八歩打を調べる。
        QTest::newRow("black-mate")
            << QStringLiteral("8k/9/7LG/9/9/9/9/9/K8 b P 1") << false << false;
        QTest::newRow("white-mate")
            << QStringLiteral("8k/9/9/9/9/9/gl7/9/K8 w p 1") << true << false;

        // 遠くの歩や斜めに動けない香は、王手した歩を取れない。
        QTest::newRow("black-mate-unreachable-pawn")
            << QStringLiteral("8k/9/7LG/9/9/9/p8/9/K8 b P 1") << false << false;
        QTest::newRow("white-mate-unreachable-pawn")
            << QStringLiteral("8k/9/8P/9/9/9/gl7/9/K8 w p 1") << true << false;
        QTest::newRow("black-mate-unreachable-lance")
            << QStringLiteral("7lk/9/7LG/9/9/9/9/9/K8 b P 1") << false << false;

        // 3二の飛車は2二の歩を飛び越えて1二の歩を取れない。
        QTest::newRow("black-mate-blocked-rook")
            << QStringLiteral("7lk/6rp1/7G1/9/9/9/9/9/K8 b P 1") << false << false;
        // 2一銀で歩を取ると3一飛の王手が通るため、王手回避にならない。
        QTest::newRow("black-mate-pinned-silver")
            << QStringLiteral("6Rsk/9/7LG/9/9/9/9/9/K8 b P 1") << false << false;

        QTest::newRow("black-king-can-escape")
            << QStringLiteral("8k/9/8G/9/9/9/9/9/K8 b P 1") << false << true;
        QTest::newRow("white-king-can-escape")
            << QStringLiteral("8k/9/9/9/9/9/g8/9/K8 w p 1") << true << true;
        QTest::newRow("black-king-can-capture")
            << QStringLiteral("8k/9/7L1/9/9/9/9/9/K8 b P 1") << false << true;
        QTest::newRow("white-king-can-capture")
            << QStringLiteral("8k/9/9/9/9/9/1l7/9/K8 w p 1") << true << true;
        QTest::newRow("black-silver-can-capture")
            << QStringLiteral("7sk/9/7LG/9/9/9/9/9/K8 b P 1") << false << true;
        QTest::newRow("white-silver-can-capture")
            << QStringLiteral("8k/9/9/9/9/9/gl7/9/KS7 w p 1") << true << true;
    }

    void pawnDropLegality()
    {
        QFETCH(QString, sfen);
        QFETCH(bool, white);
        QFETCH(bool, legal);

        ShogiBoard board;
        board.setSfen(sfen);
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;
        const auto turn = white ? EngineMoveValidator::WHITE : EngineMoveValidator::BLACK;
        const auto side = white ? fmv::Color::White : fmv::Color::Black;
        QVERIFY(validator.syncContext(simulation, turn, board.boardData(), board.pieceStand()));
        const fmv::EnginePosition original = ctx.pos;

        const QPoint to = white ? QPoint(8, 7) : QPoint(0, 1);
        fmv::Move drop;
        drop.kind = fmv::MoveKind::Drop;
        drop.to = fmv::toSquare(to.x(), to.y());
        drop.piece = fmv::PieceType::Pawn;

        fmv::LegalCore core;
        fmv::MoveList moves;
        core.generateLegalMoves(ctx.pos, side, moves);
        verifyPositionUnchanged(ctx.pos, original);
        int matchingDrops = 0;
        for (int i = 0; i < moves.size; ++i) {
            const auto& candidate = moves.moves[static_cast<std::size_t>(i)];
            if (candidate.kind == drop.kind && candidate.to == drop.to
                && candidate.piece == drop.piece && !candidate.promote) {
                ++matchingDrops;
            }
        }
        QCOMPARE(matchingDrops, legal ? 1 : 0);
        QCOMPARE(core.countLegalMoves(ctx.pos, side), moves.size);
        verifyPositionUnchanged(ctx.pos, original);

        // GUIの互換API、Context API、適用APIで同じ結果になること。
        ShogiMove move(white ? QPoint(EngineMoveValidator::WHITE_HAND_FILE, 8)
                             : QPoint(EngineMoveValidator::BLACK_HAND_FILE, 0),
                       to, white ? Piece::WhitePawn : Piece::BlackPawn, Piece::None, false);
        const auto compatStatus = validator.isLegalMove(turn, board.boardData(), board.pieceStand(), move);
        QCOMPARE(compatStatus.nonPromotingMoveExists, legal);
        QVERIFY(!compatStatus.promotingMoveExists);
        const auto contextStatus = validator.isLegalMove(ctx, move);
        QCOMPARE(contextStatus.nonPromotingMoveExists, legal);
        QVERIFY(!contextStatus.promotingMoveExists);
        verifyPositionUnchanged(ctx.pos, original);

        const auto coreStatus = core.checkMove(ctx.pos, side, drop);
        QCOMPARE(coreStatus.nonPromotingMoveExists, legal);
        verifyPositionUnchanged(ctx.pos, original);
        const auto variants = core.checkMoveVariants(ctx.pos, side, drop);
        QCOMPARE(variants.nonPromotingMoveExists, legal);
        QVERIFY(!variants.promotingMoveExists);
        verifyPositionUnchanged(ctx.pos, original);

        const bool applied = validator.tryApplyMove(simulation, move);
        QCOMPARE(applied, legal);
        if (applied) {
            QCOMPARE(simulation.undoSize, 1);
            QVERIFY(validator.undoLastMove(simulation));
        }
        QCOMPARE(ctx.turn, turn);
        QCOMPARE(simulation.undoSize, 0);
        verifyPositionUnchanged(ctx.pos, original);
    }

    void pawnAdvanceMate_legal()
    {
        // 1三歩を1二へ進めて詰ませる手は、打ち歩詰めではない。
        ShogiBoard board;
        board.setSfen(QStringLiteral("8k/9/6NGP/9/9/9/9/9/K8 b - 1"));
        EngineMoveValidator validator;
        EngineMoveValidator::SimulationContext simulation;
        auto& ctx = simulation.context;
        QVERIFY(validator.syncContext(simulation, EngineMoveValidator::BLACK, board.boardData(), board.pieceStand()));
        const fmv::EnginePosition original = ctx.pos;
        ShogiMove move(QPoint(0, 2), QPoint(0, 1), Piece::BlackPawn, Piece::None, false);
        const auto status = validator.isLegalMove(ctx, move);
        QVERIFY(status.nonPromotingMoveExists);
        QVERIFY(status.promotingMoveExists);
        QVERIFY(validator.tryApplyMove(simulation, move));
        QCOMPARE(validator.checkIfKingInCheck(ctx), 1);
        QCOMPARE(validator.generateLegalMoves(ctx), 0);
        QVERIFY(validator.undoLastMove(simulation));
        verifyPositionUnchanged(ctx.pos, original);
    }

    void moveVariants_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QPoint>("from");
        QTest::addColumn<QPoint>("to");
        QTest::addColumn<bool>("nonPromoting");
        QTest::addColumn<bool>("promoting");
        QTest::addColumn<bool>("white");
        const auto add = [](const char* name, const QString& sfen, QPoint from, QPoint to,
                            bool nonPromoting, bool promoting) {
            for (bool white : {false, true}) {
                const QByteArray row = QByteArray(name) + (white ? "-white" : "-black");
                QTest::newRow(row.constData()) << sfen << from << to << nonPromoting << promoting << white;
            }
        };
        add("pawn-outside-zone", QStringLiteral("k8/9/9/9/4P4/9/9/9/4K4 b - 1"),
            QPoint(4, 4), QPoint(4, 3), true, false);
        add("pawn-enter-zone", QStringLiteral("k8/9/9/4P4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 2), true, true);
        add("pawn-inside-zone", QStringLiteral("k8/9/4P4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(4, 1), true, true);
        add("pawn-mandatory", QStringLiteral("k8/4P4/9/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 1), QPoint(4, 0), false, true);
        add("lance-mandatory", QStringLiteral("k8/9/9/4L4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 0), false, true);
        add("lance-optional", QStringLiteral("k8/9/9/4L4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 1), true, true);
        add("knight-optional", QStringLiteral("k8/9/9/9/4N4/9/9/9/4K4 b - 1"),
            QPoint(4, 4), QPoint(3, 2), true, true);
        add("knight-last-rank", QStringLiteral("k8/9/4N4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(3, 0), false, true);
        add("knight-second-rank", QStringLiteral("k8/9/9/4N4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(3, 1), false, true);
        add("silver-leave-zone", QStringLiteral("k8/9/4S4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(3, 3), true, true);
        add("silver-last-rank", QStringLiteral("k8/4S4/9/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 1), QPoint(4, 0), true, true);
        add("rook-enter-zone", QStringLiteral("k8/9/9/9/4R4/9/9/9/4K4 b - 1"),
            QPoint(4, 4), QPoint(4, 2), true, true);
        add("rook-leave-zone", QStringLiteral("k8/9/4R4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(4, 4), true, true);
        add("bishop-enter-zone", QStringLiteral("k8/9/9/9/4B4/9/9/9/4K4 b - 1"),
            QPoint(4, 4), QPoint(2, 2), true, true);
        add("bishop-leave-zone", QStringLiteral("k8/9/4B4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(6, 4), true, true);
        for (const QString& piece : {QStringLiteral("G"), QStringLiteral("+P"), QStringLiteral("+L"),
                                     QStringLiteral("+N"), QStringLiteral("+S"), QStringLiteral("+B"),
                                     QStringLiteral("+R")}) {
            const QByteArray name = "unpromotable-" + piece.toLatin1();
            add(name.constData(), QStringLiteral("k8/9/9/4") + piece + QStringLiteral("4/9/9/9/9/4K4 b - 1"),
                QPoint(4, 3), QPoint(4, 2), true, false);
        }
        add("king", QStringLiteral("k8/9/9/4K4/9/9/9/9/9 b - 1"),
            QPoint(4, 3), QPoint(4, 2), true, false);
        add("capture", QStringLiteral("k8/9/4s4/4S4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 2), true, true);
        add("capture-promoted-piece", QStringLiteral("k8/9/4+s4/4S4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 2), true, true);
        add("pinned", QStringLiteral("k3r4/9/9/4S4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(3, 2), false, false);
        add("pinned-mandatory", QStringLiteral("k3r4/9/4N4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(3, 0), false, false);
        add("interpose", QStringLiteral("k3r4/9/9/3S5/9/9/9/9/4K4 b - 1"),
            QPoint(5, 3), QPoint(4, 2), true, true);
        add("capture-checker", QStringLiteral("k8/9/4r4/3S5/9/9/9/9/4K4 b - 1"),
            QPoint(5, 3), QPoint(4, 2), true, true);
        add("blocked-lance", QStringLiteral("k8/9/4G4/4L4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 0), false, false);
        add("friendly-destination", QStringLiteral("k8/9/4G4/4S4/9/9/9/9/4K4 b - 1"),
            QPoint(4, 3), QPoint(4, 2), false, false);
        add("illegal-movement", QStringLiteral("k8/9/4P4/9/9/9/9/9/4K4 b - 1"),
            QPoint(4, 2), QPoint(4, 3), false, false);
    }

    void moveVariants()
    {
        QFETCH(QString, sfen);
        QFETCH(QPoint, from);
        QFETCH(QPoint, to);
        QFETCH(bool, nonPromoting);
        QFETCH(bool, promoting);
        QFETCH(bool, white);
        ShogiBoard board;
        board.setSfen(sfen);
        QList<Piece> boardData = board.boardData();
        if (white) {
            // 局面を180度回転して駒の色を交換し、同じ境界条件を後手でも調べる。
            for (qsizetype i = 0; i < boardData.size(); ++i) {
                const QChar c(static_cast<char>(board.boardData().at(i)));
                boardData[boardData.size() - 1 - i] = charToPiece(c.isUpper() ? c.toLower() : c.toUpper());
            }
            from = QPoint(8 - from.x(), 8 - from.y());
            to = QPoint(8 - to.x(), 8 - to.y());
        }
        const auto turn = white ? EngineMoveValidator::WHITE : EngineMoveValidator::BLACK;
        const auto side = white ? fmv::Color::White : fmv::Color::Black;
        EngineMoveValidator validator;
        EngineMoveValidator::Context ctx;
        QVERIFY(validator.syncContext(ctx, turn, boardData, board.pieceStand()));
        ctx.pos.zobristKey = 0x123456789abcdef0ULL;
        const auto original = ctx.pos;
        const auto fromSq = fmv::toSquare(from.x(), from.y());
        const auto toSq = fmv::toSquare(to.x(), to.y());
        ShogiMove move(from, to, static_cast<Piece>(ctx.pos.board[fromSq]),
                       static_cast<Piece>(ctx.pos.board[toSq]), false);
        fmv::ConvertedMove cm;
        QVERIFY(fmv::Converter::toEngineMove(cm, turn, ctx.pos, move));
        fmv::LegalCore core;

        // 一手ずつ独立して判定する既存経路とも照合する。
        QCOMPARE(core.checkMove(ctx.pos, side, cm.nonPromote).nonPromotingMoveExists, nonPromoting);
        verifyPositionUnchanged(ctx.pos, original);
        fmv::Move promoted = cm.nonPromote;
        promoted.promote = true;
        QCOMPARE(core.checkMove(ctx.pos, side, promoted).nonPromotingMoveExists, promoting);
        verifyPositionUnchanged(ctx.pos, original);

        for (bool selectedPromotion : {false, true}) {
            move.isPromotion = selectedPromotion;
            fmv::Move candidate = cm.nonPromote;
            candidate.promote = selectedPromotion;
            const auto combined = core.checkMoveVariants(ctx.pos, side, candidate);
            QCOMPARE(combined.nonPromotingMoveExists, nonPromoting);
            QCOMPARE(combined.promotingMoveExists, promoting);
            verifyPositionUnchanged(ctx.pos, original);
            const auto contextStatus = validator.isLegalMove(ctx, move);
            QCOMPARE(contextStatus.nonPromotingMoveExists, nonPromoting);
            QCOMPARE(contextStatus.promotingMoveExists, promoting);
            verifyPositionUnchanged(ctx.pos, original);
            const auto compatStatus = validator.isLegalMove(turn, boardData, board.pieceStand(), move);
            QCOMPARE(compatStatus.nonPromotingMoveExists, nonPromoting);
            QCOMPARE(compatStatus.promotingMoveExists, promoting);
        }
    }

    void mandatoryPromotion()
    {
        // 1段目に歩が進む場合は成り必須
        QString sfen = QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.pieceStand());

        fmv::LegalCore core;

        // 不成は疑似合法ではないのでtryApplyLegalMoveで却下される
        fmv::Move nonPromote;
        nonPromote.kind = fmv::MoveKind::Board;
        nonPromote.from = fmv::toSquare(8, 1);
        nonPromote.to = fmv::toSquare(8, 0);
        nonPromote.piece = fmv::PieceType::Pawn;
        nonPromote.promote = false;

        fmv::UndoState undo1;
        bool nonPromOk = core.tryApplyLegalMove(pos, fmv::Color::Black, nonPromote, undo1);
        QVERIFY(!nonPromOk); // 1段目不成は不可

        // 成りは合法
        fmv::Move promote;
        promote.kind = fmv::MoveKind::Board;
        promote.from = fmv::toSquare(8, 1);
        promote.to = fmv::toSquare(8, 0);
        promote.piece = fmv::PieceType::Pawn;
        promote.promote = true;

        fmv::UndoState undo2;
        bool promOk = core.tryApplyLegalMove(pos, fmv::Color::Black, promote, undo2);
        QVERIFY(promOk);
        core.undoAppliedMove(pos, fmv::Color::Black, undo2);
    }
};

QTEST_MAIN(TestFmvLegalCore)
#include "tst_fmvlegalcore.moc"
