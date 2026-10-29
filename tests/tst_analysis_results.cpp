/// @file tst_analysis_results.cpp
/// @brief 棋譜解析の1局面の結果のテスト
///
/// PlyAnalysisCollector（info と bestmove から1局面の結果を作る）と
/// KifuAnalysisRowPresenter（結果を表の1行にする）をテストする。

#include <QTest>
#include <QStringList>

#include "engineanalysisrunner.h"
#include "kifuanalysisrowpresenter.h"
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

/// USI の「mate 5」「mate +」「mate +0」などから、先手から見た評価値を作る
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
    // 読み筋は平手の盤の上で、手数の偶奇で決めた手番の側から指す
    QString base = SfenUtils::hirateSfen();
    if (ply % 2 == 1) base.replace(QStringLiteral(" b "), QStringLiteral(" w "));
    analysis.pv = PvText::play(base, pv);
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
        QCOMPARE(analysis.pv.usiMoves(), (QStringList{QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QCOMPARE(KifuPresentation::pv(analysis.pv), QStringLiteral("△３四歩(33)▲２六歩(27)"));
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
        QCOMPARE(analysis.pv.usiText(), QStringLiteral("7g7f 3c3d"));
        QCOMPARE(KifuPresentation::pv(analysis.pv), QStringLiteral("▲７六歩(77)△３四歩(33)"));
    }

    void collectorDropsBookPercentAndResign()
    {
        PlyAnalysisCollector collector;
        collector.begin(0, position("position startpos"));
        collector.addInfo(info("info score cp 120 pv 7g7f 3c3d (100.00%)"));
        QCOMPARE(collector.finish(bestMove(UsiScore::cp(120))).pv.usiText(), QStringLiteral("7g7f 3c3d"));

        // 詰んだ局面ではエンジンが「pv resign」を返す。resign を候補手や読み筋の盤面表示に使わない
        collector.begin(2, position("position startpos moves 7g7f 3c3d"));
        collector.addInfo(info("info score mate -1 pv resign"));
        const PlyAnalysis resigned = collector.finish(bestMove(UsiScore::mate(-1)));
        QVERIFY(resigned.pv.usiMoves().isEmpty());
        QVERIFY(!resigned.pv.firstMove());
        QCOMPARE(KifuPresentation::pv(resigned.pv), QStringLiteral("▲投了"));
    }

    /// info が1つも届かずに bestmove が届いたら定跡
    void collectorMarksBookMove()
    {
        PlyAnalysisCollector collector;
        collector.begin(1, position("position startpos moves 7g7f"));
        const PlyAnalysis book = collector.finish(bestMove(UsiScore{}));
        QCOMPARE(book.ply, 1);
        QVERIFY(book.book);
        QVERIFY(book.pv.isEmpty());
        // 次の局面は前の局面の読み筋を引き継がない
        collector.addInfo(info("info score cp 10 pv 3c3d"));
        collector.begin(2, position("position startpos moves 7g7f 3c3d"));
        QVERIFY(collector.finish(bestMove(UsiScore{})).book);
    }

    // --- KifuAnalysisRowPresenter ---

    void appendAddsRowAndReturnsBlackScore()
    {
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        KifuAnalysisRowPresenter handler(refs);
        // ply=2 は偶数（先手番）なので評価値 200 がそのまま
        PlyAnalysis first = result(2, UsiScore::cp(200), {QStringLiteral("7g7f")});
        first.depth = 18;
        QCOMPARE(handler.append(first).value, 200);
        // ply=3 は後手番なので先手から見た値に符号を直す
        QCOMPARE(handler.append(result(3, UsiScore::cp(80), {QStringLiteral("3c3d")})).value, -80);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.row(0)->pv.usiText(), QStringLiteral("7g7f"));
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
        RecordLine line = TestReplay::recordLine(TestReplay::buildSfenRecord(hirate, moves, false),
                                                    TestReplay::buildGameMoves(hirate, moves));
        line.entries[0].displayText = QStringLiteral("▲７六歩(77)");
        NotationStyle::configure(QStringLiteral("en"), QStringLiteral("western"), false);
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        refs.line = &line;
        refs.branchPlies = {1};
        KifuAnalysisRowPresenter handler(refs);
        handler.append(result(0, UsiScore::cp(30), {QStringLiteral("7g7f")}));
        handler.append(result(1, UsiScore::cp(10), {QStringLiteral("3c3d")}));
        const auto cell = [&model](int row) { return model.index(row, KifuAnalysisListModel::MoveColumn).data().toString(); };
        QCOMPARE(model.row(1)->move, QStringLiteral("▲７六歩(77)"));
        QCOMPARE(cell(1), QStringLiteral("   1 ▲P-7f [+]"));
        NotationStyle::configure(QStringLiteral("ja_JP"), QStringLiteral("japanese"), false);
        QCOMPARE(cell(1), QStringLiteral("   1 ▲７六歩(77)+"));
        QCOMPARE(cell(0), QStringLiteral("=== 開始局面 ==="));
        NotationStyle::configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false);
    }

    /// 候補手は前の行の読み筋の最初の手。前の行の局面に至った手と同じマスへの手は「同　」と書く
    void candidateUsesSameSquareNotation()
    {
        const QString hirate = SfenUtils::hirateSfen();
        const QStringList moves{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+"),
                                QStringLiteral("3a2b")};
        const RecordLine line = TestReplay::recordLine(TestReplay::buildSfenRecord(hirate, moves, false),
                                                          TestReplay::buildGameMoves(hirate, moves));
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        refs.line = &line;
        KifuAnalysisRowPresenter rows(refs);
        const auto analyzed = [&line](int ply, const QString& pv) {
            PlyAnalysis analysis;
            analysis.ply = ply;
            analysis.score = UsiScore::cp(10);
            analysis.pv = PvText::play(line.sfenAt(ply), QStringList{pv});
            return analysis;
        };
        rows.append(analyzed(2, QStringLiteral("2g2f")));
        rows.append(analyzed(3, QStringLiteral("3a2b")));
        rows.append(analyzed(4, QStringLiteral("2g2f")));
        // 3手目の候補手は2手目の読み筋（▲２六歩）で、指した角成とは違う
        QCOMPARE(model.row(1)->candidateMove, QStringLiteral("▲２六歩(27)"));
        QVERIFY(!model.row(1)->matchesCandidate());
        // 4手目の候補手は角を取り返す手。直前の角成と同じマスなので「同　」
        QCOMPARE(model.row(2)->candidateMove, QStringLiteral("△同　銀(31)"));
        QVERIFY(model.row(2)->matchesCandidate());
    }

    void bookResultHasNoScoreOrPv()
    {
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        KifuAnalysisRowPresenter handler(refs);
        PlyAnalysis book;
        book.ply = 1;
        book.book = true;
        QVERIFY(handler.append(book).isNone());
        QVERIFY(model.row(0)->book);
        QVERIFY(model.row(0)->pv.isEmpty());
        // 定跡の印は表示するときに表示言語で作る（行には書かない）
        QCOMPARE(model.index(0, KifuAnalysisListModel::PvColumn).data().toString(), QStringLiteral("（定跡）"));
        QCOMPARE(model.row(0)->depth, -1);
    }

    void mateScoresKeepTheirSide()
    {
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        KifuAnalysisRowPresenter handler(refs);
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
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        KifuAnalysisRowPresenter handler(refs);
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
        QTest::addColumn<QString>("display");
        QTest::addColumn<QString>("judgement");
        // 評価値の列は思考の表と同じ表記（手数が無ければ「詰み」）。どちらの勝ちかは形勢の列で示す
        QTest::newRow("positive") << QStringLiteral("mate 5") << QStringLiteral("5手詰") << QStringLiteral("先手勝ち");
        QTest::newRow("negative") << QStringLiteral("mate -5") << QStringLiteral("-5手詰") << QStringLiteral("後手勝ち");
        QTest::newRow("positive-sign") << QStringLiteral("mate +") << QStringLiteral("詰み") << QStringLiteral("先手勝ち");
        QTest::newRow("negative-sign") << QStringLiteral("mate -") << QStringLiteral("詰み") << QStringLiteral("後手勝ち");
        QTest::newRow("mated") << QStringLiteral("mate 0") << QStringLiteral("0手詰") << QStringLiteral("後手勝ち");
        QTest::newRow("opponent-mated") << QStringLiteral("mate +0") << QStringLiteral("0手詰") << QStringLiteral("先手勝ち");
    }

    void mateJudgement()
    {
        QFETCH(QString, score);
        QFETCH(QString, display);
        QFETCH(QString, judgement);
        KifuAnalysisListModel model;
        KifuAnalysisRow row;
        row.evaluation = mateScore(score);
        model.appendRow(row);
        QCOMPARE(model.index(0, 3).data().toString(), display);
        QCOMPARE(model.index(0, 4).data().toString(), judgement);
    }
};

QTEST_MAIN(TestAnalysisResults)
#include "tst_analysis_results.moc"
