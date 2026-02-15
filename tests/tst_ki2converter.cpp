#include <QtTest>
#include <QCoreApplication>

#include "ki2tosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestKi2Converter : public QObject
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
        QString sfen = Ki2ToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.ki2")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = Ki2ToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        QList<KifDisplayItem> items = Ki2ToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // Returns opening item (ply 0) + 7 moves = 8
        QCOMPARE(items.size(), 8);
    }
};

QTEST_MAIN(TestKi2Converter)
#include "tst_ki2converter.moc"
