#include <QtTest>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "boundedfile.h"
#include "kifutextdecoder.h"
#include "shiftjiscodec.h"

class TestKifuTextDecoder : public QObject
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

        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kifutextdecoder_XXXXXX.txt"));
        QVERIFY(file.open());
        QCOMPARE(file.write(content), content.size());
        file.close();

        QStringList lines;
        QString usedEncoding;
        QString warn;
        QVERIFY(KifuTextDecoder::readAllLinesAuto(file.fileName(), lines, &usedEncoding, &warn));

        QCOMPARE(usedEncoding, expectedEncoding);
        QCOMPARE(lines, expectedLines);
        QVERIFY2(warn.isEmpty(), qPrintable(warn));
    }

    void readAllLinesAuto_decodesShiftJis()
    {
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/kifutextdecoder_XXXXXX.kif"));
        QVERIFY(file.open());
        const QByteArray content = QByteArray::fromHex("81a38256985a95e00d0a");  // "▲７六歩\r\n"
        QCOMPARE(file.write(content), content.size());
        file.close();

        QStringList lines;
        QString usedEncoding;
        QVERIFY(KifuTextDecoder::readAllLinesAuto(file.fileName(), lines, &usedEncoding));

        QCOMPARE(usedEncoding, QStringLiteral("cp932"));
        QCOMPARE(lines, (QStringList{QStringLiteral("▲７六歩"), QString()}));
    }

    /// 先頭の行の文字コードの宣言（KIF・KI2・CSA）は、宣言どおりに読めるときに従う。CSA は名前とコメントの
    /// ほかは ASCII なので、Shift_JIS の名前が UTF-8 としても読めてしまう（「遙子」は UTF-8 なら「ꡎq」）
    void declarationDecidesTheEncoding_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<QString>("expectedEncoding");
        QTest::addColumn<QString>("expectedText");

        const QByteArray yoko = QByteArray::fromHex("eaa18e71");  // "遙子"（CP932。UTF-8 としても読める）
        const QByteArray habu = QByteArray::fromHex("894890b691508ea1");  // "羽生善治"（CP932）
        QTest::newRow("kif shift_jis")
            << QByteArray("#KIF version=2.0 encoding=Shift_JIS\r\n") + yoko
            << QStringLiteral("cp932(declared)")
            << QStringLiteral("#KIF version=2.0 encoding=Shift_JIS\r\n遙子");
        QTest::newRow("kif utf-8")
            << QByteArray("#KIF version=2.0 encoding=UTF-8\n") + QStringLiteral("先手：遙子").toUtf8()
            << QStringLiteral("utf-8(declared)")
            << QStringLiteral("#KIF version=2.0 encoding=UTF-8\n先手：遙子");
        QTest::newRow("csa v3")
            << QByteArray("'CSA encoding=SHIFT_JIS\nV3.0\nN+") + yoko
            << QStringLiteral("cp932(declared)")
            << QStringLiteral("'CSA encoding=SHIFT_JIS\nV3.0\nN+遙子");
        QTest::newRow("csa legacy")
            << QByteArray("V2.2\n'encoding=Shift_JIS\nN+") + yoko
            << QStringLiteral("cp932(declared)")
            << QStringLiteral("V2.2\n'encoding=Shift_JIS\nN+遙子");
        // 宣言どおりに読めなければ、宣言を使わずに判別する
        QTest::newRow("unreadable declaration")
            << QByteArray("'CSA encoding=UTF-8\nN+") + habu
            << QStringLiteral("cp932")
            << QStringLiteral("'CSA encoding=UTF-8\nN+羽生善治");
        // 読まない値の宣言と、コメントの行でない "encoding=" は宣言にしない
        QTest::newRow("unknown value")
            << QByteArray("#KIF version=2.0 encoding=ISO-2022-JP\n") + QStringLiteral("先手").toUtf8()
            << QStringLiteral("utf-8")
            << QStringLiteral("#KIF version=2.0 encoding=ISO-2022-JP\n先手");
        QTest::newRow("not a comment")
            << QByteArray("N+encoding=Shift_JIS\n") + QStringLiteral("先手").toUtf8()
            << QStringLiteral("utf-8")
            << QStringLiteral("N+encoding=Shift_JIS\n先手");
        // BOM は宣言より先に決める
        QTest::newRow("bom first")
            << QByteArray("\xEF\xBB\xBF'CSA encoding=Shift_JIS\nN+") + QStringLiteral("遙子").toUtf8()
            << QStringLiteral("utf-8(bom)")
            << QStringLiteral("'CSA encoding=Shift_JIS\nN+遙子");
    }

    void declarationDecidesTheEncoding()
    {
        QFETCH(QByteArray, content);
        QFETCH(QString, expectedEncoding);
        QFETCH(QString, expectedText);

        QString usedEncoding;
        QString warn;
        QCOMPARE(KifuTextDecoder::decodeAuto(content, &usedEncoding, &warn), expectedText);
        QCOMPARE(usedEncoding, expectedEncoding);
        QVERIFY2(warn.isEmpty(), qPrintable(warn));
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

    /// 通常のファイルだけを上限まで読む（ディレクトリ・FIFO・デバイス・大きすぎるファイルは読まない。セキュリティ監査 S14）
    void readsOnlyBoundedRegularFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QByteArray bytes;
        QCOMPARE(BoundedFile::read(dir.path(), 100, bytes), BoundedFile::Result::NotRegularFile);
        QCOMPARE(BoundedFile::read(dir.filePath(QStringLiteral("missing.kif")), 100, bytes), BoundedFile::Result::OpenFailed);
        const QString path = dir.filePath(QStringLiteral("a.kif"));
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QByteArray(101, 'x'));
        }
        QCOMPARE(BoundedFile::read(path, 100, bytes), BoundedFile::Result::TooLarge);
        QVERIFY(bytes.isEmpty());
        QCOMPARE(BoundedFile::read(path, 101, bytes), BoundedFile::Result::Ok);
        QCOMPARE(bytes.size(), 101);
#ifndef Q_OS_WIN
        if (QFileInfo::exists(QStringLiteral("/dev/zero"))) {
            QStringList lines;
            QString warn;
            QVERIFY(!KifuTextDecoder::readAllLinesAuto(QStringLiteral("/dev/zero"), lines, nullptr, &warn));
            QVERIFY(warn.contains(QStringLiteral("not a regular file")));
        }
#endif
    }
};

QTEST_MAIN(TestKifuTextDecoder)

#include "tst_kifutextdecoder.moc"
