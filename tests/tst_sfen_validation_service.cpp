/// @file tst_sfen_validation_service.cpp
/// @brief SfenValidationService のユニットテスト

#include <QtTest>

#include "sfenutils.h"
#include "sfenvalidationservice.h"

class TestSfenValidationService : public QObject
{
    Q_OBJECT

private slots:
    void acceptsStartposAndHirate()
    {
        const SfenValidation v = SfenValidationService::validate(QStringLiteral("startpos"));
        QVERIFY2(v.valid, qPrintable(v.errors.join(QStringLiteral("; "))));
        QCOMPARE(v.normalizedSfen, SfenUtils::hirateSfen());
        QCOMPARE(v.turn, QStringLiteral("b"));
        QCOMPARE(v.moveNumber, 1);
        QVERIFY(v.blackKing && v.whiteKing);
        QVERIFY(!v.inCheck);
        QCOMPARE(v.legalMoveCount, 30);
        QVERIFY(v.blackHand.isEmpty() && v.whiteHand.isEmpty());

        const SfenValidation p = SfenValidationService::validate(QStringLiteral("position sfen ") + SfenUtils::hirateSfen());
        QVERIFY(p.valid);
    }

    void reportsHandsAndMissingMoveNumber()
    {
        // 飛角と歩を盤から外して持駒にした局面（手数フィールド省略）
        const SfenValidation v = SfenValidationService::validate(
            QStringLiteral("lnsgkgsnl/9/9/9/9/9/9/9/LNSGKGSNL w RB9Prb9p"));
        QVERIFY2(v.valid, qPrintable(v.errors.join(QStringLiteral("; "))));
        QCOMPARE(v.turn, QStringLiteral("w"));
        QCOMPARE(v.blackHand.value(QStringLiteral("R")), 1);
        QCOMPARE(v.blackHand.value(QStringLiteral("B")), 1);
        QCOMPARE(v.blackHand.value(QStringLiteral("P")), 9);
        QCOMPARE(v.whiteHand.value(QStringLiteral("R")), 1);
        QCOMPARE(v.whiteHand.value(QStringLiteral("P")), 9);
        QVERIFY(v.normalizedSfen.endsWith(QStringLiteral(" 1")));
    }

    void acceptsKinglessAttackerTsumePosition()
    {
        // 玉方の玉だけの詰将棋局面（攻方玉なし）は有効
        const SfenValidation v = SfenValidationService::validate(QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b N 1"));
        QVERIFY2(v.valid, qPrintable(v.errors.join(QStringLiteral("; "))));
        QVERIFY(!v.blackKing);
        QVERIFY(v.whiteKing);
        QVERIFY(v.legalMoveCount > 0);
    }

    void detectsCheck()
    {
        // 後手手番で 5一の玉に 5二金の王手
        const SfenValidation v = SfenValidationService::validate(QStringLiteral("4k4/4G4/9/9/9/9/9/9/4K4 w - 1"));
        QVERIFY(v.valid);
        QVERIFY(v.inCheck);
        // 先手手番だと手番でない後手玉に王手が残る不正局面
        const SfenValidation bad = SfenValidationService::validate(QStringLiteral("4k4/4G4/9/9/9/9/9/9/4K4 b - 1"));
        QVERIFY(!bad.valid);
        QVERIFY(bad.opponentInCheck);
        QVERIFY(!bad.normalizedSfen.isEmpty());
    }

    void rejectsMalformedInput()
    {
        QVERIFY(!SfenValidationService::validate(QString()).valid);
        QVERIFY(!SfenValidationService::validate(QStringLiteral("hello")).valid);
        QVERIFY(!SfenValidationService::validate(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1")).valid);
        QVERIFY(!SfenValidationService::validate(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 0")).valid);
        // 段の長さが 8 マスしかない
        QVERIFY(!SfenValidationService::validate(QStringLiteral("lnsgkgsn/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")).valid);
        // 玉が両方無い
        QVERIFY(!SfenValidationService::validate(QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1")).valid);
        // 歩が 19 枚
        QVERIFY(!SfenValidationService::validate(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1")).valid);
    }

    void appliesUsiMoves()
    {
        QString error;
        const QString after = SfenValidationService::applyUsiMoves(QStringLiteral("startpos"),
                                                                    {QStringLiteral("7g7f"), QStringLiteral("3c3d")}, &error);
        QVERIFY2(!after.isEmpty(), qPrintable(error));
        QCOMPARE(after, QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"));

        const QString illegal = SfenValidationService::applyUsiMoves(QStringLiteral("startpos"), {QStringLiteral("7g7e")}, &error);
        QVERIFY(illegal.isEmpty());
        QVERIFY(error.contains(QStringLiteral("7g7e")));
    }
};

QTEST_GUILESS_MAIN(TestSfenValidationService)
#include "tst_sfen_validation_service.moc"
