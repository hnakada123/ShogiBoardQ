/// @file tst_kifuformat.cpp
/// @brief 棋譜の形式の判定（貼り付けたテキスト・ファイル・名前）の回帰テスト

#include <QtTest>
#include "shogiposition.h"

#include "bodtextgenerator.h"
#include "kifuformat.h"

class TestKifuFormat : public QObject
{
    Q_OBJECT

private slots:
    void detectFormat_data()
    {
        using Format = KifuFormat;
        QTest::addColumn<QString>("text");
        QTest::addColumn<Format>("expected");

        const QString sfen = QStringLiteral(
            "lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
        const QString bod = BodTextGenerator::generate(*Position::fromSfen(sfen), 4, QStringLiteral("△８四歩(83)"));
        QTest::newRow("bod-last-move") << bod << Format::Bod;
        QTest::newRow("bod-no-last-move") << BodTextGenerator::generate(*Position::fromSfen(sfen), 4, {}) << Format::Bod;
        QTest::newRow("bod-with-comment") << bod + QStringLiteral("\n*候補手は▲２五歩\n") << Format::Bod;
        QTest::newRow("ki2-with-bod") << bod + QStringLiteral("\n  ▲２五歩  △８五歩\n") << Format::Ki2;
        QTest::newRow("kif-with-bod") << bod + QStringLiteral("\n手数----指手---------消費時間--\n1 ２五歩(26)\n") << Format::Kif;
        QTest::newRow("kif-without-heading") << bod + QStringLiteral("\n  1 ２五歩(26)\n") << Format::Kif;
        QTest::newRow("ki2-header-comment") << QStringLiteral("先手：テスト\n*△３四歩を検討\n▲７六歩 △３四歩\n") << Format::Ki2;
        QTest::newRow("sfen") << sfen << Format::Sfen;
        QTest::newRow("usi-short") << QStringLiteral("startpos moves 7g7f") << Format::Usi;
        QTest::newRow("usi-sfen") << QStringLiteral("sfen ") + sfen + QStringLiteral(" moves 2g2f") << Format::Usi;
        QTest::newRow("sfen-no-ply") << sfen.left(sfen.lastIndexOf(QLatin1Char(' '))) << Format::Usi;
        QTest::newRow("usi-comment") << QStringLiteral("# comment\nstartpos moves 7g7f") << Format::Usi;
        QTest::newRow("json-array") << QStringLiteral("[{\"move\":{}}]") << Format::Jkf;
        QTest::newRow("usi-position") << QStringLiteral("position startpos moves 7g7f 3c3d") << Format::Usi;
        QTest::newRow("jkf-object") << QStringLiteral("{\"header\":{},\"moves\":[{}]}") << Format::Jkf;
        QTest::newRow("csa-header") << QStringLiteral("V2.2\nPI\n+\n+7776FU\nT1") << Format::Csa;
        QTest::newRow("csa-moves") << QStringLiteral("+7776FU\n-3334FU") << Format::Csa;
        QTest::newRow("usen-custom-position") << QStringLiteral("1nsgkgsn1_9_ppppppppp_9_9_9_PPPPPPPPP_1B5R1_LNSGKGSNL.w.-~0.09k7ku.r") << Format::Usen;
        QTest::newRow("usen") << QStringLiteral("~0.7ku.r~1.0e4.r") << Format::Usen;
        QTest::newRow("kif-tilde") << QStringLiteral("*https://example.org/~user\n手数----指手---------\n1 ７六歩(77)") << Format::Kif;
        QTest::newRow("csa-tilde") << QStringLiteral("'comment ~\nPI\n+\n+7776FU") << Format::Csa;
    }

    void detectFormat()
    {
        QFETCH(QString, text);
        QFETCH(KifuFormat, expected);
        QCOMPARE(KifuFormats::detect(text), expected);
    }

    // 開ける拡張子は形式の表から作る（JKF の .json を含み、局面図は含まない）
    void readableSuffixesComeFromTheTable()
    {
        const QStringList all = KifuFormats::readableSuffixes();
        for (const char* suffix : {"kif", "kifu", "ki2", "ki2u", "csa", "jkf", "json", "usi", "usen", "sfen"})
            QVERIFY2(all.contains(QLatin1String(suffix)), suffix);
        QVERIFY(!all.contains(QStringLiteral("bod")));
        QCOMPARE(KifuFormats::readableSuffixes(KifuFormat::Jkf), QStringList({QStringLiteral("jkf"), QStringLiteral("json")}));
        QCOMPARE(KifuFormats::readableSuffixes(KifuFormat::Kif), QStringList({QStringLiteral("kifu"), QStringLiteral("kif")}));
        QVERIFY(KifuFormats::isReadableFile(QStringLiteral("/games/a.JSON")));
        QVERIFY(!KifuFormats::isReadableFile(QStringLiteral("/games/a.txt")));
    }

    // 拡張子で決まる形式と、.json・未知の拡張子の内容による判定
    void detectFileFormat_data()
    {
        QTest::addColumn<QString>("suffix");
        QTest::addColumn<QByteArray>("content");
        QTest::addColumn<int>("expected");
        using F = KifuFormat;
        const QByteArray kif = "手合割：平手\n   1 ７六歩(77)\n";
        QTest::newRow("kif") << "kif" << kif << int(F::Kif);
        QTest::newRow("kifu") << "kifu" << kif << int(F::Kif);
        QTest::newRow("ki2u") << "ki2u" << QByteArray("▲７六歩\n") << int(F::Ki2);
        QTest::newRow("csa") << "csa" << QByteArray("PI\n+\n+7776FU\n") << int(F::Csa);
        QTest::newRow("jkf") << "jkf" << QByteArray("{}") << int(F::Jkf);
        QTest::newRow("usen") << "usen" << QByteArray("~0.7ku.") << int(F::Usen);
        QTest::newRow("sfen") << "sfen" << QByteArray("position startpos moves 7g7f") << int(F::Usi);
        // JKF は .json で配布されることも多い
        QTest::newRow("json-jkf") << "json" << QByteArray("{\"header\":{},\"moves\":[{}]}") << int(F::Jkf);
        QTest::newRow("txt-csa") << "txt" << QByteArray("V2.2\nPI\n+\n+7776FU\n") << int(F::Csa);
        QTest::newRow("txt-kif") << "txt" << kif << int(F::Kif);
    }

    void detectFileFormat()
    {
        QFETCH(QString, suffix);
        QFETCH(QByteArray, content);
        QFETCH(int, expected);
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/detect_XXXXXX.") + suffix);
        QVERIFY(file.open());
        file.write(content);
        file.close();
        QCOMPARE(int(KifuFormats::detectFile(file.fileName())), expected);
    }

    // CLI・自動化 API の形式名と拡張子
    void namesAndExtensions()
    {
        QCOMPARE(KifuFormats::fromName(QStringLiteral("KIFU")), KifuFormat::Kif);
        QCOMPARE(KifuFormats::fromName(QStringLiteral("ki2u")), KifuFormat::Ki2);
        QCOMPARE(KifuFormats::fromName(QStringLiteral("auto")), std::nullopt);
        QCOMPARE(KifuFormats::name(KifuFormat::Kif), QStringLiteral("kif"));
        QCOMPARE(KifuFormats::name(KifuFormat::Sfen), QStringLiteral("sfen"));
        QCOMPARE(KifuFormats::fromPath(QStringLiteral("/tmp/a.CSA")), KifuFormat::Csa);
        QCOMPARE(KifuFormats::fromPath(QStringLiteral("/tmp/a.txt")), std::nullopt);
        QCOMPARE(KifuFormats::fromPath(QStringLiteral("/tmp/a.bod")), std::nullopt);
        QVERIFY(!KifuFormats::isSavable(KifuFormat::Sfen));
        QVERIFY(KifuFormats::isSavable(KifuFormat::Usen));
        // CLI・自動化 API の選択肢（保存できる形式の名前）
        QCOMPARE(KifuFormats::savableNames(),
                 QStringList({QStringLiteral("kif"), QStringLiteral("ki2"), QStringLiteral("csa"), QStringLiteral("jkf"),
                              QStringLiteral("usi"), QStringLiteral("usen")}));
    }

    // 保存できる拡張子の表（保存ダイアログの並び・形式・文字コード）
    void saveTargetsComeFromTheTable()
    {
        QStringList suffixes;
        for (const KifuFormats::SaveTarget& target : KifuFormats::saveTargets()) {
            suffixes.append(target.suffix);
            QVERIFY(KifuFormats::isSavable(target.format));
            QCOMPARE(KifuFormats::fromName(target.suffix), target.format);
            QVERIFY(target.label);
            // Shift_JIS で書くのは .kif と .ki2 だけ
            QCOMPARE(target.encoding == KifuEncoding::ShiftJis,
                     target.suffix == QLatin1String("kif") || target.suffix == QLatin1String("ki2"));
        }
        QCOMPARE(suffixes, QStringList({QStringLiteral("kifu"), QStringLiteral("kif"), QStringLiteral("ki2u"),
                                        QStringLiteral("ki2"), QStringLiteral("csa"), QStringLiteral("jkf"),
                                        QStringLiteral("usen"), QStringLiteral("usi")}));
        const auto ki2 = KifuFormats::saveTargetForPath(QStringLiteral("/tmp/A.KI2"));
        QVERIFY(ki2);
        QCOMPARE(ki2->format, KifuFormat::Ki2);
        QCOMPARE(ki2->encoding, KifuEncoding::ShiftJis);
        QVERIFY(!KifuFormats::saveTargetForPath(QStringLiteral("/tmp/a.sfen")));
        QVERIFY(!KifuFormats::saveTargetForPath(QStringLiteral("/tmp/a")));
    }
};

QTEST_GUILESS_MAIN(TestKifuFormat)
#include "tst_kifuformat.moc"
