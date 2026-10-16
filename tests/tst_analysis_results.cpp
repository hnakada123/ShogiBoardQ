/// @file tst_analysis_results.cpp
/// @brief 棋譜解析の1局面の結果のテスト
///
/// PlyAnalysisCollector（info と bestmove から1局面の結果を作る）と
/// AnalysisResultHandler（結果を表の1行にする）をテストする。

#include <QTest>
#include <QStringList>

#include "analysisjob.h"
#include "analysisresulthandler.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "usilineparser.h"

namespace {
UsiInfo info(const char* line)
{
    return *UsiLineParser::parseInfo(QString::fromUtf8(line));
}

UsiPosition position(const char* command)
{
    return UsiPosition::parse(QString::fromUtf8(command)).value_or(UsiPosition{});
}

UsiBestMove bestMove(const UsiScore& score)
{
    UsiBestMove result;
    result.move = QStringLiteral("7g7f");
    result.score = score;
    return result;
}

/// 棋譜解析の表の「mate 5」「mate +」「mate +0」などの表示から、先手から見た評価値を作る
UsiScore mateScore(const QString& text)
{
    const QString mate = text.mid(QStringLiteral("mate ").size());
    UsiScore score;
    score.kind = UsiScore::Kind::Mate;
    if (mate == QLatin1String("+") || mate == QLatin1String("-")) {
        score.mateWin = mate == QLatin1String("+");
        return score;
    }
    score = UsiScore::mate(mate.toInt());
    if (mate == QLatin1String("+0")) score.mateWin = true;
    return score;
}

/// 手番側から見た評価値の結果
PlyAnalysis result(int ply, const UsiScore& score, const QStringList& pv = {})
{
    PlyAnalysis analysis;
    analysis.ply = ply;
    analysis.score = score;
    analysis.usiPv = pv;
    analysis.pvKanji = pv.join(QLatin1Char(' '));
    return analysis;
}
} // namespace

class TestAnalysisResults : public QObject
{
    Q_OBJECT

private slots:
    // --- PlyAnalysisCollector ---

    void collectorFormatsBestLineOnPosition()
    {
        PlyAnalysisCollector collector;
        collector.begin(1, position("position startpos moves 7g7f"));
        collector.addInfo(info("info depth 10 score cp 50 pv 3c3d 2g2f"));
        const PlyAnalysis analysis = collector.finish(bestMove(UsiScore::cp(50)));
        QCOMPARE(analysis.ply, 1);
        QVERIFY(!analysis.book);
        QCOMPARE(analysis.score.value, 50);
        QCOMPARE(analysis.usiPv, (QStringList{QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QCOMPARE(analysis.pvKanji, QStringLiteral("△３四歩(33)▲２六歩(27)"));
    }

    /// エンジン設定の MultiPV が2以上でも、1局面の結果は最善の読み筋（multipv 1）で確定する（U1）
    void collectorUsesBestLineWithMultiPv()
    {
        PlyAnalysisCollector collector;
        collector.begin(0, position("position startpos"));
        // 深さごとに multipv 1, 2 の順で届き、bestmove の直前の行は2番目の読み筋
        collector.addInfo(info("info depth 10 multipv 1 score cp 200 pv 7g7f 3c3d"));
        collector.addInfo(info("info depth 10 multipv 2 score cp -300 pv 2g2f 8c8d"));
        const PlyAnalysis analysis = collector.finish(bestMove(UsiScore::cp(200)));
        QCOMPARE(analysis.score.value, 200);
        QCOMPARE(analysis.usiPv.join(QLatin1Char(' ')), QStringLiteral("7g7f 3c3d"));
        QCOMPARE(analysis.pvKanji, QStringLiteral("▲７六歩(77)△３四歩(33)"));
    }

    void collectorDropsBookPercentAndResign()
    {
        PlyAnalysisCollector collector;
        collector.begin(0, position("position startpos"));
        collector.addInfo(info("info score cp 120 pv 7g7f 3c3d (100.00%)"));
        QCOMPARE(collector.finish(bestMove(UsiScore::cp(120))).usiPv.join(QLatin1Char(' ')),
                 QStringLiteral("7g7f 3c3d"));

        // 詰んだ局面ではエンジンが「pv resign」を返す。resign を候補手や読み筋の盤面表示に使わない
        collector.begin(2, position("position startpos moves 7g7f 3c3d"));
        collector.addInfo(info("info score mate -1 pv resign"));
        const PlyAnalysis resigned = collector.finish(bestMove(UsiScore::mate(-1)));
        QVERIFY(resigned.usiPv.isEmpty());
        QCOMPARE(resigned.pvKanji, QStringLiteral("▲投了"));
    }

    /// info が1つも届かずに bestmove が届いたら定跡
    void collectorMarksBookMove()
    {
        PlyAnalysisCollector collector;
        collector.begin(1, position("position startpos moves 7g7f"));
        const PlyAnalysis book = collector.finish(bestMove(UsiScore{}));
        QCOMPARE(book.ply, 1);
        QVERIFY(book.book);
        QVERIFY(book.usiPv.isEmpty());
        // 次の局面は前の局面の読み筋を引き継がない
        collector.addInfo(info("info score cp 10 pv 3c3d"));
        collector.begin(2, position("position startpos moves 7g7f 3c3d"));
        QVERIFY(collector.finish(bestMove(UsiScore{})).book);
    }

    // --- AnalysisResultHandler ---

    void appendAddsRowAndReturnsBlackScore()
    {
        KifuAnalysisListModel model;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        AnalysisResultHandler handler(refs);
        // ply=2 は偶数（先手番）なので評価値 200 がそのまま
        QCOMPARE(handler.append(result(2, UsiScore::cp(200), {QStringLiteral("7g7f")})).value, 200);
        // ply=3 は後手番なので先手から見た値に符号を直す
        QCOMPARE(handler.append(result(3, UsiScore::cp(80), {QStringLiteral("3c3d")})).value, -80);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.item(0)->usiPv(), QStringLiteral("7g7f"));
    }

    void bookResultHasNoScoreOrPv()
    {
        KifuAnalysisListModel model;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        AnalysisResultHandler handler(refs);
        PlyAnalysis book;
        book.ply = 1;
        book.book = true;
        QVERIFY(handler.append(book).isNone());
        QCOMPARE(model.item(0)->principalVariation(), QStringLiteral("（定跡）"));
        QCOMPARE(model.item(0)->usiPv(), QString());
    }

    void mateScoresKeepTheirSide()
    {
        KifuAnalysisListModel model;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        AnalysisResultHandler handler(refs);
        // 後手番の局面なので先手から見ると詰まされる側
        const UsiScore mated = handler.append(result(1, UsiScore::mate(5), {QStringLiteral("7g7f")}));
        QVERIFY(mated.isMate());
        QCOMPARE(mated.mateText(), QStringLiteral("-5"));
        QVERIFY(handler.append(result(2, UsiScore::cp(120))).isCp());
        QCOMPARE(handler.append(result(3, info("info score mate -").score)).mateText(), QStringLiteral("+"));
    }

    void missingEvaluationAndDifferences()
    {
        KifuAnalysisListModel model;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        AnalysisResultHandler handler(refs);
        handler.append(result(0, UsiScore::cp(120), {QStringLiteral("7g7f")}));
        QVERIFY(!model.item(0)->evaluationDifference());
        QCOMPARE(model.index(0, 5).data().toString(), QStringLiteral("-"));
        handler.append(result(1, UsiScore::cp(-160), {QStringLiteral("3c3d")}));
        QCOMPARE(model.item(1)->evaluationDifference(), std::optional<int>(40));
        QCOMPARE(model.index(1, 5).data().toString(), QStringLiteral("40"));
        QVERIFY(handler.append(result(2, UsiScore{}, {QStringLiteral("2g2f")})).isNone());
        QVERIFY(model.item(2)->evaluation().isNone());
        QCOMPARE(model.index(2, 3).data().toString(), QStringLiteral("-"));
        QVERIFY(!model.item(2)->evaluationDifference());
        QVERIFY(model.index(2, 4).data().toString().isEmpty());
        handler.append(result(3, UsiScore::cp(-200), {QStringLiteral("8c8d")}));
        QVERIFY(!model.item(3)->evaluationDifference());
        handler.append(result(4, UsiScore::mate(-3), {QStringLiteral("7g7f")}));
        QVERIFY(!model.item(4)->evaluationDifference());
    }

    void mateJudgement_data()
    {
        QTest::addColumn<QString>("score");
        QTest::addColumn<QString>("judgement");
        QTest::newRow("positive") << QStringLiteral("mate 5") << QStringLiteral("先手勝ち");
        QTest::newRow("negative") << QStringLiteral("mate -5") << QStringLiteral("後手勝ち");
        QTest::newRow("positive-sign") << QStringLiteral("mate +") << QStringLiteral("先手勝ち");
        QTest::newRow("negative-sign") << QStringLiteral("mate -") << QStringLiteral("後手勝ち");
        QTest::newRow("mated") << QStringLiteral("mate 0") << QStringLiteral("後手勝ち");
        QTest::newRow("opponent-mated") << QStringLiteral("mate +0") << QStringLiteral("先手勝ち");
    }

    void mateJudgement()
    {
        QFETCH(QString, score);
        QFETCH(QString, judgement);
        KifuAnalysisListModel model;
        model.appendItem(new KifuAnalysisResultsDisplay(QString(), mateScore(score), std::nullopt, QString()));
        QCOMPARE(model.index(0, 3).data().toString(), score);
        QCOMPARE(model.index(0, 4).data().toString(), judgement);
    }
};

QTEST_MAIN(TestAnalysisResults)
#include "tst_analysis_results.moc"
