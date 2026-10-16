#include <QtTest>

#include "enginemovevalidator.h"
#include "fmvconverter.h"
#include "fmvposition.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestFmvConverter : public QObject
{
    Q_OBJECT

private slots:
    void toEnginePosition_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        bool ok = fmv::Converter::toEnginePosition(pos, board.position());
        QVERIFY(ok);

        // 先手玉の位置（8段目の4筋: rank=8, file=4 → sq=76）
        QCOMPARE(pos.kingSq[0], fmv::toSquare(4, 8)); // 先手玉
        QCOMPARE(pos.kingSq[1], fmv::toSquare(4, 0)); // 後手玉

        // 駒数チェック
        QCOMPARE(pos.occupied.count(), 40); // 平手は40枚
        QCOMPARE(pos.colorOcc[0].count(), 20); // 先手20枚
        QCOMPARE(pos.colorOcc[1].count(), 20); // 後手20枚

        // 歩の数
        QCOMPARE(pos.pieceOcc[0][static_cast<int>(fmv::PieceType::Pawn)].count(), 9);
        QCOMPARE(pos.pieceOcc[1][static_cast<int>(fmv::PieceType::Pawn)].count(), 9);

        // 持ち駒は空
        for (int c = 0; c < 2; ++c) {
            for (int h = 0; h < static_cast<int>(fmv::HandType::HandTypeNb); ++h) {
                QCOMPARE(pos.hand[c][static_cast<std::size_t>(h)], 0);
            }
        }
    }

    void toEnginePosition_withHand()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P2g 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.position());

        // 先手の持ち歩
        QCOMPARE(pos.hand[0][static_cast<std::size_t>(fmv::HandType::Pawn)], 1);
        // 後手の持ち金
        QCOMPARE(pos.hand[1][static_cast<std::size_t>(fmv::HandType::Gold)], 2);
    }

    void toEngineMove_boardMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.position());

        // 7六歩（内部の升は0始まり）
        const UsiMove sm = UsiMove::fromString(u"7g7f");
        fmv::ConvertedMove cm;
        bool ok = fmv::Converter::toEngineMove(cm, Turn::Black, pos, sm);
        QVERIFY(ok);

        QCOMPARE(cm.nonPromote.kind, fmv::MoveKind::Board);
        QCOMPARE(cm.nonPromote.from, fmv::toSquare(6, 6));
        QCOMPARE(cm.nonPromote.to, fmv::toSquare(6, 5));
        QCOMPARE(cm.nonPromote.piece, fmv::PieceType::Pawn);
        QVERIFY(!cm.nonPromote.promote);
        QVERIFY(!cm.hasPromoteVariant); // 6段目→5段目は敵陣外
    }

    void toEngineMove_drop()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b S 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.position());

        // 先手銀打ち
        const UsiMove sm = UsiMove::fromString(u"S*5e");
        fmv::ConvertedMove cm;
        bool ok = fmv::Converter::toEngineMove(cm, Turn::Black, pos, sm);
        QVERIFY(ok);

        QCOMPARE(cm.nonPromote.kind, fmv::MoveKind::Drop);
        QCOMPARE(cm.nonPromote.to, fmv::toSquare(4, 4));
        QCOMPARE(cm.nonPromote.piece, fmv::PieceType::Silver);
        QVERIFY(!cm.hasPromoteVariant);
    }

    void toEngineMove_promotionZone()
    {
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        ShogiBoard board;
        board.setSfen(sfen);

        fmv::EnginePosition pos;
        fmv::Converter::toEnginePosition(pos, board.position());

        // 5四歩→5三: 3段目は敵陣なので成り可
        const UsiMove sm = UsiMove::fromString(u"5d5c");
        fmv::ConvertedMove cm;
        bool ok = fmv::Converter::toEngineMove(cm, Turn::Black, pos, sm);
        QVERIFY(ok);
        QVERIFY(cm.hasPromoteVariant);
        QVERIFY(cm.promote.promote);
    }

    void toColor()
    {
        QCOMPARE(fmv::Converter::toColor(Turn::Black), fmv::Color::Black);
        QCOMPARE(fmv::Converter::toColor(Turn::White), fmv::Color::White);
    }
};

QTEST_MAIN(TestFmvConverter)
#include "tst_fmvconverter.moc"
