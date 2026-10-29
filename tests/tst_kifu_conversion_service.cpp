/// @file tst_kifu_conversion_service.cpp
/// @brief KifuConversionService のユニットテスト（fixtures の各形式を相互変換する）

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

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
    void terminalTextRoundTrip_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QString>("label");
        QTest::addColumn<int>("format");
        for (const QString& label : {QStringLiteral("トライ"), QStringLiteral("独自の終局理由"),
                                     QStringLiteral("独自の投了判断"), QStringLiteral("独自の勝ち"),
                                     QStringLiteral("不戦勝"), QStringLiteral("不戦敗")}) {
            for (const bool ki2 : {false, true}) {
                const QString source = ki2
                    ? QStringLiteral("手合割：平手\n▲７六歩\nまで1手で%1\n").arg(label)
                    : QStringLiteral("手合割：平手\n1 ７六歩(77)\n2 %1 ( 0:03/00:00:03)\n").arg(label);
                for (const Format format : {Format::Kif, Format::Ki2, Format::Jkf}) {
                    const QString name = label + QString::number(ki2) + QString::number(int(format));
                    QTest::newRow(qPrintable(name)) << source << label << int(format);
                }
            }
        }
    }

    void terminalTextRoundTrip()
    {
        QFETCH(QString, source);
        QFETCH(QString, label);
        QFETCH(int, format);
        KifuConversionService::Request request;
        request.text = source;
        request.outputFormat = static_cast<Format>(format);
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        QVERIFY2(saved.warnings.isEmpty(), qPrintable(saved.warnings.join(QLatin1Char('\n'))));
        const auto reread = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY2(reread.success, qPrintable(reread.error));
        QVERIFY2(reread.warning.isEmpty(), qPrintable(reread.warning));
        QCOMPARE(reread.record.mainline.entries.size(), 2);
        const RecordEntry& ending = reread.record.mainline.entries.last();
        QVERIFY(ending.isTerminal());
        QCOMPARE(ending.terminalLabel(), label);
        QCOMPARE(ending.mover, Turn::White);
        if (label == QStringLiteral("トライ")) QCOMPARE(ending.winner(), std::optional<Turn>(Turn::Black));
        else if (label.startsWith(QStringLiteral("独自"))) QVERIFY(!ending.winner());
    }

    void jkfSpecialPreservesTextButEmptySpecialRemainsError()
    {
        const auto custom = KifuLoadParser::parseText(QStringLiteral("{\"moves\":[{}, {\"special\":\"CUSTOM_END\"}]}"));
        QVERIFY(custom.success);
        QCOMPARE(custom.record.mainline.entries.last().terminal, TerminalType::Custom);
        QCOMPARE(custom.record.mainline.entries.last().terminalLabel(), QStringLiteral("CUSTOM_END"));
        for (const QString& value : {QStringLiteral("\"\""), QStringLiteral("\"  \""), QStringLiteral("null")}) {
            const auto invalid = KifuLoadParser::parseText(QStringLiteral("{\"moves\":[{}, {\"special\":%1}]}").arg(value));
            QVERIFY(invalid.success);
            QCOMPARE(invalid.record.mainline.entries.last().terminal, TerminalType::Error);
        }
    }

    void differentCustomEndingsStayInSeparateBranches()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("手合割：平手\n1 ７六歩(77)\n2 独自の結果A\n"
                                      "変化：2手\n2 独自の結果B\n");
        request.outputFormat = Format::Kif;
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        const auto loaded = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY(loaded.success);
        QCOMPARE(loaded.record.mainline.entries.last().terminalText, QStringLiteral("独自の結果A"));
        QCOMPARE(loaded.record.variations.size(), 1);
        QCOMPARE(loaded.record.variations.first().line.entries.last().terminalText, QStringLiteral("独自の結果B"));
    }

    void omittedDropOrBlockedOriginIsRecovered_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<QStringList>("moves");
        QTest::newRow("ki2-missing-hand") << QStringLiteral("手合割：平手\n▲５五歩 △３四歩\n")
            << QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")};
        QTest::newRow("ki2-blocked-bishop") << QStringLiteral("手合割：平手\n▲５五角 △３四歩\n")
            << QStringList{QStringLiteral("8h5e"), QStringLiteral("3c3d")};
        QTest::newRow("csa-missing-hand") << QStringLiteral("V3.0\nPI\n+\n+0055FU\nT1.234\n-3334FU\nT0.125\n")
            << QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")};
    }

    void omittedDropOrBlockedOriginIsRecovered()
    {
        QFETCH(QString, source);
        QFETCH(QStringList, moves);
        const auto loaded = KifuLoadParser::parseText(source);
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QVERIFY2(loaded.warning.isEmpty(), qPrintable(loaded.warning));
        QCOMPARE(loaded.record.mainline.usiMoves(), moves);
        if (source.startsWith(QLatin1String("V3.0"))) {
            QCOMPARE(loaded.record.mainline.entries.first().time.elapsedMs, 1234);
            QCOMPARE(loaded.record.mainline.entries.last().time.elapsedMs, 125);
        }
    }

    void usiDrawKeepsGenericDraw()
    {
        const auto loaded = KifuLoadParser::parseText(QStringLiteral("position startpos moves 7g7f draw"));
        QVERIFY(loaded.success);
        QVERIFY(loaded.warning.isEmpty());
        QCOMPARE(loaded.record.mainline.entries.last().terminal, TerminalType::Draw);
        QCOMPARE(loaded.record.mainline.entries.last().terminalLabel(), QStringLiteral("引き分け"));
    }

    /// 未終局の KI2 は指し手だけで終え、未知の終局行を作らない。
    void ki2UnfinishedRecordHasNoResultLine()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d");
        request.outputFormat = Format::Ki2;
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        for (const QString& line : saved.lines) QVERIFY(!line.startsWith(QStringLiteral("まで")));
        const auto reread = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY(reread.success);
        QCOMPARE(reread.record.mainline.usiMoves(), saved.usiMoves);
        QVERIFY(!reread.record.mainline.endsWithTerminal());
    }

    /// 0手の終局も USI の手順欄に置き、平手・任意局面とも読み戻せる。
    void usiTerminalWithoutMoves_data()
    {
        QTest::addColumn<QString>("start");
        QTest::addColumn<QString>("terminal");
        for (const QString& start : {QStringLiteral("startpos"),
                                     QStringLiteral("sfen 4k4/9/9/9/9/9/9/9/4K4 w - 1")}) {
            for (const QString& terminal : {QStringLiteral("resign"), QStringLiteral("win"), QStringLiteral("break")})
                QTest::newRow(qPrintable(start + terminal)) << start << terminal;
        }
    }

    void usiTerminalWithoutMoves()
    {
        QFETCH(QString, start);
        QFETCH(QString, terminal);
        KifuConversionService::Request request;
        request.text = QStringLiteral("position %1 moves %2").arg(start, terminal);
        request.outputFormat = Format::Usi;
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        QCOMPARE(saved.lines, QStringList{request.text});
        const auto reread = KifuLoadParser::parseText(saved.lines.first());
        QVERIFY(reread.success);
        QVERIFY(reread.warning.isEmpty());
        QCOMPARE(reread.record.mainline.entries.size(), 1);
        QCOMPARE(reread.record.mainline.entries.first().terminal, saved.mainline.entries.first().terminal);
    }

    /// KI2 の結果行から、王手千日手・引き分け・最大手数・エラーの種類を失わない。
    void ki2ExtendedTerminalRoundTrip_data()
    {
        QTest::addColumn<QString>("terminal");
        for (const QString& terminal : {QStringLiteral("王手千日手"), QStringLiteral("引き分け"),
                                         QStringLiteral("最大手数到達"), QStringLiteral("エラー")})
            QTest::newRow(qPrintable(terminal)) << terminal;
    }

    void ki2ExtendedTerminalRoundTrip()
    {
        QFETCH(QString, terminal);
        KifuConversionService::Request request;
        request.text = QStringLiteral("手合割：平手\n1 ７六歩(77)\n2 %1\n").arg(terminal);
        request.outputFormat = Format::Ki2;
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        QVERIFY(saved.lines.contains(QStringLiteral("まで1手で%1").arg(terminal)));
        const auto reread = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY(reread.success);
        QCOMPARE(reread.record.mainline.entries.size(), 2);
        QCOMPARE(reread.record.mainline.entries.last().terminal, TerminalTypes::fromWord(terminal));
    }

    /// 読み込んだ反則の駒打ちも JKF の保存で黙って消さず、後続の手と時間を保つ。
    void jkfPreservesLenientDrop()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("手合割：平手\n1 ５五歩打 ( 0:01/00:00:01)\n"
                                      "2 ３四歩(33) ( 0:02/00:00:02)\n3 反則負け\n");
        request.outputFormat = Format::Jkf;
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        const auto reread = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY(reread.success);
        QVERIFY(reread.warning.isEmpty());
        QCOMPARE(reread.record.mainline.usiMoves(), (QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")}));
        QCOMPARE(reread.record.mainline.entries.size(), 3);
        for (qsizetype i = 0; i < saved.mainline.entries.size(); ++i) {
            QCOMPARE(reread.record.mainline.entries.at(i).sfen, saved.mainline.entries.at(i).sfen);
            QCOMPARE(reread.record.mainline.entries.at(i).time.toText(), saved.mainline.entries.at(i).time.toText());
        }
        QCOMPARE(reread.record.mainline.entries.last().terminal, TerminalType::IllegalLoss);
    }

    /// 指せない手があれば、その手の手前までを読み、警告に理由と読んだ手数を書く（どの形式も同じ）
    void unreplayableMoveTruncatesTheLine_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("format");
        QTest::newRow("usi-empty-source") << QStringLiteral("position startpos moves 7g7f 3c3d 7g7e") << int(Format::Usi);
        QTest::newRow("kif-empty-source") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n3 ７五歩(77)\n") << int(Format::Kif);
    }
    void unreplayableMoveTruncatesTheLine()
    {
        QFETCH(QString, text);
        QFETCH(int, format);
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = static_cast<Format>(format);
        request.outputFormat = Format::Sfen;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.usiMoves, (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        const QString warnings = result.warnings.join(QLatin1Char('\n'));
        QVERIFY2(warnings.contains(QStringLiteral("Source square is empty")), qPrintable(warnings));
        QVERIFY2(warnings.contains(QStringLiteral("read up to move 2")), qPrintable(warnings));
        const auto loaded = KifuLoadParser::parseText(text);
        QVERIFY(loaded.success);
        QVERIFY(!loaded.warning.isEmpty());
    }

    /// 変化の最初の手から指せなければ、その変化は手のない変化になる（本譜は残す）
    void unreplayableVariationKeepsMainline()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n変化：3手\n3 ７五歩(77)\n");
        request.inputFormat = Format::Kif;
        request.outputFormat = Format::Sfen;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.usiMoves, (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        QVERIFY(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("Cannot restore move 3")));
    }

    /// パスの手があれば、その手の手前までを読み、パスに対応していないと警告する（B1）
    void passMoveTruncatesTheLine_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("format");
        QTest::addColumn<QStringList>("moves");
        QTest::newRow("kif") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 パス\n   3 ２六歩(27)\n")
                             << int(Format::Kif) << QStringList{QStringLiteral("7g7f")};
        QTest::newRow("ki2") << QStringLiteral("手合割：平手\n▲７六歩 △３四歩 ▲パス △８四歩\n")
                             << int(Format::Ki2) << QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        QTest::newRow("kif-variation") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 ３四歩(33)\n"
                                                         "変化：2手\n   2 ８四歩(83)\n   3 パス\n")
                                       << int(Format::Kif) << QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    }
    void passMoveTruncatesTheLine()
    {
        QFETCH(QString, text);
        QFETCH(int, format);
        QFETCH(QStringList, moves);
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = static_cast<Format>(format);
        request.outputFormat = Format::Kif;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.usiMoves, moves);
        const QString warnings = result.warnings.join(QLatin1Char('\n'));
        QVERIFY2(warnings.contains(QStringLiteral("is a pass")), qPrintable(warnings));
        const auto loaded = KifuLoadParser::parseText(text);
        QVERIFY(loaded.success);
        QVERIFY(loaded.warning.contains(QStringLiteral("is a pass")));
    }

    /// 同じ手順なら、どの形式から読んでも KIF の指し手の表記は同じ。不成は表記しない（B2）
    void kifMoveTextDoesNotDependOnSource_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("format");
        QTest::newRow("ki2") << QStringLiteral("手合割：平手\n▲７六歩 △３四歩 ▲２二角不成 △同銀 ▲５八金右\n") << int(Format::Ki2);
        QTest::newRow("usi") << QStringLiteral("position startpos moves 7g7f 3c3d 8h2b 3a2b 4i5h") << int(Format::Usi);
        QTest::newRow("csa") << QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n+8822KA\n-3122GI\n+4958KI\n") << int(Format::Csa);
        QTest::newRow("kif") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n"
                                               "3 ２二角不成(88)\n4 同銀(31)\n5 ５八金右(49)\n") << int(Format::Kif);
    }
    void kifMoveTextDoesNotDependOnSource()
    {
        QFETCH(QString, text);
        QFETCH(int, format);
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = static_cast<Format>(format);
        request.outputFormat = Format::Kif;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QStringList moveLines;
        for (const QString& line : result.lines)
            if (QRegularExpression(QStringLiteral("^\\s+\\d+ ")).match(line).hasMatch()) moveLines << line.simplified();
        QCOMPARE(moveLines, (QStringList{
            QStringLiteral("1 ７六歩(77) ( 0:00/00:00:00)"), QStringLiteral("2 ３四歩(33) ( 0:00/00:00:00)"),
            QStringLiteral("3 ２二角(88) ( 0:00/00:00:00)"), QStringLiteral("4 同 銀(31) ( 0:00/00:00:00)"),
            QStringLiteral("5 ５八金(49) ( 0:00/00:00:00)")}));
    }

    /// 旧版が出力した不成を含む棋譜を、他ソフトで読める KIF に直しても手・時間・分岐を保つ
    void legacyNonPromotionExport()
    {
        const auto saved = convertFile(QStringLiteral("test_nonpromotion_branch.kifu"), Format::Kif);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        QVERIFY(saved.warnings.isEmpty());
        QCOMPARE(saved.usiMoves.size(), 42);
        QCOMPARE(saved.usiMoves.at(13), QStringLiteral("2b8h"));
        QCOMPARE(saved.usiMoves.at(14), QStringLiteral("7i8h"));
        QVERIFY(saved.hasBranches);
        QCOMPARE(saved.mainline.entries.size(), 43);
        QVERIFY(saved.mainline.terminal());
        QCOMPARE(saved.mainline.terminal()->terminal, TerminalType::Resign);

        const QString text = saved.lines.join(QLatin1Char('\n'));
        // KIF 仕様 ８：不成の場合は「不成」を表記しない。直後の「同」の参照先を壊さない。
        // https://kakinoki.o.oo7.jp/kif_format.html
        QVERIFY2(!text.contains(QStringLiteral("不成")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("14 ８八角(22)")));
        QVERIFY(text.contains(QStringLiteral("15 同　銀(79)")));
        QVERIFY(text.contains(QStringLiteral("21 ６五角成(83)")));
        QVERIFY(text.contains(QStringLiteral("変化：21手")));

        const auto original = KifuLoadParser::parseFile(fixture(QStringLiteral("test_nonpromotion_branch.kifu")));
        const auto reread = KifuLoadParser::parseText(text);
        QVERIFY(original.success);
        QVERIFY2(reread.success, qPrintable(reread.warning));
        QVERIFY(reread.warning.isEmpty());
        QCOMPARE(reread.record.mainline.usiMoves(), saved.usiMoves);
        QCOMPARE(reread.record.mainline.entries.size(), original.record.mainline.entries.size());
        for (qsizetype i = 0; i < original.record.mainline.entries.size(); ++i) {
            const auto& before = original.record.mainline.entries.at(i);
            const auto& after = reread.record.mainline.entries.at(i);
            QCOMPARE(after.sfen, before.sfen);
            QVERIFY(after.time.valid);
            QCOMPARE(after.time.elapsedMs, before.time.elapsedMs);
            QCOMPARE(after.time.totalMs, before.time.totalMs);
            QCOMPARE(after.terminal, before.terminal);
        }
        QCOMPARE(original.record.variations.size(), 1);
        QCOMPARE(reread.record.variations.size(), 1);
        const auto& branch = reread.record.variations.first();
        QCOMPARE(branch.startPly, 21);
        QCOMPARE(branch.parentVariation, -1);
        QCOMPARE(branch.line.usiMoves(), (QStringList{QStringLiteral("4i5h"), QStringLiteral("7c7d")}));
        QCOMPARE(branch.line.entries.size(), 2);
        for (qsizetype i = 0; i < branch.line.entries.size(); ++i) {
            QCOMPARE(branch.line.entries.at(i).sfen, original.record.variations.first().line.entries.at(i).sfen);
            QCOMPARE(branch.line.entries.at(i).time.toText(), original.record.variations.first().line.entries.at(i).time.toText());
        }
    }

    /// CSA で局面に合わない手・手番と違う符号の手は、その手の手前までを読む（B3）。後の手で手番がずれない
    void csaUnreadableMoveTruncatesTheRecord_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("reason");
        QTest::newRow("no-piece") << QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n+1112HI\n-8384FU\n+2726FU\n%TORYO\n")
                                  << QStringLiteral("does not match the position");
        QTest::newRow("wrong-side") << QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n-8384FU\n+2726FU\n%TORYO\n")
                                    << QStringLiteral("Not the side to move");
    }
    void csaUnreadableMoveTruncatesTheRecord()
    {
        QFETCH(QString, text);
        QFETCH(QString, reason);
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = Format::Csa;
        request.outputFormat = Format::Csa;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.usiMoves, (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        const QString warnings = result.warnings.join(QLatin1Char('\n'));
        QVERIFY2(warnings.contains(reason), qPrintable(warnings));
        QVERIFY2(warnings.contains(QStringLiteral("read up to move 2")), qPrintable(warnings));
        // 打ち切った後の手も終局も書かない
        QVERIFY(!result.lines.contains(QStringLiteral("+8384FU")));
        QVERIFY(!result.lines.contains(QStringLiteral("-8384FU")));
        QVERIFY(!result.lines.contains(QStringLiteral("%TORYO")));
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
        QVERIFY2(encoded.ok, qPrintable(encoded.failure.message));
        request.text = encoded.lines.join(QLatin1Char('\n'));
        request.inputFormat = request.outputFormat;
        request.outputFormat = Format::Jkf;
        const auto decoded = KifuConversionService::convert(request);
        QVERIFY2(decoded.ok, qPrintable(decoded.failure.message));
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
        QVERIFY2(result.ok, qPrintable(result.failure.message));
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
                                               + QStringLiteral(": ") + result.failure.message));
                QVERIFY2(result.usiMoves == basicMoves(), qPrintable(file + QStringLiteral(": ") + result.usiMoves.join(QLatin1Char(' '))));
                QVERIFY(!result.lines.isEmpty());
            }
        }
    }

    void kifHeaderCarriesPlayerNames()
    {
        const auto csa = convertFile(QStringLiteral("test_basic.kif"), Format::Csa);
        QVERIFY2(csa.ok, qPrintable(csa.failure.message));
        QVERIFY(csa.lines.contains(QStringLiteral("N+テスト先手")));
        QVERIFY(csa.lines.contains(QStringLiteral("N-テスト後手")));

        const auto kif = convertFile(QStringLiteral("test_basic.csa"), Format::Kif);
        QVERIFY2(kif.ok, qPrintable(kif.failure.message));
        QVERIFY(kif.lines.filter(QStringLiteral("先手：")).size() == 1);
    }

    /// 対局情報のない棋譜を変換しても、変換した時刻を対局の日時として書かず、仮の対局者名も作らない
    void conversionDoesNotInventGameInfo()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d");
        request.outputFormat = Format::Kif;
        const auto kif = KifuConversionService::convert(request);
        QVERIFY2(kif.ok, qPrintable(kif.failure.message));
        QVERIFY2(kif.lines.filter(QStringLiteral("対局日")).isEmpty(), qPrintable(kif.lines.join(QLatin1Char('\n'))));
        QVERIFY(kif.lines.filter(QStringLiteral("開始日時")).isEmpty());
        QVERIFY(kif.lines.filter(QStringLiteral("先手：")).isEmpty());
        QVERIFY(kif.lines.filter(QStringLiteral("後手：")).isEmpty());

        request.outputFormat = Format::Csa;
        const auto csa = KifuConversionService::convert(request);
        QVERIFY2(csa.ok, qPrintable(csa.failure.message));
        QVERIFY2(csa.lines.filter(QStringLiteral("$START_TIME")).isEmpty(), qPrintable(csa.lines.join(QLatin1Char('\n'))));
    }

    void textInputIsAutoDetected()
    {
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d");
        request.outputFormat = Format::Sfen;
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.inputFormat, Format::Usi);
        QCOMPARE(result.sfens.size(), 3);
        QCOMPARE(result.lines, result.sfens);

        KifuConversionService::Request csa;
        csa.text = QStringLiteral("V2.2\nN+A\nN-B\nPI\n+\n+7776FU\n-3334FU\n%TORYO\n");
        csa.outputFormat = Format::Kif;
        const auto csaResult = KifuConversionService::convert(csa);
        QVERIFY2(csaResult.ok, qPrintable(csaResult.failure.message));
        QCOMPARE(csaResult.inputFormat, Format::Csa);
        QCOMPARE(csaResult.usiMoves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
    }

    void branchesAreReportedAndMainlineKept()
    {
        const auto result = convertFile(QStringLiteral("test_branch.kif"), Format::Kif);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QVERIFY(result.hasBranches);
        QVERIFY(!result.usiMoves.isEmpty());
    }

    /// 失敗は分類つきで返す（ファイルがない・入力の指定の誤り・読めない中身・不正な開始局面）
    void reportsErrors()
    {
        KifuConversionService::Request request;
        request.inputPath = fixture(QStringLiteral("does_not_exist.kif"));
        request.outputFormat = Format::Kif;
        const auto missing = KifuConversionService::convert(request);
        QVERIFY(!missing.ok);
        QCOMPARE(missing.failure.kind, ErrorKind::NotFound);

        KifuConversionService::Request both;
        both.inputPath = fixture(QStringLiteral("test_basic.kif"));
        both.text = QStringLiteral("x");
        both.outputFormat = Format::Kif;
        const auto ambiguous = KifuConversionService::convert(both);
        QVERIFY(!ambiguous.ok);
        QCOMPARE(ambiguous.failure.kind, ErrorKind::InvalidArgument);

        // 歩が20枚ある局面は validate-sfen・render-board と同じく読めない
        KifuConversionService::Request tooMany;
        tooMany.text = QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b 2P 1");
        tooMany.outputFormat = Format::Kif;
        const auto invalidStart = KifuConversionService::convert(tooMany);
        QVERIFY(!invalidStart.ok);
        QCOMPARE(invalidStart.failure.kind, ErrorKind::InvalidContent);

        QVERIFY(!KifuFormats::fromName(QStringLiteral("pgn")).has_value());
        QCOMPARE(KifuFormats::fromPath(QStringLiteral("/tmp/a.KI2U")), Format::Ki2);
    }
};

QTEST_GUILESS_MAIN(TestKifuConversionService)
#include "tst_kifu_conversion_service.moc"
