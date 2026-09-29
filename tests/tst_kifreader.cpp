#include <QtTest>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>

#include "kifreader.h"
#include "shiftjiscodec.h"

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

    void readAllLinesAuto_decodesShiftJis()
    {
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kifreader_XXXXXX.kif"));
        QVERIFY(file.open());
        const QByteArray content = QByteArray::fromHex("81a38256985a95e00d0a");  // "▲７六歩\r\n"
        QCOMPARE(file.write(content), content.size());
        file.close();

        QStringList lines;
        QString usedEncoding;
        QVERIFY(KifReader::readAllLinesAuto(file.fileName(), lines, &usedEncoding));

        QCOMPARE(usedEncoding, QStringLiteral("cp932"));
        QCOMPARE(lines, (QStringList{QStringLiteral("▲７六歩"), QString()}));
    }

    // Qt が Shift_JIS を扱えない環境（ICU なし）で使う OS の変換機能を直接確認する
    void shiftJisPlatform_roundTrip()
    {
        const QByteArray bytes = QByteArray::fromHex("81a38256985a95e08740");  // "▲７六歩①"（①は CP932 拡張）
        const auto decoded = ShiftJisCodec::decodeWithPlatform(bytes);
        QVERIFY(decoded.has_value());
        QCOMPARE(*decoded, QStringLiteral("▲７六歩①"));

        const auto encoded = ShiftJisCodec::encodeWithPlatform(*decoded);
        QVERIFY(encoded.has_value());
        QCOMPARE(*encoded, bytes);
    }

    void shiftJisPlatform_rejectsInvalidInput()
    {
        QVERIFY(!ShiftJisCodec::decodeWithPlatform(QByteArray::fromHex("81")).has_value());
        QVERIFY(!ShiftJisCodec::encodeWithPlatform(QStringLiteral("\U0001F600")).has_value());
        QVERIFY(!ShiftJisCodec::encode(QStringLiteral("\U0001F600")).has_value());
    }
};

QTEST_MAIN(TestKifReader)

#include "tst_kifreader.moc"
