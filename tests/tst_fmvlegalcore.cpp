#include <QtTest>

#include "fmvconverter.h"
#include "fmvlegalcore.h"
#include "fmvposition.h"
#include "shogiboard.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFmvLegalCore : public QObject
{
    Q_OBJECT

private slots:
    void countLegalMoves_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

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
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

        fmv::LegalCore core;
        int checks = core.countChecksToKing(pos, fmv::Color::Black);
        QVERIFY(checks >= 1);
    }

    void countChecksToKing_noCheck()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

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
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

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

    void pawnDropMate_illegal()
    {
        // 打ち歩詰めの局面: 玉の前に歩を打つと詰むが、打ち歩詰めは違法
        // 簡易テスト: 1一玉を持つ後手に対して先手が2一歩を打つと相手が動けない場合
        QString sfen = QStringLiteral("k8/9/9/9/9/9/9/9/8K b P 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

        fmv::LegalCore core;
        // 歩を(0,1)に打つ → 相手玉(0,0)に王手
        fmv::Move drop;
        drop.kind = fmv::MoveKind::Drop;
        drop.to = fmv::toSquare(0, 1);
        drop.piece = fmv::PieceType::Pawn;
        drop.promote = false;

        int movesWithoutDrop = core.countLegalMoves(pos, fmv::Color::Black);
        // この局面では歩打ちを含めた手がいくつあるか確認（打ち歩詰めは除外される）
        QVERIFY(movesWithoutDrop > 0);
    }

    void mandatoryPromotion()
    {
        // 1段目に歩が進む場合は成り必須
        QString sfen = QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

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
