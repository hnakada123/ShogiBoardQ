#include <QtTest>

#include "fmvconverter.h"
#include "fmvlegalcore.h"
#include "fmvposition.h"
#include "shogiboard.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFmvPerft : public QObject
{
    Q_OBJECT

    std::int64_t perft(fmv::EnginePosition& pos, fmv::Color side,
                       const fmv::LegalCore& core, int depth)
    {
        if (depth == 0) {
            return 1;
        }

        fmv::MoveList moves;
        moves.clear();
        core.generateLegalMoves(pos, side, moves);

        if (depth == 1) {
            return moves.size;
        }

        std::int64_t nodes = 0;
        fmv::Color next = fmv::opposite(side);
        for (int i = 0; i < moves.size; ++i) {
            fmv::UndoState undo;
            if (pos.doMove(moves.moves[static_cast<std::size_t>(i)], side, undo)) {
                nodes += perft(pos, next, core, depth - 1);
                pos.undoMove(undo, side);
            }
        }
        return nodes;
    }

private slots:
    void perft_hirate_depth1()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

        fmv::LegalCore core;
        std::int64_t nodes = perft(pos, fmv::Color::Black, core, 1);
        QCOMPARE(nodes, 30);
    }

    void perft_hirate_depth2()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

        fmv::LegalCore core;
        std::int64_t nodes = perft(pos, fmv::Color::Black, core, 2);
        QCOMPARE(nodes, 900);
    }

    void perft_hirate_depth3()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());

        fmv::LegalCore core;
        std::int64_t nodes = perft(pos, fmv::Color::Black, core, 3);
        // Known value: 25470
        QCOMPARE(nodes, 25470);
    }
};

QTEST_MAIN(TestFmvPerft)
#include "tst_fmv_perft.moc"
