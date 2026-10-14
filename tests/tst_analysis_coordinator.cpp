/// @file tst_analysis_coordinator.cpp
/// @brief 棋譜解析コーディネータ・結果ハンドラテスト
///
/// AnalysisCoordinator のエンジン応答処理と
/// AnalysisResultHandler の結果蓄積・確定ロジックをテストする。

#include <QTest>
#include <QSignalSpy>
#include <QStringList>
#include <limits>

#include "usiinfoparser.h"
#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "kifuanalysislistmodel.h"

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
    void reset_clearsAllPending();
    void mateMetadataSurvivesCommit();
    void missingEvaluationAndDifferences();
    void mateJudgement_data();
    void mateJudgement();
    void extractUsiMoveFromKanji_basicMove();
    void extractUsiMoveFromKanji_promotion();
    void extractUsiMoveFromKanji_drop();
    void extractUsiMoveFromKanji_emptyInput();
};

// === AnalysisCoordinator 基本テスト ===

void TestAnalysisCoordinator::initialState_isIdle()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    QCOMPARE(coord.currentPly(), -1);
}

void TestAnalysisCoordinator::startAnalyzeRange_emitsPositionPrepared()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.sfenRecord = &sfenRecord;

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
    deps.sfenRecord = &sfenRecord;

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
    deps.sfenRecord = &sfenRecord;

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
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    coord.onEngineInfo(*UsiInfoParser::parse(QStringLiteral("info depth 10 seldepth 15 score cp 123 pv 7g7f 3c3d")));

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
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(0);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    coord.onEngineInfo(*UsiInfoParser::parse(QStringLiteral("info depth 20 score mate 5 pv 7g7f 3c3d 8h2b+")));

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
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisProgress);

    // 解析開始前の info は捨てる
    coord.onEngineInfo(*UsiInfoParser::parse(QStringLiteral("info depth 3 pv 7g7f")));
    QCOMPARE(spy.count(), 0);

    coord.startAnalyzeSingle(0);

    // 評価値も読み筋もない info（info string）も進捗として渡す（受け手が使う項目を選ぶ）
    coord.onEngineInfo(*UsiInfoParser::parse(QStringLiteral("info string hello world")));

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
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);

    QSignalSpy spy(&coord, &AnalysisCoordinator::analysisFinished);

    // SinglePosition モードで bestmove を受信すると Idle で完了
    coord.onEngineBestmoveReceived(QStringLiteral("bestmove 7g7f"));

    QCOMPARE(spy.count(), 1);
    auto mode = spy.at(0).at(0).value<AnalysisCoordinator::Mode>();
    QCOMPARE(mode, AnalysisCoordinator::Idle);
    QCOMPARE(coord.currentPly(), -1);
}

void TestAnalysisCoordinator::onEngineBestmove_range_advancesToNextPly()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.sfenRecord = &sfenRecord;

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
    coord.onEngineBestmoveReceived(QStringLiteral("bestmove 7g7f"));

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
    deps.sfenRecord = &sfenRecord;

    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 5000;
    coord.setOptions(opt);

    coord.startAnalyzeSingle(1);

    QSignalSpy spy(&coord, &AnalysisCoordinator::requestSearch);

    // sendGoCommand で position + go infinite の送信を要求する
    coord.sendGoCommand();

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().startsWith(QStringLiteral("position")));

    // 要求は1回だけ（保留中の position は使い切る）
    coord.sendGoCommand();
    QCOMPARE(spy.count(), 1);
}

void TestAnalysisCoordinator::setOptions_multiPV_sendSetoption()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.sfenRecord = &sfenRecord;

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
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);

    handler.updatePending(5, 150, 0, QStringLiteral("7g7f 3c3d"));

    // updatePending だけでは lastCommittedPly は更新されない
    QCOMPARE(handler.lastCommittedPly(), -1);
    QCOMPARE(handler.lastCommittedScoreCp(), 0);
}

void TestAnalysisCoordinator::commitPendingResult_updatesLastCommitted()
{
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);

    handler.updatePending(2, 200, 0, QStringLiteral("7g7f 3c3d"));
    handler.commitPendingResult();

    // commit 後は lastCommittedPly が更新される
    QCOMPARE(handler.lastCommittedPly(), 2);
    // ply=2 は偶数（先手番）なので scoreCp=200 がそのまま
    QCOMPARE(handler.lastCommittedScoreCp(), 200);

    // モデルに1行追加されている
    QCOMPARE(model.rowCount(), 1);
}

void TestAnalysisCoordinator::commitPendingResult_bookMove_clearsUsiPv()
{
    QStringList sfenRecord = makeSampleSfenRecord();
    AnalysisCoordinator::Deps deps;
    deps.sfenRecord = &sfenRecord;
    AnalysisCoordinator coord(deps);
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = 1000;
    coord.setOptions(opt);
    coord.startAnalyzeSingle(1); // currentPly=1 をセット

    AnalysisResultHandler handler;
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    refs.coord = &coord;
    handler.setRefs(refs);

    handler.updatePending(1, 120, 0, QStringLiteral("7g7f 3c3d (100.00%)"));
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
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);

    handler.updatePending(2, std::numeric_limits<int>::min(), -1, QStringLiteral("resign"), QStringLiteral("-1"));
    handler.updatePendingPvKanji(QStringLiteral("▲投了"));
    handler.commitPendingResult();

    KifuAnalysisResultsDisplay* item = model.item(0);
    QVERIFY(item != nullptr);
    QCOMPARE(item->usiPv(), QString());
    QCOMPARE(item->principalVariation(), QStringLiteral("▲投了"));
}

void TestAnalysisCoordinator::mateMetadataSurvivesCommit()
{
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);
    handler.updatePending(1, std::numeric_limits<int>::min(), 5, QStringLiteral("7g7f"));
    // nodes/time のみの追報で詰み情報が消えない。
    handler.updatePending(1, std::numeric_limits<int>::min(), 0, QString());
    handler.commitPendingResult();
    QCOMPARE(handler.lastCommittedMate(), QStringLiteral("-5"));
    handler.updatePending(2, 120, 0, QStringLiteral("7g7f"));
    handler.commitPendingResult();
    QVERIFY(handler.lastCommittedMate().isEmpty());
    QCOMPARE(handler.lastCommittedScoreCp(), 120);
    handler.updatePending(3, std::numeric_limits<int>::min(), 0, QString(), QStringLiteral("-"));
    handler.commitPendingResult();
    QCOMPARE(handler.lastCommittedMate(), QStringLiteral("+"));
    handler.reset();
    QVERIFY(handler.lastCommittedMate().isEmpty());
}

void TestAnalysisCoordinator::missingEvaluationAndDifferences()
{
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;
    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);
    handler.updatePending(0, 120, 0, QStringLiteral("7g7f"));
    handler.commitPendingResult();
    QCOMPARE(model.item(0)->evaluationDifference(), QStringLiteral("-"));
    handler.updatePending(1, -160, 0, QStringLiteral("3c3d"));
    handler.commitPendingResult();
    QCOMPARE(model.item(1)->evaluationDifference(), QStringLiteral("40"));
    handler.updatePending(2, std::numeric_limits<int>::min(), 0, QStringLiteral("2g2f"));
    handler.commitPendingResult();
    QCOMPARE(model.item(2)->evaluationValue(), QStringLiteral("-"));
    QCOMPARE(model.item(2)->evaluationDifference(), QStringLiteral("-"));
    QVERIFY(model.index(2, 4).data().toString().isEmpty());
    QCOMPARE(handler.lastCommittedScoreCp(), std::numeric_limits<int>::min());
    handler.updatePending(3, -200, 0, QStringLiteral("8c8d"));
    handler.commitPendingResult();
    QCOMPARE(model.item(3)->evaluationDifference(), QStringLiteral("-"));
    handler.updatePending(4, std::numeric_limits<int>::min(), -3, QStringLiteral("7g7f"));
    handler.commitPendingResult();
    QCOMPARE(model.item(4)->evaluationDifference(), QStringLiteral("-"));
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
    model.appendItem(new KifuAnalysisResultsDisplay(QString(), score, QString(), QString()));
    QCOMPARE(model.index(0, 4).data().toString(), judgement);
}

void TestAnalysisCoordinator::reset_clearsAllPending()
{
    AnalysisResultHandler handler;
    KifuAnalysisListModel model;

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = &model;
    handler.setRefs(refs);

    handler.updatePending(3, 100, 0, QStringLiteral("7g7f"));
    handler.commitPendingResult();
    QCOMPARE(handler.lastCommittedPly(), 3);

    handler.reset();

    QCOMPARE(handler.lastCommittedPly(), -1);
    QCOMPARE(handler.lastCommittedScoreCp(), 0);
}

void TestAnalysisCoordinator::extractUsiMoveFromKanji_basicMove()
{
    // "▲７六歩(77)" → "7g7f"
    const QString result = AnalysisResultHandler::extractUsiMoveFromKanji(
        QStringLiteral("\u2617\uFF17\u516D\u6B69(77)"));
    // ▲ = \u2617 ... actually let me use the actual characters
    // Let me construct it properly
    const QString input = QString::fromUtf8("▲７六歩(77)");
    const QString usi = AnalysisResultHandler::extractUsiMoveFromKanji(input);
    QCOMPARE(usi, QStringLiteral("7g7f"));
}

void TestAnalysisCoordinator::extractUsiMoveFromKanji_promotion()
{
    // "▲３三歩成(34)" → "3d3c+"
    const QString input = QString::fromUtf8("▲３三歩成(34)");
    const QString usi = AnalysisResultHandler::extractUsiMoveFromKanji(input);
    QCOMPARE(usi, QStringLiteral("3d3c+"));
}

void TestAnalysisCoordinator::extractUsiMoveFromKanji_drop()
{
    // "▲５五角打" → "B*5e"
    const QString input = QString::fromUtf8("▲５五角打");
    const QString usi = AnalysisResultHandler::extractUsiMoveFromKanji(input);
    QCOMPARE(usi, QStringLiteral("B*5e"));
}

void TestAnalysisCoordinator::extractUsiMoveFromKanji_emptyInput()
{
    const QString usi = AnalysisResultHandler::extractUsiMoveFromKanji(QString());
    QVERIFY(usi.isEmpty());
}

QTEST_MAIN(TestAnalysisCoordinator)
#include "tst_analysis_coordinator.moc"
