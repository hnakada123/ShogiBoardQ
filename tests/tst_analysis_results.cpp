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
#include "kifuanalysisrow.h"
#include "kifupresentation.h"
#include "sfenutils.h"
#include "test_replay.h"
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
        QCOMPARE(analysis.depth, 10);
    }

    /// エンジン設定の MultiPV が2以上でも、1局面の結果は最善の読み筋（multipv 1）で確定する（U1）
    void collectorUsesBestLineWithMultiPv()
    {
        PlyAnalysisCollector collector;
        collector.begin(0, position("position startpos"));
        // 深さごとに multipv 1, 2 の順で届き、bestmove の直前の行は2番目の読み筋
        collector.addInfo(info("info depth 10 multipv 1 score cp 200 pv 7g7f 3c3d"));
        collector.addInfo(info("info depth 11 multipv 2 score cp -300 pv 2g2f 8c8d"));
        const PlyAnalysis analysis = collector.finish(bestMove(UsiScore::cp(200)));
        QCOMPARE(analysis.score.value, 200);
        // 深さも最善の読み筋のもの（定跡に登録するときに使う）
        QCOMPARE(analysis.depth, 10);
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
        PlyAnalysis first = result(2, UsiScore::cp(200), {QStringLiteral("7g7f")});
        first.depth = 18;
        QCOMPARE(handler.append(first).value, 200);
        // ply=3 は後手番なので先手から見た値に符号を直す
        QCOMPARE(handler.append(result(3, UsiScore::cp(80), {QStringLiteral("3c3d")})).value, -80);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.row(0)->usiPv, QStringLiteral("7g7f"));
        // 行は局面の手数と読み筋の深さも持つ（定跡に登録するときに使う）
        QCOMPARE(model.row(0)->ply, 2);
        QCOMPARE(model.row(0)->depth, 18);
        QCOMPARE(model.row(1)->ply, 3);
    }

    /// 手の列は棋譜の表記（RecordEntry::displayText）から表示のたびに作り、手数と分岐の印を付ける。
    /// 英語表記で解析した後に日本語表記へ切り替えても日本語で出す
    void moveColumnFollowsNotation()
    {
        const QString hirate = SfenUtils::hirateSfen();
        const QStringList moves{QStringLiteral("7g7f")};
        RecordLine line = RecordLine::fromPositions(TestReplay::buildSfenRecord(hirate, moves, false),
                                                    TestReplay::buildGameMoves(hirate, moves));
        line.entries[0].displayText = QStringLiteral("▲７六歩(77)");
        KifuPresentation::configure(QStringLiteral("en"), QStringLiteral("western"), false);
        KifuAnalysisListModel model;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        refs.line = &line;
        refs.branchPlies = {1};
        AnalysisResultHandler handler(refs);
        handler.append(result(0, UsiScore::cp(30), {QStringLiteral("7g7f")}));
        handler.append(result(1, UsiScore::cp(10), {QStringLiteral("3c3d")}));
        const auto cell = [&model](int row) { return model.index(row, KifuAnalysisListModel::MoveColumn).data().toString(); };
        QCOMPARE(model.row(1)->move, QStringLiteral("▲７六歩(77)"));
        QCOMPARE(cell(1), QStringLiteral("   1 ▲P-7f [+]"));
        KifuPresentation::configure(QStringLiteral("ja_JP"), QStringLiteral("japanese"), false);
        QCOMPARE(cell(1), QStringLiteral("   1 ▲７六歩(77)+"));
        QCOMPARE(cell(0), QStringLiteral("=== 開始局面 ==="));
        KifuPresentation::configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false);
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
        QVERIFY(model.row(0)->book);
        QCOMPARE(model.row(0)->pv, QString());
        // 定跡の印は表示するときに表示言語で作る（行には書かない）
        QCOMPARE(model.index(0, KifuAnalysisListModel::PvColumn).data().toString(), QStringLiteral("（定跡）"));
        QCOMPARE(model.row(0)->usiPv, QString());
        QCOMPARE(model.row(0)->depth, -1);
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
        QVERIFY(!model.row(0)->evaluationDifference);
        QCOMPARE(model.index(0, 5).data().toString(), QStringLiteral("-"));
        handler.append(result(1, UsiScore::cp(-160), {QStringLiteral("3c3d")}));
        QCOMPARE(model.row(1)->evaluationDifference, std::optional<int>(40));
        QCOMPARE(model.index(1, 5).data().toString(), QStringLiteral("40"));
        QVERIFY(handler.append(result(2, UsiScore{}, {QStringLiteral("2g2f")})).isNone());
        QVERIFY(model.row(2)->evaluation.isNone());
        QCOMPARE(model.index(2, 3).data().toString(), QStringLiteral("-"));
        QVERIFY(!model.row(2)->evaluationDifference);
        QVERIFY(model.index(2, 4).data().toString().isEmpty());
        handler.append(result(3, UsiScore::cp(-200), {QStringLiteral("8c8d")}));
        QVERIFY(!model.row(3)->evaluationDifference);
        handler.append(result(4, UsiScore::mate(-3), {QStringLiteral("7g7f")}));
        QVERIFY(!model.row(4)->evaluationDifference);
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
        KifuAnalysisRow row;
        row.evaluation = mateScore(score);
        model.appendRow(row);
        QCOMPARE(model.index(0, 3).data().toString(), score);
        QCOMPARE(model.index(0, 4).data().toString(), judgement);
    }
};

QTEST_MAIN(TestAnalysisResults)
#include "tst_analysis_results.moc"
