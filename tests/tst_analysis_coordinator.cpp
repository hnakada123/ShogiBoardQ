/// @file tst_analysis_coordinator.cpp
/// @brief 棋譜解析コーディネータ・結果ハンドラテスト
///
/// AnalysisCoordinator のエンジン応答処理と
/// AnalysisResultHandler の結果蓄積・確定ロジックをテストする。

#include <QTest>
#include <QSignalSpy>
#include <QStringList>
#include <limits>

#include "usilineparser.h"
#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "kifuanalysislistmodel.h"

namespace {
UsiInfo info(const char* line)
{
    return *UsiLineParser::parseInfo(QString::fromUtf8(line));
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
} // namespace

class TestAnalysisCoordinator : public QObject
{
    Q_OBJECT

private:
    /// テスト用の sfenRecord を作成（平手→7六歩→3四歩→2六歩）
    QStringList makeSampleSfenRecord()
    {
        return {
            QStringLiteral("position startpos"),
            QStringLiteral("position startpos moves 7g7f"),
            QStringLiteral("position startpos moves 7g7f 3c3d"),
            QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"),
        };
    }

private slots:
    // --- AnalysisCoordinator 基本 ---
    void initialState_isIdle();
    void startAnalyzeRange_emitsPositionPrepared();
    void startAnalyzeSingle_emitsPositionPrepared();
    void stop_returnsToIdle();

    // --- エンジン応答処理 ---
    void onEngineInfo_scorecp_emitsProgress();
    void onEngineInfo_scoremate_emitsProgress();
    void onEngineInfo_noScore_emitsProgress();
    void onEngineBestmove_single_emitsFinished();
    void onEngineBestmove_range_advancesToNextPly();

    // --- USI コマンド送信 ---
    void sendGoCommand_emitsRequestSendUsiCommand();
    void setOptions_multiPV_sendSetoption();

    // --- AnalysisResultHandler ---
    void updatePending_storesData();
    void commitPendingResult_updatesLastCommitted();
    void commitPendingResult_bookMove_clearsUsiPv();
    void commitPendingResult_resignIsNotAMove();
    void resetLastCommitted_forgetsCommittedPly();
    void mateMetadataSurvivesCommit();
    void missingEvaluationAndDifferences();
    void mateJudgement_data();
    void mateJudgement();
};

// === AnalysisCoordinator 基本テスト ===

void TestAnalysisCoordinator::initialState_isIdle()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    QCOMPARE(coord.currentPly(), -1);
}

void TestAnalysisCoordinator::startAnalyzeRange_emitsPositionPrepared()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.startPly = 0;
    opt.endPly = 2;
    opt.movetimeMs = 1000;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::positionPrepared);
    coord.startAnalyzeRange();

    QCOMPARE(spy.count(), 1);
    // 最初のplyは startPly=0
    QCOMPARE(spy.at(0).at(0).toInt(), 0);
    QCOMPARE(coord.currentPly(), 0);
}

void TestAnalysisCoordinator::startAnalyzeSingle_emitsPositionPrepared()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 1000;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::positionPrepared);
    coord.startAnalyzeSingle(2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 2);
    QCOMPARE(coord.currentPly(), 2);
}

void TestAnalysisCoordinator::stop_returnsToIdle()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 1000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);
    QVERIFY(coord.currentPly() >= 0);

    QSignalSpy finSpy(&coord, &AnalysisCoordinator::analysisFinished);
    coord.stop();

    QCOMPARE(coord.currentPly(), -1);
    QCOMPARE(finSpy.count(), 1);
    // stop() は Idle モードで analysisFinished を発行
    auto mode = finSpy.at(0).at(0).value<AnalysisCoordinator::Mode>();
    QCOMPARE(mode, AnalysisCoordinator::Idle);
}

// === エンジン応答処理テスト ===

void TestAnalysisCoordinator::onEngineInfo_scorecp_emitsProgress()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    coord.onEngineInfo(*UsiLineParser::parseInfo(QStringLiteral("info depth 10 seldepth 15 score cp 123 pv 7g7f 3c3d")));

    QCOMPARE(spy.count(), 1);

    // 引数の検証: ply, info
    const auto args = spy.at(0);
    QCOMPARE(args.at(0).toInt(), 1);
    const auto info = qvariant_cast<UsiInfo>(args.at(1));
    QCOMPARE(info.depth, 10);
    QCOMPARE(info.seldepth, 15);
    QVERIFY(info.score.isCp());
    QCOMPARE(info.score.value, 123);
    QCOMPARE(info.pv, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
}

void TestAnalysisCoordinator::onEngineInfo_scoremate_emitsProgress()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(0);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    coord.onEngineInfo(*UsiLineParser::parseInfo(QStringLiteral("info depth 20 score mate 5 pv 7g7f 3c3d 8h2b+")));

    QCOMPARE(spy.count(), 1);

    const auto args = spy.at(0);
    QCOMPARE(args.at(0).toInt(), 0);
    const auto info = qvariant_cast<UsiInfo>(args.at(1));
    QCOMPARE(info.depth, 20);
    QVERIFY(info.score.isMate());
    QCOMPARE(info.score.value, 5);
}

void TestAnalysisCoordinator::onEngineInfo_noScore_emitsProgress()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    // 解析開始前の info は捨てる
    coord.onEngineInfo(*UsiLineParser::parseInfo(QStringLiteral("info depth 3 pv 7g7f")));
    QCOMPARE(spy.count(), 0);

    coord.startAnalyzeSingle(0);

    // 評価値も読み筋もない info（info string）も進捗として渡す（受け手が使う項目を選ぶ）
    coord.onEngineInfo(*UsiLineParser::parseInfo(QStringLiteral("info string hello world")));

    QCOMPARE(spy.count(), 1);
    const auto info = qvariant_cast<UsiInfo>(spy.at(0).at(1));
    QCOMPARE(info.depth, -1);
    QVERIFY(!info.hasScore());
    QVERIFY(!info.hasPv());
    QCOMPARE(info.string, QStringLiteral("hello world"));
}

void TestAnalysisCoordinator::onEngineBestmove_single_emitsFinished()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisFinished);

    // SinglePosition モードで bestmove を受信すると Idle で完了
    coord.onEngineBestmoveReceived();

    QCOMPARE(spy.count(), 1);
    auto mode = spy.at(0).at(0).value<AnalysisCoordinator::Mode>();
    QCOMPARE(mode, AnalysisCoordinator::Idle);
    QCOMPARE(coord.currentPly(), -1);
}

void TestAnalysisCoordinator::onEngineBestmove_range_advancesToNextPly()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.startPly = 0;
    opt.endPly = 2;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeRange();
    QCOMPARE(coord.currentPly(), 0);

    QSignalSpy posSpy(&coord, &AnalysisCoordinator::positionPrepared);

    // 最初の bestmove → 次の ply へ進む
    coord.onEngineBestmoveReceived();

    QCOMPARE(coord.currentPly(), 1);
    // positionPrepared が次の ply 用に発火
    QCOMPARE(posSpy.count(), 1);
    QCOMPARE(posSpy.at(0).at(0).toInt(), 1);
}

// === USI コマンド送信テスト ===

void TestAnalysisCoordinator::sendGoCommand_emitsRequestSendUsiCommand()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::requestSearch);
    // UI からの呼び返しがなくても、開始時に探索要求が一度だけ発行される。
    coord.startAnalyzeSingle(1);
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().startsWith(QStringLiteral("position")));
    QCOMPARE(spy.at(0).at(1).toInt(), 5000);  // この時間で stop を送ってもらう
    coord.stop();
    coord.onEngineBestmoveReceived();
    QCOMPARE(spy.count(), 1);
}

void TestAnalysisCoordinator::setOptions_multiPV_sendSetoption()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 1000;
    opt.multiPV = 3;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::requestSetOption);

    coord.startAnalyzeSingle(0);

    // multiPV>1 の場合、MultiPV の setoption を要求する
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("MultiPV"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("3"));
}

// === AnalysisResultHandler テスト ===

void TestAnalysisCoordinator::updatePending_storesData()
{
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);

    handler.updatePending(5, info("info score cp 150 pv 7g7f 3c3d"));

    // updatePending だけでは lastCommittedPly は更新されない
    QCOMPARE(handler.lastCommittedPly(), -1);
    QVERIFY(handler.lastCommittedScore().isNone());
}

void TestAnalysisCoordinator::commitPendingResult_updatesLastCommitted()
{
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);

    handler.updatePending(2, info("info score cp 200 pv 7g7f 3c3d"));
    handler.commitPendingResult();

    // commit 後は lastCommittedPly が更新される
    QCOMPARE(handler.lastCommittedPly(), 2);
    // ply=2 は偶数（先手番）なので評価値 200 がそのまま
    QCOMPARE(handler.lastCommittedScore().value, 200);

    // モデルに1行追加されている
    QCOMPARE(model.rowCount(), 1);
}

void TestAnalysisCoordinator::commitPendingResult_bookMove_clearsUsiPv()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.positions = sfenRecord;
    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 1000;
    coord.setOptions(opt);
    coord.startAnalyzeSingle(1); // currentPly=1 をセット

    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    refs.coord = &coord;
    AnalysisResultHandler handler(refs);

    handler.updatePending(1, info("info score cp 120 pv 7g7f 3c3d (100.00%)"));
    handler.commitPendingResult();
    QCOMPARE(model.rowCount(), 1);

    KifuAnalysisResultsDisplay* first = model.item(0);
    QVERIFY(first != nullptr);
    QCOMPARE(first->usiPv(), QStringLiteral("7g7f 3c3d"));

    // info 行なしで bestmove が来た想定（定跡）
    handler.commitPendingResult();
    QCOMPARE(model.rowCount(), 2);

    KifuAnalysisResultsDisplay* second = model.item(1);
    QVERIFY(second != nullptr);
    QCOMPARE(second->principalVariation(), QStringLiteral("（定跡）"));
    QCOMPARE(second->usiPv(), QString());
}

void TestAnalysisCoordinator::commitPendingResult_resignIsNotAMove()
{
    // 詰んだ局面ではエンジンが「pv resign」を返す。resign を候補手や読み筋の盤面表示に使わない
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);

    handler.updatePending(2, info("info score mate -1 pv resign"));
    handler.updatePendingPvKanji(QStringLiteral("▲投了"));
    handler.commitPendingResult();

    KifuAnalysisResultsDisplay* item = model.item(0);
    QVERIFY(item != nullptr);
    QCOMPARE(item->usiPv(), QString());
    QCOMPARE(item->principalVariation(), QStringLiteral("▲投了"));
}

void TestAnalysisCoordinator::mateMetadataSurvivesCommit()
{
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);
    handler.updatePending(1, info("info score mate 5 pv 7g7f"));
    // nodes/time のみの追報で詰み情報が消えない。
    handler.updatePending(1, info("info nodes 100"));
    handler.commitPendingResult();
    // 後手番の局面なので先手から見ると詰まされる側
    QVERIFY(handler.lastCommittedScore().isMate());
    QCOMPARE(handler.lastCommittedScore().mateText(), QStringLiteral("-5"));
    handler.updatePending(2, info("info score cp 120 pv 7g7f"));
    handler.commitPendingResult();
    QVERIFY(handler.lastCommittedScore().isCp());
    QCOMPARE(handler.lastCommittedScore().value, 120);
    handler.updatePending(3, info("info score mate -"));
    handler.commitPendingResult();
    QCOMPARE(handler.lastCommittedScore().mateText(), QStringLiteral("+"));
}

void TestAnalysisCoordinator::missingEvaluationAndDifferences()
{
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);
    handler.updatePending(0, info("info score cp 120 pv 7g7f"));
    handler.commitPendingResult();
    QVERIFY(!model.item(0)->evaluationDifference());
    QCOMPARE(model.index(0, 5).data().toString(), QStringLiteral("-"));
    handler.updatePending(1, info("info score cp -160 pv 3c3d"));
    handler.commitPendingResult();
    QCOMPARE(model.item(1)->evaluationDifference(), std::optional<int>(40));
    QCOMPARE(model.index(1, 5).data().toString(), QStringLiteral("40"));
    handler.updatePending(2, info("info pv 2g2f"));
    handler.commitPendingResult();
    QVERIFY(model.item(2)->evaluation().isNone());
    QCOMPARE(model.index(2, 3).data().toString(), QStringLiteral("-"));
    QVERIFY(!model.item(2)->evaluationDifference());
    QVERIFY(model.index(2, 4).data().toString().isEmpty());
    QVERIFY(handler.lastCommittedScore().isNone());
    handler.updatePending(3, info("info score cp -200 pv 8c8d"));
    handler.commitPendingResult();
    QVERIFY(!model.item(3)->evaluationDifference());
    handler.updatePending(4, info("info score mate -3 pv 7g7f"));
    handler.commitPendingResult();
    QVERIFY(!model.item(4)->evaluationDifference());
}

void TestAnalysisCoordinator::mateJudgement_data()
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

void TestAnalysisCoordinator::mateJudgement()
{
    QFETCH(QString, score);
    QFETCH(QString, judgement);
    KifuAnalysisListModel model;
    model.appendItem(new KifuAnalysisResultsDisplay(QString(), mateScore(score), std::nullopt, QString()));
    QCOMPARE(model.index(0, 3).data().toString(), score);
    QCOMPARE(model.index(0, 4).data().toString(), judgement);
}

void TestAnalysisCoordinator::resetLastCommitted_forgetsCommittedPly()
{
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    AnalysisResultHandler handler(refs);

    handler.updatePending(3, info("info score cp 100 pv 7g7f"));
    handler.commitPendingResult();
    QCOMPARE(handler.lastCommittedPly(), 3);

    handler.resetLastCommitted();

    QCOMPARE(handler.lastCommittedPly(), -1);
}

QTEST_MAIN(TestAnalysisCoordinator)
#include "tst_analysis_coordinator.moc"
