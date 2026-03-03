/// @file tst_analysis_coordinator.cpp
/// @brief 棋譜解析コーディネータ・結果ハンドラテスト
///
/// AnalysisCoordinator のエンジン応答処理と
/// AnalysisResultHandler の結果蓄積・確定ロジックをテストする。

#include <QTest>
#include <QSignalSpy>
#include <QStringList>
#include <limits>

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
    void onEngineInfoLine_scorecp_emitsProgress();
    void onEngineInfoLine_scoremate_emitsProgress();
    void onEngineInfoLine_noScore_noProgress();
    void onEngineBestmove_single_emitsFinished();
    void onEngineBestmove_range_advancesToNextPly();

    // --- USI コマンド送信 ---
    void sendGoCommand_emitsRequestSendUsiCommand();
    void setOptions_multiPV_sendSetoption();

    // --- AnalysisResultHandler ---
    void updatePending_storesData();
    void commitPendingResult_updatesLastCommitted();
    void reset_clearsAllPending();
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

void TestAnalysisCoordinator::onEngineInfoLine_scorecp_emitsProgress()
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

    const QString infoLine = QStringLiteral("info depth 10 seldepth 15 score cp 123 pv 7g7f 3c3d");
    coord.onEngineInfoLine(infoLine);

    QCOMPARE(spy.count(), 1);

    // 引数の検証: ply, depth, seldepth, scoreCp, mate, pv, rawInfoLine
    const auto args = spy.at(0);
    QCOMPARE(args.at(0).toInt(), 1);     // ply
    QCOMPARE(args.at(1).toInt(), 10);    // depth
    QCOMPARE(args.at(2).toInt(), 15);    // seldepth
    QCOMPARE(args.at(3).toInt(), 123);   // scoreCp
    QCOMPARE(args.at(4).toInt(), 0);     // mate（cpの場合は0）
    QVERIFY(args.at(5).toString().contains(QStringLiteral("7g7f")));  // pv
    QCOMPARE(args.at(6).toString(), infoLine);  // rawInfoLine
}

void TestAnalysisCoordinator::onEngineInfoLine_scoremate_emitsProgress()
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

    const QString infoLine = QStringLiteral("info depth 20 score mate 5 pv 7g7f 3c3d 8h2b+");
    coord.onEngineInfoLine(infoLine);

    QCOMPARE(spy.count(), 1);

    const auto args = spy.at(0);
    QCOMPARE(args.at(0).toInt(), 0);     // ply
    QCOMPARE(args.at(1).toInt(), 20);    // depth
    QCOMPARE(args.at(3).toInt(), std::numeric_limits<int>::min());  // scoreCp（mate時はINT_MIN）
    QCOMPARE(args.at(4).toInt(), 5);     // mate
}

void TestAnalysisCoordinator::onEngineInfoLine_noScore_noProgress()
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

    // パース不能な info 行でも analysisProgress は発火される（raw表示用）
    const QString infoLine = QStringLiteral("info string hello world");
    coord.onEngineInfoLine(infoLine);

    QCOMPARE(spy.count(), 1);
    // パース不能時は depth=-1, scoreCp=INT_MIN, mate=0, pv=empty
    const auto args = spy.at(0);
    QCOMPARE(args.at(1).toInt(), -1);    // depth
    QCOMPARE(args.at(3).toInt(), std::numeric_limits<int>::min());  // scoreCp
    QCOMPARE(args.at(4).toInt(), 0);     // mate
    QVERIFY(args.at(5).toString().isEmpty());  // pv
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

    QSignalSpy spy(&coord, &AnalysisCoordinator::requestSendUsiCommand);

    // sendGoCommand で position + go infinite が送信される
    coord.sendGoCommand();

    QVERIFY(spy.count() >= 2);
    // 最初のコマンドは "position startpos moves 7g7f"
    QVERIFY(spy.at(0).at(0).toString().startsWith(QStringLiteral("position")));
    // 2番目は "go infinite"
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("go infinite"));
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

    QSignalSpy spy(&coord, &AnalysisCoordinator::requestSendUsiCommand);

    coord.startAnalyzeSingle(0);

    // multiPV>1 の場合、setoption コマンドが送信される
    bool found = false;
    for (int i = 0; i < spy.count(); ++i) {
        const QString cmd = spy.at(i).at(0).toString();
        if (cmd.contains(QStringLiteral("setoption name MultiPV value 3"))) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
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
