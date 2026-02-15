#include <QtTest>
#include <QCoreApplication>

#include "jkftosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestJkfConverter : public QObject
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
        QString sfen = JkfToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.jkf")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.jkf")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void mapPresetToSfen_hirate()
    {
        QString sfen = JkfToSfenConverter::mapPresetToSfen(QStringLiteral("HIRATE"));
        QCOMPARE(sfen, kHirateSfen);
    }
};

QTEST_MAIN(TestJkfConverter)
#include "tst_jkfconverter.moc"
