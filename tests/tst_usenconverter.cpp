#include <QtTest>
#include <QCoreApplication>

#include "usentosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestUsenConverter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    void detectInitialSfen()
    {
        QString sfen = UsenToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.usen")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = UsenToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void decodeUsenMoves()
    {
        // Decode the base36-encoded moves from the USEN string
        QString usen = QStringLiteral("~0.7ku2jm6y236e5t24be9qc");
        // Extract move portion (after "~0.")
        QString moveStr = usen.mid(3);
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(usen);
        QCOMPARE(decoded.size(), 7);
        QCOMPARE(decoded, kExpectedUsiMoves);
    }

    void crossFormat_matchesKif()
    {
        // Verify USEN produces the same USI moves as other formats
        QString error;
        QStringList usenMoves = UsenToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(usenMoves, kExpectedUsiMoves);
    }

    void detectInitialSfen_handicap()
    {
        // 駒落ち局面（四枚落ち）: 初期局面が ~ の前にエンコードされている
        QString label;
        QString sfen = UsenToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_handicap.usen")), &label);
        QCOMPARE(label, QStringLiteral("局面指定"));
        QCOMPARE(sfen, QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
    }

    void parseWithVariations_handicap()
    {
        // 駒落ちUSENの本譜をパースし、指し手が正しくデコードされることを確認
        KifParseResult res;
        QString error;
        bool ok = UsenToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_handicap.usen")), res, &error);
        QVERIFY2(ok, qPrintable(error));

        // baseSfen が駒落ち局面であること
        QCOMPARE(res.mainline.baseSfen,
                 QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));

        // 11手の指し手 + 投了が含まれること
        QCOMPARE(res.mainline.usiMoves.size(), 11);

        // 先頭の指し手を確認（後手番から開始: △3二銀, ▲7六歩）
        QCOMPARE(res.mainline.usiMoves.at(0), QStringLiteral("3a2b"));
        QCOMPARE(res.mainline.usiMoves.at(1), QStringLiteral("7g7f"));
    }
};

QTEST_MAIN(TestUsenConverter)
#include "tst_usenconverter.moc"
