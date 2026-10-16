/// @file tst_kifu_conversion_service.cpp
/// @brief KifuConversionService のユニットテスト（fixtures の各形式を相互変換する）

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "kifuconversionservice.h"
#include "sfenutils.h"
#include "kifuloadparser.h"

using Format = KifuFormat;

class TestKifuConversionService : public QObject
{
    Q_OBJECT

private:
    static QString fixture(const QString& name)
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

    static KifuConversionService::Result convertFile(const QString& name, Format output, std::optional<KifuFormat> input = std::nullopt)
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
    void unreplayableMoveIsRejected_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("format");
        QTest::newRow("usi-empty-source") << QStringLiteral("position startpos moves 7g7f 3c3d 7g7e") << int(Format::Usi);
        QTest::newRow("kif-empty-source") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n3 ７五歩(77)\n") << int(Format::Kif);
        QTest::newRow("kif-variation-empty-source") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n変化：3手\n3 ７五歩(77)\n") << int(Format::Kif);
    }
    void unreplayableMoveIsRejected()
    {
        QFETCH(QString, text);
        QFETCH(int, format);
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = static_cast<Format>(format);
        request.outputFormat = Format::Sfen;
        const auto result = KifuConversionService::convert(request);
        QVERIFY(!result.ok);
        QVERIFY(result.error.contains(QStringLiteral("3")));
        QVERIFY(result.error.contains(QStringLiteral("Source square is empty")));
        QVERIFY(result.lines.isEmpty());
        const auto loaded = KifuLoadParser::parseText(text);
        QVERIFY(!loaded.success);
        QVERIFY(!loaded.error.isEmpty());
    }

    void transposedPositionsKeepVariationParent_data()
    {
        QTest::addColumn<int>("format");
        QTest::newRow("jkf") << static_cast<int>(Format::Jkf);
        QTest::newRow("kif") << static_cast<int>(Format::Kif);
        QTest::newRow("ki2") << static_cast<int>(Format::Ki2);
        QTest::newRow("usen") << static_cast<int>(Format::Usen);
    }

    void transposedPositionsKeepVariationParent()
    {
        QFETCH(int, format);
        auto move = [](int fromX, int fromY, int toX, int toY, int color) {
            return QJsonObject{{"move", QJsonObject{{"color", color}, {"piece", "FU"},
                {"from", QJsonObject{{"x", fromX}, {"y", fromY}}},
                {"to", QJsonObject{{"x", toX}, {"y", toY}}}}}};
        };
        auto branchLast = move(6, 7, 6, 6, 0);
        branchLast["forks"] = QJsonArray{QJsonArray{move(5, 7, 5, 6, 0)}};
        const QJsonArray branch{move(2, 7, 2, 6, 0), move(8, 3, 8, 4, 1),
                                move(7, 7, 7, 6, 0), move(3, 3, 3, 4, 1), branchLast};
        auto first = move(7, 7, 7, 6, 0);
        first["forks"] = QJsonArray{branch};
        const QJsonObject record{{"initial", QJsonObject{{"preset", "HIRATE"}}},
            {"moves", QJsonArray{QJsonObject{}, first, move(3, 3, 3, 4, 1),
                                 move(2, 7, 2, 6, 0), move(8, 3, 8, 4, 1), move(2, 6, 2, 5, 0)}}};
        KifuConversionService::Request request;
        request.text = QString::fromUtf8(QJsonDocument(record).toJson());
        request.inputFormat = Format::Jkf;
        request.outputFormat = static_cast<Format>(format);
        const auto encoded = KifuConversionService::convert(request);
        QVERIFY2(encoded.ok, qPrintable(encoded.error));
        request.text = encoded.lines.join(QLatin1Char('\n'));
        request.inputFormat = request.outputFormat;
        request.outputFormat = Format::Jkf;
        const auto decoded = KifuConversionService::convert(request);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        const auto moves = QJsonDocument::fromJson(decoded.lines.join(QLatin1Char('\n')).toUtf8())
                               .object().value("moves").toArray();
        QCOMPARE(moves.size(), 6);
        QVERIFY(moves.at(5).toObject().value("forks").toArray().isEmpty());
        const auto forks = moves.at(1).toObject().value("forks").toArray();
        QCOMPARE(forks.size(), 1);
        const auto nested = forks.at(0).toArray().at(4).toObject().value("forks").toArray();
        QCOMPARE(nested.size(), 1);
        QCOMPARE(nested.at(0).toArray().at(0).toObject().value("move").toObject()
                     .value("to").toObject().value("x").toInt(), 5);
    }

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
                QVERIFY2(result.ok, qPrintable(file + QStringLiteral(" -> ") + KifuFormats::name(output)
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

        QVERIFY(!KifuFormats::fromName(QStringLiteral("pgn")).has_value());
        QCOMPARE(KifuFormats::fromPath(QStringLiteral("/tmp/a.KI2U")), Format::Ki2);
    }
};

QTEST_GUILESS_MAIN(TestKifuConversionService)
#include "tst_kifu_conversion_service.moc"
