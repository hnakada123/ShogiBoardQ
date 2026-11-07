/// @file tst_kifu_conversion_service.cpp
/// @brief KifuConversionService のユニットテスト（fixtures の各形式を相互変換する）

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifuconversionservice.h"
#include "kifubranchnode.h"
#include "kifuexport.h"
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

    /// 開始局面のコメント、初手の消費時間・しおり・コメント、2手目の消費時間がある KIF
    static QString annotatedKif()
    {
        return QStringLiteral("手合割：平手\n*開始局面のコメント\n手数----指手---------消費時間--\n"
                              "   1 ７六歩(77)   ( 0:10/0:00:10)\n&初手のしおり\n*初手のコメント\n"
                              "   2 ３四歩(33)   ( 0:05/0:00:05)\n");
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
        QTest::addColumn<QString>("warning");
        // 持ち駒にない歩を打つ反則の手も記録どおり読み、警告する
        QTest::newRow("ki2-missing-hand") << QStringLiteral("手合割：平手\n▲５五歩 △３四歩\n")
            << QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")}
            << QStringLiteral("KI2: Move 1 is an illegal drop: The dropped piece is not in hand: P*5e");
        QTest::newRow("ki2-blocked-bishop") << QStringLiteral("手合割：平手\n▲５五角 △３四歩\n")
            << QStringList{QStringLiteral("8h5e"), QStringLiteral("3c3d")} << QString();
        QTest::newRow("csa-missing-hand") << QStringLiteral("V3.0\nPI\n+\n+0055FU\nT1.234\n-3334FU\nT0.125\n")
            << QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")}
            << QStringLiteral("CSA: Move 1 is an illegal drop: The dropped piece is not in hand: P*5e");
    }

    void omittedDropOrBlockedOriginIsRecovered()
    {
        QFETCH(QString, source);
        QFETCH(QStringList, moves);
        QFETCH(QString, warning);
        const auto loaded = KifuLoadParser::parseText(source);
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.warning.trimmed(), warning);
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

    /// 保存する文字コードを渡すと、KIF・KI2・CSA の先頭行の宣言もその文字コードにする（書いた後に置き換えない）
    void outputEncodingIsDeclaredInTheHeader_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<QString>("utf8");
        QTest::addColumn<QString>("shiftJis");
        QTest::newRow("kif") << int(Format::Kif) << QStringLiteral("#KIF version=2.0 encoding=UTF-8")
                             << QStringLiteral("#KIF version=2.0 encoding=Shift_JIS");
        QTest::newRow("ki2") << int(Format::Ki2) << QStringLiteral("#KIF version=2.0 encoding=UTF-8")
                             << QStringLiteral("#KIF version=2.0 encoding=Shift_JIS");
        QTest::newRow("csa") << int(Format::Csa) << QStringLiteral("'CSA encoding=UTF-8")
                             << QStringLiteral("'CSA encoding=Shift_JIS");
    }

    void outputEncodingIsDeclaredInTheHeader()
    {
        QFETCH(int, format);
        QFETCH(QString, utf8);
        QFETCH(QString, shiftJis);
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d");
        request.outputFormat = static_cast<Format>(format);
        const auto plain = KifuConversionService::convert(request);
        QVERIFY2(plain.ok, qPrintable(plain.failure.message));
        QCOMPARE(plain.lines.first(), utf8);
        request.outputEncoding = KifuEncoding::ShiftJis;
        const auto sjis = KifuConversionService::convert(request);
        QVERIFY2(sjis.ok, qPrintable(sjis.failure.message));
        QCOMPARE(sjis.lines.first(), shiftJis);
        QCOMPARE(sjis.lines.mid(1), plain.lines.mid(1));
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
        // 反則の手は残すが、読むたびに警告する
        QVERIFY2(reread.warning.contains(QStringLiteral("JKF: Move 1 is an illegal drop")), qPrintable(reread.warning));
        QCOMPARE(reread.record.mainline.usiMoves(), (QStringList{QStringLiteral("P*5e"), QStringLiteral("3c3d")}));
        QCOMPARE(reread.record.mainline.entries.size(), 3);
        for (qsizetype i = 0; i < saved.mainline.entries.size(); ++i) {
            QCOMPARE(reread.record.mainline.entries.at(i).sfen, saved.mainline.entries.at(i).sfen);
            QCOMPARE(reread.record.mainline.entries.at(i).time.toText(), saved.mainline.entries.at(i).time.toText());
        }
        QCOMPARE(reread.record.mainline.entries.last().terminal, TerminalType::IllegalLoss);
    }

    /// 持ち駒にない駒を打つ反則の手は、どの形式でも記録どおり読み（手順を打ち切らない）、警告する（不正入力 I03）
    void illegalDropIsKeptWithAWarning_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<QString>("tag");
        QTest::newRow("kif") << int(Format::Kif) << QStringLiteral("KIF");
        QTest::newRow("ki2") << int(Format::Ki2) << QStringLiteral("KI2");
        QTest::newRow("csa") << int(Format::Csa) << QStringLiteral("CSA");
        QTest::newRow("jkf") << int(Format::Jkf) << QStringLiteral("JKF");
        QTest::newRow("usi") << int(Format::Usi) << QStringLiteral("USI");
        QTest::newRow("usen") << int(Format::Usen) << QStringLiteral("USEN");
    }
    void illegalDropIsKeptWithAWarning()
    {
        QFETCH(int, format);
        QFETCH(QString, tag);
        const QStringList moves{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("B*5e"), QStringLiteral("8c8d")};
        KifuConversionService::Request request;
        request.text = QStringLiteral("position startpos moves 7g7f 3c3d B*5e 8c8d");
        request.outputFormat = static_cast<Format>(format);
        const auto saved = KifuConversionService::convert(request);
        QVERIFY2(saved.ok, qPrintable(saved.failure.message));
        const QString warnings = saved.warnings.join(QLatin1Char('\n'));
        QVERIFY2(warnings.contains(QStringLiteral("USI: Move 3 is an illegal drop: The dropped piece is not in hand: B*5e")),
                 qPrintable(warnings));
        const auto reread = KifuLoadParser::parseText(saved.lines.join(QLatin1Char('\n')));
        QVERIFY(reread.success);
        QCOMPARE(reread.record.mainline.usiMoves(), moves);
        QVERIFY2(reread.warning.contains(QStringLiteral("%1: Move 3 is an illegal drop").arg(tag)), qPrintable(reread.warning));
        QVERIFY(!reread.warning.contains(QStringLiteral("Move 4")));
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
                                    << QStringLiteral("The move is written for the side not to move");
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
        // QJsonArray{配列} は、要素1つの同じ型のリストとしてコピーになるコンパイラがある（CWG 1467）。append で入れ子にする
        QJsonArray lastForks;
        lastForks.append(QJsonArray{move(5, 7, 5, 6, 0)});
        branchLast["forks"] = lastForks;
        const QJsonArray branch{move(2, 7, 2, 6, 0), move(8, 3, 8, 4, 1),
                                move(7, 7, 7, 6, 0), move(3, 3, 3, 4, 1), branchLast};
        auto first = move(7, 7, 7, 6, 0);
        QJsonArray firstForks;
        firstForks.append(branch);
        first["forks"] = firstForks;
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

    void variationsAreDroppedOnlyByFormatsThatCannotHoldThem_data()
    {
        QTest::addColumn<Format>("output");
        QTest::addColumn<bool>("writesVariations");
        QTest::newRow("kif") << Format::Kif << true;
        QTest::newRow("ki2") << Format::Ki2 << true;
        QTest::newRow("csa") << Format::Csa << false;
        QTest::newRow("jkf") << Format::Jkf << true;
        QTest::newRow("usi") << Format::Usi << false;
        QTest::newRow("usen") << Format::Usen << true;
        QTest::newRow("sfen") << Format::Sfen << false;
    }

    /// 変化を書けるかの表（KifuExport::writesVariations）は、書き出しが実際に変化を書くかと合う。
    /// MCP の convert_kifu の要約はこの結果（variations_dropped）で「本譜だけ」と出す
    void variationsAreDroppedOnlyByFormatsThatCannotHoldThem()
    {
        QFETCH(Format, output);
        QFETCH(bool, writesVariations);
        QCOMPARE(KifuExport::writesVariations(output), writesVariations);

        const auto result = convertFile(QStringLiteral("test_branch.kif"), output);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QVERIFY(result.hasBranches);
        QCOMPARE(result.variationsDropped, !writesVariations);
        if (output == Format::Sfen) {
            QCOMPARE(result.lines, result.sfens);  // 本譜の局面列
            return;
        }

        KifuConversionService::Request reread;
        reread.text = result.lines.join(QLatin1Char('\n'));
        reread.inputFormat = output;
        reread.outputFormat = Format::Kif;
        const auto again = KifuConversionService::convert(reread);
        QVERIFY2(again.ok, qPrintable(again.failure.message));
        QCOMPARE(again.hasBranches, writesVariations);
        QCOMPARE(again.usiMoves, result.usiMoves);
        QVERIFY(!again.variationsDropped);

        const auto plain = convertFile(QStringLiteral("test_basic.kif"), output);
        QVERIFY2(plain.ok, qPrintable(plain.failure.message));
        QVERIFY(!plain.hasBranches);
        QVERIFY(!plain.variationsDropped);
    }

    /// USI 形式のファイルは、局面として読める最初の行を読む（行の判定も position コマンドと同じ UsiPosition::split
    /// と Position::fromSfen。監査 M4）。コメントや、"sfen" で始まっても局面として読めない行は飛ばす
    void usiReadsTheFirstReadablePositionLine()
    {
        KifuConversionService::Request request;
        request.inputFormat = Format::Usi;
        request.outputFormat = Format::Usi;
        request.text = QStringLiteral("# ShogiGUI\nsfen broken b - 1\n\nposition startpos moves 7g7f 3c3d\n");
        const auto result = KifuConversionService::convert(request);
        QVERIFY2(result.ok, qPrintable(result.failure.message));
        QCOMPARE(result.usiMoves, (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));

        // 手数を省いた SFEN と "moves" を省いた手も、position コマンドと同じく読む
        request.text = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 5i4h\n");
        const auto omitted = KifuConversionService::convert(request);
        QVERIFY2(omitted.ok, qPrintable(omitted.failure.message));
        QCOMPARE(omitted.usiMoves, QStringList{QStringLiteral("5i4h")});
    }

    /// 書けるものの表（KifuExport::capabilities）は、書き出しが実際に消費時間・コメント・しおりを書くかと合う
    void capabilitiesMatchWhatTheExportersWrite_data()
    {
        QTest::addColumn<Format>("output");
        for (const Format format : {Format::Kif, Format::Ki2, Format::Csa, Format::Jkf, Format::Usen, Format::Usi})
            QTest::newRow(qPrintable(KifuFormats::name(format))) << format;
    }

    void capabilitiesMatchWhatTheExportersWrite()
    {
        QFETCH(Format, output);
        KifuConversionService::Request request;
        request.text = annotatedKif();
        request.outputFormat = output;
        const auto written = KifuConversionService::convert(request);
        QVERIFY2(written.ok, qPrintable(written.failure.message));

        KifuConversionService::Request reread;
        reread.text = written.lines.join(QLatin1Char('\n'));
        reread.inputFormat = output;
        reread.outputFormat = Format::Kif;
        const auto again = KifuConversionService::convert(reread);
        QVERIFY2(again.ok, qPrintable(again.failure.message));
        QVERIFY(!again.mainline.entries.isEmpty());
        const RecordEntry& first = again.mainline.entries.first();

        const KifuExport::Capabilities can = KifuExport::capabilities(output);
        QCOMPARE(first.time.isRecorded(), can.times);
        QCOMPARE(first.comment.contains(QStringLiteral("初手のコメント")), can.comments);
        QCOMPARE(again.mainline.openingComment.contains(QStringLiteral("開始局面のコメント")), can.comments);
        QCOMPARE(first.bookmark.contains(QStringLiteral("初手のしおり")), can.bookmarks);
    }

    /// 形式で書くと失うもの（保存の確認・コピーの知らせ）。変化を書けない形式は本譜だけを見る（監査 2026-11-01 F20）
    void lossFollowsTheCapabilities_data()
    {
        QTest::addColumn<QString>("record");
        QTest::addColumn<Format>("output");
        QTest::addColumn<QString>("expected");   // 失うもの: v=変化 t=消費時間 c=コメント b=しおり x=終局
        // 本譜に消費時間・コメント・しおり・詰み、変化にコメント
        const QString annotated = QStringLiteral("annotated");
        QTest::newRow("kif") << annotated << Format::Kif << QString();
        QTest::newRow("ki2") << annotated << Format::Ki2 << QStringLiteral("t");
        QTest::newRow("csa") << annotated << Format::Csa << QStringLiteral("vb");
        QTest::newRow("jkf") << annotated << Format::Jkf << QStringLiteral("b");
        QTest::newRow("usen") << annotated << Format::Usen << QStringLiteral("tcbx");
        QTest::newRow("usi") << annotated << Format::Usi << QStringLiteral("vtcb");
        // 本譜には何もなく、変化にだけコメントとしおり
        const QString variationOnly = QStringLiteral("variation-only");
        QTest::newRow("variation-only-kif") << variationOnly << Format::Kif << QString();
        QTest::newRow("variation-only-csa") << variationOnly << Format::Csa << QStringLiteral("v");
        QTest::newRow("variation-only-jkf") << variationOnly << Format::Jkf << QStringLiteral("b");
        QTest::newRow("variation-only-usen") << variationOnly << Format::Usen << QStringLiteral("cb");
        QTest::newRow("variation-only-usi") << variationOnly << Format::Usi << QStringLiteral("v");
    }

    void lossFollowsTheCapabilities()
    {
        QFETCH(QString, record);
        QFETCH(Format, output);
        QFETCH(QString, expected);
        const QString text = record == QLatin1String("annotated")
            ? annotatedKif() + QStringLiteral("   3 詰み\n\n変化：2手\n   2 ８四歩(83)\n*変化のコメント\n")
            : QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 ３四歩(33)\n\n"
                             "変化：2手\n   2 ８四歩(83)\n&変化のしおり\n*変化のコメント\n");
        const KifuLoadResult loaded = KifuLoadParser::parseText(text);
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        KifuBranchTree tree;
        QVERIFY(KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record));
        QCOMPARE(tree.lineCount(), 2);

        const KifuExport::Loss loss = KifuExport::lossIn(tree, output);
        QCOMPARE(loss.variations, expected.contains(QLatin1Char('v')));
        QCOMPARE(loss.times, expected.contains(QLatin1Char('t')));
        QCOMPARE(loss.comments, expected.contains(QLatin1Char('c')));
        QCOMPARE(loss.bookmarks, expected.contains(QLatin1Char('b')));
        QCOMPARE(loss.terminals, expected.contains(QLatin1Char('x')) ? QList<TerminalType>{TerminalType::Checkmate}
                                                                      : QList<TerminalType>{});
        QCOMPARE(loss.isEmpty(), expected.isEmpty());
    }

    /// 終局の行を書けるか（CSA・USI・USEN は符号の表。KIF・KI2・JKF は終局語で書ける）
    void terminalsFollowTheCodeTables()
    {
        QVERIFY(KifuExport::writesTerminal(Format::Kif, TerminalType::Custom));
        QVERIFY(KifuExport::writesTerminal(Format::Jkf, TerminalType::Try));
        QVERIFY(KifuExport::writesTerminal(Format::Csa, TerminalType::IllegalWin));
        QVERIFY(!KifuExport::writesTerminal(Format::Csa, TerminalType::Custom));
        QVERIFY(KifuExport::writesTerminal(Format::Usi, TerminalType::Checkmate));
        QVERIFY(!KifuExport::writesTerminal(Format::Usi, TerminalType::MaxMoves));
        QVERIFY(KifuExport::writesTerminal(Format::Usen, TerminalType::Resign));
        QVERIFY(!KifuExport::writesTerminal(Format::Usen, TerminalType::IllegalLoss));
        QVERIFY(!KifuExport::writesTerminal(Format::Usen, TerminalType::IllegalWin));
    }

    /// 失敗は分類つきで返す（ファイルがない・相対パスとディレクトリ・入力の指定の誤り・読めない中身・不正な開始局面）
    void reportsErrors()
    {
        KifuConversionService::Request request;
        request.inputPath = fixture(QStringLiteral("does_not_exist.kif"));
        request.outputFormat = Format::Kif;
        const auto missing = KifuConversionService::convert(request);
        QVERIFY(!missing.ok);
        QCOMPARE(missing.failure.kind, ErrorKind::NotFound);

        // 入力のファイルは自動化 API の kifu.load と同じく確かめる（相対パスとディレクトリは引数の誤りで、読まない）
        KifuConversionService::Request relative;
        relative.inputPath = QStringLiteral("fixtures/test_basic.kif");
        relative.outputFormat = Format::Kif;
        QCOMPARE(KifuConversionService::convert(relative).failure.kind, ErrorKind::InvalidArgument);
        KifuConversionService::Request folder;
        folder.inputPath = fixture(QString());
        folder.outputFormat = Format::Kif;
        QCOMPARE(KifuConversionService::convert(folder).failure.kind, ErrorKind::InvalidArgument);

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

    /// 長い棋譜でも失うものの確かめで手数の深さまで再帰しない（監査 2026-11-07 F20。セキュリティ監査 S5）
    void lossInDoesNotRecurseOnLongRecords()
    {
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuBranchNode* current = tree.root();
        for (int i = 0; i < 200000; ++i)
            current = tree.addRecord(current, RecordEntry::played({}, QStringLiteral("m"), QStringLiteral("s")));
        QVERIFY(KifuExport::lossIn(tree, KifuFormat::Kif).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestKifuConversionService)
#include "tst_kifu_conversion_service.moc"
