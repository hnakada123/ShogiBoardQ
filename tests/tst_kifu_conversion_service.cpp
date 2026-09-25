/// @file tst_kifu_conversion_service.cpp
/// @brief KifuConversionService のユニットテスト（fixtures の各形式を相互変換する）

#include <QtTest>

#include "kifuconversionservice.h"
#include "sfenutils.h"

using Format = KifuConversionService::Format;

class TestKifuConversionService : public QObject
{
    Q_OBJECT

private:
    static QString fixture(const QString& name)
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

    static KifuConversionService::Result convertFile(const QString& name, Format output, Format input = Format::Auto)
    {
        KifuConversionService::Request request;
        request.inputPath = fixture(name);
        request.inputFormat = input;
        request.outputFormat = output;
        return KifuConversionService::convert(request);
    }

    static const QStringList& basicMoves()
    {
        static const QStringList moves = {
            QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"), QStringLiteral("8c8d"),
            QStringLiteral("2f2e"), QStringLiteral("8d8e"), QStringLiteral("6i7h")};
        return moves;
    }

private slots:
    void kifToUsiKeepsMainline()
    {
        const auto result = convertFile(QStringLiteral("test_basic.kif"), Format::Usi);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.inputFormat, Format::Kif);
        QCOMPARE(result.initialSfen, SfenUtils::hirateSfen());
        QCOMPARE(result.usiMoves, basicMoves());
        QCOMPARE(result.sfens.size(), basicMoves().size() + 1);
        QCOMPARE(result.lines.size(), 1);
        QVERIFY(result.lines.first().startsWith(QStringLiteral("position startpos moves 7g7f 3c3d")));
    }

    void everyFormatRoundTripsToSameMoves()
    {
        const QStringList files = {QStringLiteral("test_basic.kif"), QStringLiteral("test_basic.ki2"),
                                   QStringLiteral("test_basic.csa"), QStringLiteral("test_basic.jkf"),
                                   QStringLiteral("test_basic.usi"), QStringLiteral("test_basic.usen")};
        const QList<Format> outputs = {Format::Kif, Format::Ki2, Format::Csa, Format::Jkf, Format::Usi, Format::Usen, Format::Sfen};
        for (const QString& file : files) {
            for (Format output : outputs) {
                const auto result = convertFile(file, output);
                QVERIFY2(result.ok, qPrintable(file + QStringLiteral(" -> ") + KifuConversionService::formatName(output)
                                               + QStringLiteral(": ") + result.error));
                QVERIFY2(result.usiMoves == basicMoves(), qPrintable(file + QStringLiteral(": ") + result.usiMoves.join(QLatin1Char(' '))));
                QVERIFY(!result.lines.isEmpty());
            }
        }
    }

    void kifHeaderCarriesPlayerNames()
    {
        const auto csa = convertFile(QStringLiteral("test_basic.kif"), Format::Csa);
        QVERIFY2(csa.ok, qPrintable(csa.error));
        QVERIFY(csa.lines.contains(QStringLiteral("N+テスト先手")));
        QVERIFY(csa.lines.contains(QStringLiteral("N-テスト後手")));

        const auto kif = convertFile(QStringLiteral("test_basic.csa"), Format::Kif);
        QVERIFY2(kif.ok, qPrintable(kif.error));
        QVERIFY(kif.lines.filter(QStringLiteral("先手：")).size() == 1);
    }

    void textInputIsAutoDetected()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d");
        request.outputFormat = Format::Sfen;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.inputFormat, Format::Usi);
        QCOMPARE(result.sfens.size(), 3);
        QCOMPARE(result.lines, result.sfens);

        KifuConversionService::Request csa;
        csa.text = QStringLiteral("V2.2\nN+A\nN-B\nPI\n+\n+7776FU\n-3334FU\n%TORYO\n");
        csa.outputFormat = Format::Kif;
        const auto csaResult = KifuConversionService::convert(csa);
        QVERIFY2(csaResult.ok, qPrintable(csaResult.error));
        QCOMPARE(csaResult.inputFormat, Format::Csa);
        QCOMPARE(csaResult.usiMoves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
    }

    void branchesAreReportedAndMainlineKept()
    {
        const auto result = convertFile(QStringLiteral("test_branch.kif"), Format::Kif);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.hasBranches);
        QVERIFY(!result.usiMoves.isEmpty());
    }

    void reportsErrors()
    {
        KifuConversionService::Request request;
        request.inputPath = fixture(QStringLiteral("does_not_exist.kif"));
        request.outputFormat = Format::Kif;
        QVERIFY(!KifuConversionService::convert(request).ok);

        KifuConversionService::Request both;
        both.inputPath = fixture(QStringLiteral("test_basic.kif"));
        both.text = QStringLiteral("x");
        both.outputFormat = Format::Kif;
        QVERIFY(!KifuConversionService::convert(both).ok);

        QVERIFY(!KifuConversionService::parseFormat(QStringLiteral("pgn")).has_value());
        QCOMPARE(KifuConversionService::formatFromPath(QStringLiteral("/tmp/a.KI2U")), Format::Ki2);
    }
};

QTEST_GUILESS_MAIN(TestKifuConversionService)
#include "tst_kifu_conversion_service.moc"
