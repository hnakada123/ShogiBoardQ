#include <QtTest>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>

#include "kifreader.h"

class TestKifReader : public QObject
{
    Q_OBJECT

private slots:
    void readAllLinesAuto_detectsBom_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<QString>("expectedEncoding");
        QTest::addColumn<QStringList>("expectedLines");

        QTest::newRow("utf32le")
            << QByteArray::fromHex("fffe00006100000062000000630000000a000000")
            << QStringLiteral("utf-32le(bom)")
            << QStringList{QStringLiteral("abc"), QString()};

        QTest::newRow("utf32be")
            << QByteArray::fromHex("0000feff00000041000000420000000a")
            << QStringLiteral("utf-32be(bom)")
            << QStringList{QStringLiteral("AB"), QString()};
    }

    void readAllLinesAuto_detectsBom()
    {
        QFETCH(QByteArray, content);
        QFETCH(QString, expectedEncoding);
        QFETCH(QStringList, expectedLines);

        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kifreader_XXXXXX.txt"));
        QVERIFY(file.open());
        QCOMPARE(file.write(content), content.size());
        file.close();

        QStringList lines;
        QString usedEncoding;
        QString warn;
        QVERIFY(KifReader::readAllLinesAuto(file.fileName(), lines, &usedEncoding, &warn));

        QCOMPARE(usedEncoding, expectedEncoding);
        QCOMPARE(lines, expectedLines);
        QVERIFY2(warn.isEmpty(), qPrintable(warn));
    }
};

QTEST_MAIN(TestKifReader)

#include "tst_kifreader.moc"
