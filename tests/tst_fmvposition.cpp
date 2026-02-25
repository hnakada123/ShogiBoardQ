#include <QtTest>

#include "fmvposition.h"
#include "fmvconverter.h"
#include "shogiboard.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFmvPosition : public QObject
{
    Q_OBJECT

    /// board配列とbitboardの一貫性を検証
    void verifyBitboardConsistency(const fmv::EnginePosition& pos)
    {
        fmv::EnginePosition rebuilt = pos;
        rebuilt.rebuildBitboards();

        QCOMPARE(pos.occupied, rebuilt.occupied);
        QCOMPARE(pos.colorOcc[0], rebuilt.colorOcc[0]);
        QCOMPARE(pos.colorOcc[1], rebuilt.colorOcc[1]);
        QCOMPARE(pos.kingSq[0], rebuilt.kingSq[0]);
        QCOMPARE(pos.kingSq[1], rebuilt.kingSq[1]);

        for (int c = 0; c < 2; ++c) {
            for (int pt = 0; pt < static_cast<int>(fmv::PieceType::PieceTypeNb); ++pt) {
                QCOMPARE(pos.pieceOcc[c][pt], rebuilt.pieceOcc[c][pt]);
            }
        }
    }

private slots:
    void doUndo_boardMove_noCapture()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());
        fmv::EnginePosition original = pos;

        // 7六歩: from=(6,6), to=(6,5)
        fmv::Move move;
        move.kind = fmv::MoveKind::Board;
        move.from = fmv::toSquare(6, 6);
        move.to = fmv::toSquare(6, 5);
        move.piece = fmv::PieceType::Pawn;
        move.promote = false;

        fmv::UndoState undo;
        bool ok = pos.doMove(move, fmv::Color::Black, undo);
        QVERIFY(ok);

        // 移動元は空、移動先に歩
        QCOMPARE(pos.board[fmv::toSquare(6, 6)], ' ');
        QCOMPARE(pos.board[fmv::toSquare(6, 5)], 'P');
        verifyBitboardConsistency(pos);

        // undo
        pos.undoMove(undo, fmv::Color::Black);
        QCOMPARE(pos.board, original.board);
        verifyBitboardConsistency(pos);
    }

    void doUndo_boardMove_capture()
    {
        // 歩が相手の歩を取る局面
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4p4/4P4/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());
        fmv::EnginePosition original = pos;

        // 5五歩が5四の相手歩を取る: (4,4)→(4,3)
        fmv::Move move;
        move.kind = fmv::MoveKind::Board;
        move.from = fmv::toSquare(4, 4);
        move.to = fmv::toSquare(4, 3);
        move.piece = fmv::PieceType::Pawn;
        move.promote = false;

        fmv::UndoState undo;
        bool ok = pos.doMove(move, fmv::Color::Black, undo);
        QVERIFY(ok);

        QCOMPARE(pos.board[fmv::toSquare(4, 3)], 'P');
        QCOMPARE(pos.board[fmv::toSquare(4, 4)], ' ');
        // 持ち駒に歩が追加
        QCOMPARE(pos.hand[0][static_cast<std::size_t>(fmv::HandType::Pawn)], 1);
        verifyBitboardConsistency(pos);

        // undo
        pos.undoMove(undo, fmv::Color::Black);
        QCOMPARE(pos.board, original.board);
        QCOMPARE(pos.hand[0][static_cast<std::size_t>(fmv::HandType::Pawn)], 0);
        verifyBitboardConsistency(pos);
    }

    void doUndo_boardMove_promotion()
    {
        // 歩が3段目に入って成り
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());
        fmv::EnginePosition original = pos;

        // (4,3)→(4,2) 成り
        fmv::Move move;
        move.kind = fmv::MoveKind::Board;
        move.from = fmv::toSquare(4, 3);
        move.to = fmv::toSquare(4, 2);
        move.piece = fmv::PieceType::Pawn;
        move.promote = true;

        fmv::UndoState undo;
        bool ok = pos.doMove(move, fmv::Color::Black, undo);
        QVERIFY(ok);

        // 成り歩（と金）
        QCOMPARE(pos.board[fmv::toSquare(4, 2)], 'Q');
        verifyBitboardConsistency(pos);

        // undo → 元に戻る
        pos.undoMove(undo, fmv::Color::Black);
        QCOMPARE(pos.board, original.board);
        verifyBitboardConsistency(pos);
    }

    void doUndo_drop()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b S 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());
        fmv::EnginePosition original = pos;

        // 銀打ち: (4,4)
        fmv::Move move;
        move.kind = fmv::MoveKind::Drop;
        move.from = fmv::kInvalidSquare;
        move.to = fmv::toSquare(4, 4);
        move.piece = fmv::PieceType::Silver;
        move.promote = false;

        fmv::UndoState undo;
        bool ok = pos.doMove(move, fmv::Color::Black, undo);
        QVERIFY(ok);

        QCOMPARE(pos.board[fmv::toSquare(4, 4)], 'S');
        QCOMPARE(pos.hand[0][static_cast<std::size_t>(fmv::HandType::Silver)], 0);
        verifyBitboardConsistency(pos);

        // undo
        pos.undoMove(undo, fmv::Color::Black);
        QCOMPARE(pos.board, original.board);
        QCOMPARE(pos.hand[0][static_cast<std::size_t>(fmv::HandType::Silver)], 1);
        verifyBitboardConsistency(pos);
    }

    void sequential_doUndo_roundtrip()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.boardData(), board.getPieceStand());
        fmv::EnginePosition original = pos;

        // 数手進める
        struct MoveSpec {
            fmv::Square from, to;
            fmv::PieceType piece;
            fmv::Color side;
        };
        MoveSpec moves[] = {
            { fmv::toSquare(6, 6), fmv::toSquare(6, 5), fmv::PieceType::Pawn,   fmv::Color::Black },
            { fmv::toSquare(2, 2), fmv::toSquare(2, 3), fmv::PieceType::Pawn,   fmv::Color::White },
            { fmv::toSquare(1, 7), fmv::toSquare(6, 2), fmv::PieceType::Bishop, fmv::Color::Black },
        };

        fmv::UndoState undos[3];
        for (int i = 0; i < 3; ++i) {
            fmv::Move m;
            m.kind = fmv::MoveKind::Board;
            m.from = moves[i].from;
            m.to = moves[i].to;
            m.piece = moves[i].piece;
            m.promote = false;
            bool ok = pos.doMove(m, moves[i].side, undos[i]);
            QVERIFY(ok);
            verifyBitboardConsistency(pos);
        }

        // 逆順にundo
        for (int i = 2; i >= 0; --i) {
            pos.undoMove(undos[i], moves[i].side);
            verifyBitboardConsistency(pos);
        }

        QCOMPARE(pos.board, original.board);
    }
};

QTEST_MAIN(TestFmvPosition)
#include "tst_fmvposition.moc"
