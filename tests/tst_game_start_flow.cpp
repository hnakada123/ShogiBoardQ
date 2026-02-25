/// @file tst_game_start_flow.cpp
/// @brief 対局開始フローの回帰テスト
///
/// GameStartCoordinator の PlayMode 判定、start() バリデーション、
/// 時間制御正規化、シグナル発火をテストする。

#include <QTest>
#include <QSignalSpy>

#include "gamestartcoordinator.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "playmode.h"
#include "kifurecordlistmodel.h"
#include "shogiview.h"

// test_stubs_game_start_flow.cpp で定義されたテスト追跡用変数
namespace TestTracker {
    extern bool configureAndStartCalled;
    extern bool startInitialEngineMoveIfNeededCalled;
    extern bool setTimeControlConfigCalled;
    extern bool refreshGoTimesCalled;
    extern bool clearGameOverStateCalled;
    extern MatchCoordinator::StartOptions lastStartOptions;
    extern bool lastUseByoyomi;
    extern int lastByoyomiMs1;
    extern int lastByoyomiMs2;
    extern int lastIncMs1;
    extern int lastIncMs2;
    extern bool lastLoseOnTimeout;
    extern void reset();
}

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// テスト用ハーネス: GameStartCoordinator + MatchCoordinator のセットアップ
struct TestHarness {
    MatchCoordinator::Deps matchDeps;
    MatchCoordinator* match = nullptr;

    GameStartCoordinator::Deps gscDeps;
    GameStartCoordinator* gsc = nullptr;

    ShogiClock clock;

    TestHarness()
    {
        match = new MatchCoordinator(matchDeps);
        gscDeps.match = match;
        gscDeps.clock = &clock;
        gsc = new GameStartCoordinator(gscDeps);
        TestTracker::reset();
    }

    ~TestHarness()
    {
        delete gsc;
        delete match;
    }

    /// HvH平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeHvhParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.mode = PlayMode::HumanVsHuman;
        p.opt.sfenStart = kHirateSfen;
        p.autoStartEngineMove = false;
        return p;
    }

    /// HvE平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeHveParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.mode = PlayMode::EvenHumanVsEngine;
        p.opt.sfenStart = kHirateSfen;
        p.opt.engineIsP2 = true;
        p.opt.enginePath2 = QStringLiteral("/path/to/engine");
        p.opt.engineName2 = QStringLiteral("TestEngine");
        p.autoStartEngineMove = false;
        return p;
    }

    /// EvE平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeEveParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.mode = PlayMode::EvenEngineVsEngine;
        p.opt.sfenStart = kHirateSfen;
        p.opt.engineIsP1 = true;
        p.opt.engineIsP2 = true;
        p.opt.enginePath1 = QStringLiteral("/path/to/engine1");
        p.opt.enginePath2 = QStringLiteral("/path/to/engine2");
        p.opt.engineName1 = QStringLiteral("Engine1");
        p.opt.engineName2 = QStringLiteral("Engine2");
        p.autoStartEngineMove = true;
        return p;
    }
};

class TestGameStartFlow : public QObject
{
    Q_OBJECT

private slots:
    // ============================================================
    // A) PlayMode 判定: determinePlayModeAlignedWithTurn()
    //    （静的メソッド、純粋ロジック）
    // ============================================================

    // --- 平手（initPositionNumber == 1）---

    void playMode_even_hvh()
    {
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, true, true, kHirateSfen);
        QCOMPARE(mode, PlayMode::HumanVsHuman);
    }

    void playMode_even_hve_senteTurn()
    {
        // 先手:人間, 後手:エンジン, SFEN手番=先手(b)
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, true, false, kHirateSfen);
        QCOMPARE(mode, PlayMode::EvenHumanVsEngine);
    }

    void playMode_even_evh_senteTurn()
    {
        // 先手:エンジン, 後手:人間, SFEN手番=先手(b)
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, false, true, kHirateSfen);
        QCOMPARE(mode, PlayMode::EvenEngineVsHuman);
    }

    void playMode_even_eve()
    {
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, false, false, kHirateSfen);
        QCOMPARE(mode, PlayMode::EvenEngineVsEngine);
    }

    void playMode_even_hve_goteTurn()
    {
        // 手番が後手(w)の場合:
        // isPlayer1Human=true → 先手が人間なのにSFEN手番がw
        // → turn == 'w' かつ isPlayer2Human == false
        //    → p2 は人間でない → EvenHumanVsEngine
        const QString goteSfen =
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, true, false, goteSfen);
        QCOMPARE(mode, PlayMode::EvenHumanVsEngine);
    }

    void playMode_even_evh_goteTurn()
    {
        // 手番が後手(w), 先手:エンジン, 後手:人間
        const QString goteSfen =
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, false, true, goteSfen);
        QCOMPARE(mode, PlayMode::EvenEngineVsHuman);
    }

    void playMode_even_noTurnFallback()
    {
        // SFEN手番情報なし → 座席で判定
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, true, false, QStringLiteral("invalid_sfen"));
        QCOMPARE(mode, PlayMode::EvenHumanVsEngine);
    }

    // --- 駒落ち（initPositionNumber != 1）---

    void playMode_handicap_hvh()
    {
        // 二枚落ち SFEN (手番=後手)
        const QString nimaiochiSfen =
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            7, true, true, nimaiochiSfen);
        QCOMPARE(mode, PlayMode::HumanVsHuman);
    }

    void playMode_handicap_hve_goteTurn()
    {
        // 駒落ち: 先手が人間（下手）、後手がエンジン（上手）
        // SFEN手番は後手(w) → HandicapHumanVsEngine
        const QString nimaiochiSfen =
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            7, true, false, nimaiochiSfen);
        // turn='w', isPlayer2Human=false → HandicapHumanVsEngine
        QCOMPARE(mode, PlayMode::HandicapHumanVsEngine);
    }

    void playMode_handicap_evh_goteTurn()
    {
        // 駒落ち: 先手がエンジン、後手が人間
        const QString nimaiochiSfen =
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            7, false, true, nimaiochiSfen);
        QCOMPARE(mode, PlayMode::HandicapEngineVsHuman);
    }

    void playMode_handicap_eve()
    {
        const QString nimaiochiSfen =
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            7, false, false, nimaiochiSfen);
        QCOMPARE(mode, PlayMode::HandicapEngineVsEngine);
    }

    void playMode_handicap_senteTurn()
    {
        // 駒落ちでも先手(b)表記があるSFEN
        const QString handicapSenteStart =
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            7, true, false, handicapSenteStart);
        // turn='b', isPlayer1Human=true → HandicapHumanVsEngine
        QCOMPARE(mode, PlayMode::HandicapHumanVsEngine);
    }

    void playMode_allHandicapPositions()
    {
        // 各種駒落ちポジション番号(2-14)で EvE が正しく判定されることを確認
        const QStringList handicapSfens = {
            QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"), // 香落ち
            QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),   // 角落ち
            QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),   // 飛車落ち
            QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),     // 二枚落ち
            QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"),           // 十枚落ち
        };
        const int posNos[] = { 2, 4, 5, 7, 14 };

        for (int i = 0; i < handicapSfens.size(); ++i) {
            const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
                posNos[i], false, false, handicapSfens.at(i));
            QCOMPARE(mode, PlayMode::HandicapEngineVsEngine);
        }
    }

    // ============================================================
    // B) start() バリデーション
    // ============================================================

    void start_nullMatch_noSignalEmitted()
    {
        // MatchCoordinator が null の場合、started シグナルが発火しないこと
        GameStartCoordinator::Deps deps;
        deps.match = nullptr;
        GameStartCoordinator gsc(deps);
        QSignalSpy spy(&gsc, &GameStartCoordinator::started);

        GameStartCoordinator::StartParams p;
        p.opt.mode = PlayMode::HumanVsHuman;
        p.opt.sfenStart = kHirateSfen;

        gsc.start(p);

        QCOMPARE(spy.count(), 0);
    }

    void start_notStartedMode_noSignalEmitted()
    {
        // PlayMode::NotStarted の場合、started シグナルが発火しないこと
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        GameStartCoordinator::StartParams p;
        p.opt.mode = PlayMode::NotStarted;

        h.gsc->start(p);

        QCOMPARE(spy.count(), 0);
        QVERIFY(!TestTracker::configureAndStartCalled);
    }

    // ============================================================
    // C) start() 正常系: HvH, HvE, EvE
    // ============================================================

    void start_hvh_emitsStartedSignal()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        auto p = h.makeHvhParams();
        h.gsc->start(p);

        QCOMPARE(spy.count(), 1);
        QVERIFY(TestTracker::configureAndStartCalled);
        QCOMPARE(TestTracker::lastStartOptions.mode, PlayMode::HumanVsHuman);
    }

    void start_hve_configuresCalled()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        auto p = h.makeHveParams();
        h.gsc->start(p);

        QCOMPARE(spy.count(), 1);
        QVERIFY(TestTracker::configureAndStartCalled);
        QCOMPARE(TestTracker::lastStartOptions.mode, PlayMode::EvenHumanVsEngine);
    }

    void start_eve_triggersInitialEngineMove()
    {
        TestHarness h;

        auto p = h.makeEveParams();
        h.gsc->start(p);

        QVERIFY(TestTracker::configureAndStartCalled);
        QVERIFY2(TestTracker::startInitialEngineMoveIfNeededCalled,
                 "EvE should trigger initial engine move");
    }

    void start_hvh_noInitialEngineMove()
    {
        TestHarness h;

        auto p = h.makeHvhParams();
        p.autoStartEngineMove = false;
        h.gsc->start(p);

        QVERIFY(!TestTracker::startInitialEngineMoveIfNeededCalled);
    }

    // ============================================================
    // D) TimeControl 正規化テスト
    // ============================================================

    void start_byoyomiPriorityOverIncrement()
    {
        // 秒読みと加算が同時に指定された場合、秒読みが優先される
        TestHarness h;

        auto p = h.makeHvhParams();
        p.tc.enabled = true;
        p.tc.p1.baseMs = 600000;      // 10分
        p.tc.p2.baseMs = 600000;
        p.tc.p1.byoyomiMs = 30000;    // 30秒秒読み
        p.tc.p2.byoyomiMs = 30000;
        p.tc.p1.incrementMs = 10000;  // 10秒加算（秒読みがあるので無視されるべき）
        p.tc.p2.incrementMs = 10000;

        h.gsc->start(p);

        QVERIFY(TestTracker::setTimeControlConfigCalled);
        QVERIFY2(TestTracker::lastUseByoyomi, "byoyomi should be used when both specified");
        QCOMPARE(TestTracker::lastByoyomiMs1, 30000);
        QCOMPARE(TestTracker::lastByoyomiMs2, 30000);
        // increment は 0 にリセットされるべき
        QCOMPARE(TestTracker::lastIncMs1, 0);
        QCOMPARE(TestTracker::lastIncMs2, 0);
    }

    void start_incrementOnlyWhenNoByoyomi()
    {
        // 秒読みなし、加算ありの場合
        TestHarness h;

        auto p = h.makeHvhParams();
        p.tc.enabled = true;
        p.tc.p1.baseMs = 600000;
        p.tc.p2.baseMs = 600000;
        p.tc.p1.byoyomiMs = 0;
        p.tc.p2.byoyomiMs = 0;
        p.tc.p1.incrementMs = 5000;   // 5秒加算
        p.tc.p2.incrementMs = 5000;

        h.gsc->start(p);

        QVERIFY(TestTracker::setTimeControlConfigCalled);
        QVERIFY2(!TestTracker::lastUseByoyomi, "byoyomi should not be used");
        QCOMPARE(TestTracker::lastIncMs1, 5000);
        QCOMPARE(TestTracker::lastIncMs2, 5000);
    }

    void start_autoEnableWhenValuesPresent()
    {
        // enabled=false だが時間値が設定されている場合、自動で有効化される
        TestHarness h;
        QSignalSpy tcSpy(h.gsc, &GameStartCoordinator::requestApplyTimeControl);

        auto p = h.makeHvhParams();
        p.tc.enabled = false;            // 明示的にfalse
        p.tc.p1.baseMs = 3600000;        // 1時間（値あり）
        p.tc.p2.baseMs = 3600000;

        h.gsc->start(p);

        // requestApplyTimeControl が発火していること（値があるので有効化）
        QVERIFY(tcSpy.count() > 0);
    }

    void start_noTimeControlWhenAllZero()
    {
        // 全て0の場合、requestApplyTimeControl は発火しない
        TestHarness h;
        QSignalSpy tcSpy(h.gsc, &GameStartCoordinator::requestApplyTimeControl);

        auto p = h.makeHvhParams();
        p.tc.enabled = false;
        p.tc.p1.baseMs = 0;
        p.tc.p2.baseMs = 0;
        p.tc.p1.byoyomiMs = 0;
        p.tc.p2.byoyomiMs = 0;
        p.tc.p1.incrementMs = 0;
        p.tc.p2.incrementMs = 0;

        h.gsc->start(p);

        QCOMPARE(tcSpy.count(), 0);
    }

    void start_asymmetricTimeControl()
    {
        // 先手と後手で異なる持ち時間設定
        TestHarness h;

        auto p = h.makeHvhParams();
        p.tc.enabled = true;
        p.tc.p1.baseMs = 3600000;     // 先手: 1時間
        p.tc.p1.byoyomiMs = 60000;    // 先手: 60秒秒読み
        p.tc.p2.baseMs = 1800000;     // 後手: 30分
        p.tc.p2.byoyomiMs = 30000;    // 後手: 30秒秒読み

        h.gsc->start(p);

        QVERIFY(TestTracker::setTimeControlConfigCalled);
        QCOMPARE(TestTracker::lastByoyomiMs1, 60000);
        QCOMPARE(TestTracker::lastByoyomiMs2, 30000);
    }

    // ============================================================
    // E) prepare() シグナルテスト
    // ============================================================

    void prepare_emitsPreStartCleanup()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::requestPreStartCleanup);

        GameStartCoordinator::Request req;
        req.mode = static_cast<int>(PlayMode::HumanVsHuman);
        req.startSfen = kHirateSfen;
        req.clock = &h.clock;
        req.skipCleanup = false;

        h.gsc->prepare(req);

        QCOMPARE(spy.count(), 1);
    }

    void prepare_skipCleanup_noCleanupSignal()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::requestPreStartCleanup);

        GameStartCoordinator::Request req;
        req.mode = static_cast<int>(PlayMode::HumanVsHuman);
        req.startSfen = kHirateSfen;
        req.clock = &h.clock;
        req.skipCleanup = true;

        h.gsc->prepare(req);

        QCOMPARE(spy.count(), 0);
    }

    // ============================================================
    // F) 連続対局設定（StartOptions プロパティ検証）
    // ============================================================

    void start_maxMovesPassedThrough()
    {
        TestHarness h;

        auto p = h.makeHvhParams();
        p.opt.maxMoves = 256;

        h.gsc->start(p);

        QVERIFY(TestTracker::configureAndStartCalled);
        QCOMPARE(TestTracker::lastStartOptions.maxMoves, 256);
    }

    void start_autoSaveKifuPassedThrough()
    {
        TestHarness h;

        auto p = h.makeHvhParams();
        p.opt.autoSaveKifu = true;
        p.opt.kifuSaveDir = QStringLiteral("/tmp/kifu");

        h.gsc->start(p);

        QVERIFY(TestTracker::configureAndStartCalled);
        QVERIFY(TestTracker::lastStartOptions.autoSaveKifu);
        QCOMPARE(TestTracker::lastStartOptions.kifuSaveDir, QStringLiteral("/tmp/kifu"));
    }

    // ============================================================
    // G) prepareDataCurrentPosition / prepareInitialPosition
    // ============================================================

    void prepareInitialPosition_setsCorrectSfen()
    {
        // prepareInitialPosition は startSfenStr/currentSfenStr を更新する
        TestHarness h;

        QString startSfen;
        QString currentSfen;
        QStringList sfenRecord;
        KifuRecordListModel kifuModel;

        // 平手の StartGameDialog を模擬（スタブは startingPositionNumber() = 0 を返す）
        // → prepareInitialPosition は idx を 1（平手）にクランプする
        GameStartCoordinator::Ctx ctx;
        ctx.view = nullptr;    // スタブなので null でも大丈夫（view操作はスキップ）
        ctx.gc = nullptr;
        ctx.startSfenStr = &startSfen;
        ctx.currentSfenStr = &currentSfen;
        ctx.sfenRecord = &sfenRecord;
        ctx.kifuModel = &kifuModel;
        ctx.startDlg = nullptr;  // ダイアログ null → デフォルトで平手(1)

        h.gsc->prepareInitialPosition(ctx);

        // 平手 SFEN が設定されているべき
        QCOMPARE(startSfen, kHirateSfen);
        QCOMPARE(currentSfen, kHirateSfen);
        // sfenRecord にも SFEN がシードされる
        QCOMPARE(sfenRecord.size(), 1);
        QCOMPARE(sfenRecord.at(0), kHirateSfen);
    }

    void prepareInitialPosition_kifuModelGetsHeader()
    {
        TestHarness h;

        QString startSfen;
        QString currentSfen;
        QStringList sfenRecord;
        KifuRecordListModel kifuModel;

        GameStartCoordinator::Ctx ctx;
        ctx.startSfenStr = &startSfen;
        ctx.currentSfenStr = &currentSfen;
        ctx.sfenRecord = &sfenRecord;
        ctx.kifuModel = &kifuModel;
        ctx.startDlg = nullptr;

        h.gsc->prepareInitialPosition(ctx);

        // 棋譜モデルにヘッダ行が追加されているべき
        QCOMPARE(kifuModel.rowCount(), 1);
    }

    void prepareDataCurrentPosition_setsBaseSfen()
    {
        TestHarness h;

        // 途中局面のSFEN
        const QString midGameSfen =
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");

        QString startSfen = kHirateSfen;
        QString currentSfen = midGameSfen;

        // ShogiView は removeHighlightAllData() を呼ばれるので、スタブが必要
        ShogiView view;

        GameStartCoordinator::Ctx ctx;
        ctx.view = &view;
        ctx.gc = nullptr;
        ctx.startSfenStr = &startSfen;
        ctx.currentSfenStr = &currentSfen;

        h.gsc->prepareDataCurrentPosition(ctx);

        // currentSfenStr が baseSfen に設定されるべき
        QCOMPARE(currentSfen, midGameSfen);
        // clearGameOverState が呼ばれたはず
        QVERIFY(TestTracker::clearGameOverStateCalled);
    }

    // ============================================================
    // H) PlayMode 判定: エッジケース
    // ============================================================

    void playMode_emptySfen()
    {
        // 空のSFEN → フォールバック
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, true, false, QString());
        QCOMPARE(mode, PlayMode::EvenHumanVsEngine);
    }

    void playMode_startpos()
    {
        // "startpos" 文字列 → 手番情報なし → フォールバック
        const PlayMode mode = GameStartCoordinator::determinePlayModeAlignedWithTurn(
            1, false, true, QStringLiteral("startpos"));
        QCOMPARE(mode, PlayMode::EvenEngineVsHuman);
    }

    // ============================================================
    // I) 複数回 start() を呼んでも安全
    // ============================================================

    void start_multipleCallsSafe()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        auto p1 = h.makeHvhParams();
        h.gsc->start(p1);

        auto p2 = h.makeHveParams();
        h.gsc->start(p2);

        auto p3 = h.makeEveParams();
        h.gsc->start(p3);

        QCOMPARE(spy.count(), 3);
    }
};

QTEST_MAIN(TestGameStartFlow)
#include "tst_game_start_flow.moc"
