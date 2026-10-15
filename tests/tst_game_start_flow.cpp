/// @file tst_game_start_flow.cpp
/// @brief 対局開始フローの回帰テスト
///
/// 対局開始ダイアログからの対局者の読み取り、start() バリデーション、
/// 時間制御正規化、シグナル発火をテストする。

#include <QTest>
#include <QSignalSpy>

#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "kifurecordlistmodel.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include <QTemporaryDir>
#include <memory>

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// テスト用ハーネス: GameStartCoordinator + MatchCoordinator のセットアップ
struct TestHarness {
    ShogiClock clock;
    ShogiGameController gc;
    AppActivityState activity;
    EngineLifecycleManager engines{&clock};

    MatchCoordinator* match = nullptr;
    GameStartCoordinator* gsc = nullptr;
    std::unique_ptr<QSignalSpy> prepared;  ///< 対局を準備した回数（盤を開始局面にした）

    TestHarness()
    {
        MatchCoordinator::Deps matchDeps;
        matchDeps.gc = &gc;
        matchDeps.clock = &clock;
        matchDeps.activity = &activity;
        matchDeps.engines = &engines;
        match = new MatchCoordinator(matchDeps);
        prepared = std::make_unique<QSignalSpy>(match, &MatchCoordinator::gamePrepared);
        GameStartCoordinator::Deps gscDeps;
        gscDeps.match = match;
        gsc = new GameStartCoordinator(gscDeps);
    }

    ~TestHarness()
    {
        delete gsc;
        prepared.reset();
        delete match;
        engines.destroyEngines();
    }

    int initializedCount() const { return static_cast<int>(prepared->count()); }

    /// HvH平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeHvhParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.sfenStart = kHirateSfen;
        p.autoStartEngineMove = false;
        return p;
    }

    /// HvE平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeHveParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.players.whiteIsEngine = true;
        p.opt.sfenStart = kHirateSfen;
        p.opt.enginePath2 = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        p.opt.players.whiteEngineName = QStringLiteral("TestEngine");
        p.autoStartEngineMove = false;
        return p;
    }

    /// EvE平手の最小 StartParams を構築
    GameStartCoordinator::StartParams makeEveParams()
    {
        GameStartCoordinator::StartParams p;
        p.opt.players.blackIsEngine = true;
        p.opt.players.whiteIsEngine = true;
        p.opt.sfenStart = kHirateSfen;
        p.opt.enginePath1 = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        p.opt.enginePath2 = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        p.opt.players.blackEngineName = QStringLiteral("Engine1");
        p.opt.players.whiteEngineName = QStringLiteral("Engine2");
        p.autoStartEngineMove = true;
        return p;
    }
};

/// start() が適用後に通知した時間設定を受け取る。
struct AppliedTimeControl {
    int count = 0;
    GameStartCoordinator::TimeSettings last;
};

static AppliedTimeControl startAndCaptureTimeControl(GameStartCoordinator* gsc,
                                                     const GameStartCoordinator::StartParams& params)
{
    AppliedTimeControl applied;
    QSignalSpy spy(gsc, &GameStartCoordinator::requestApplyTimeControl);
    gsc->start(params);
    applied.count = static_cast<int>(spy.count());
    if (!spy.isEmpty()) {
        applied.last = qvariant_cast<GameStartCoordinator::TimeSettings>(spy.last().at(0));
    }
    return applied;
}

class TestGameStartFlow : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void initializeGameValidatesBeforeCleanup()
    {
        TestHarness h;
        const QString invalid = QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1");
        GameStartCoordinator::Ctx ctx;
        ctx.currentSfen = invalid;
        ctx.dialogData.startingPositionNumber = 0;
        ctx.dialogData.isHuman1 = ctx.dialogData.isHuman2 = true;
        QSignalSpy cleanup(h.gsc, &GameStartCoordinator::requestPreStartCleanup);
        QSignalSpy time(h.gsc, &GameStartCoordinator::requestApplyTimeControl);
        QString error;
        QVERIFY(!h.gsc->initializeGame(ctx, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(cleanup.count(), 0);
        QCOMPARE(time.count(), 0);
        QCOMPARE(h.initializedCount(), 0);
    }
    void initializeGameAppliesCleanupAndClockOnce()
    {
        TestHarness h;
        GameStartCoordinator::Ctx ctx;
        ctx.dialogData.startingPositionNumber = 1;
        ctx.dialogData.isHuman1 = ctx.dialogData.isHuman2 = true;
        ctx.dialogData.basicTimeMinutes1 = 5;
        QSignalSpy cleanup(h.gsc, &GameStartCoordinator::requestPreStartCleanup);
        QSignalSpy time(h.gsc, &GameStartCoordinator::requestApplyTimeControl);
        QString error;
        QVERIFY(h.gsc->initializeGame(ctx, error));
        QCOMPARE(cleanup.count(), 1);
        QCOMPARE(time.count(), 1);
        QCOMPARE(h.initializedCount(), 1);
        QCOMPARE(h.clock.timeControl().black.mainMs, 300000);
        QVERIFY(h.clock.isRunning());
    }
    void timeoutChoiceSurvivesTimeControlNormalization_data()
    {
        QTest::addColumn<bool>("loseOnTimeout");
        QTest::newRow("continue-after-timeout") << false;
        QTest::newRow("lose-on-timeout") << true;
    }

    void timeoutChoiceSurvivesTimeControlNormalization()
    {
        QFETCH(bool, loseOnTimeout);
        StartGameDialogData data;
        data.byoyomiSec1 = 60;
        data.byoyomiSec2 = 120;
        data.isLoseOnTimeout = loseOnTimeout;

        TestHarness h;
        auto p = h.makeHvhParams();
        p.tc = GameStartOptionsBuilder::buildTimeControl(data);
        QVERIFY(p.tc.enabled);
        QCOMPARE(p.tc.loseOnTimeout, loseOnTimeout);
        const AppliedTimeControl applied = startAndCaptureTimeControl(h.gsc, p);
        QCOMPARE(applied.count, 1);
        QCOMPARE(applied.last.loseOnTimeout, loseOnTimeout);
        QCOMPARE(applied.last.rules.black.byoyomiMs, 60000LL);
        QCOMPARE(applied.last.rules.white.byoyomiMs, 120000LL);
    }

    // ============================================================
    // A) 対局者の読み取り: playersFromDialog()
    // ============================================================

    void players_fromDialog_data()
    {
        QTest::addColumn<bool>("human1");
        QTest::addColumn<bool>("engine1");
        QTest::addColumn<bool>("human2");
        QTest::addColumn<bool>("engine2");
        QTest::addColumn<bool>("blackIsEngine");
        QTest::addColumn<bool>("whiteIsEngine");
        QTest::newRow("human-human") << true << false << true << false << false << false;
        QTest::newRow("human-engine") << true << false << false << true << false << true;
        QTest::newRow("engine-human") << false << true << true << false << true << false;
        QTest::newRow("engine-engine") << false << true << false << true << true << true;
        // 両方に印があるときは人間とする
        QTest::newRow("both-marked") << true << true << true << true << false << false;
    }

    void players_fromDialog()
    {
        QFETCH(bool, human1);
        QFETCH(bool, engine1);
        QFETCH(bool, human2);
        QFETCH(bool, engine2);
        QFETCH(bool, blackIsEngine);
        QFETCH(bool, whiteIsEngine);
        StartGameDialogData data;
        data.isHuman1 = human1;
        data.isEngine1 = engine1;
        data.isHuman2 = human2;
        data.isEngine2 = engine2;
        data.humanName1 = QStringLiteral("下手さん");
        data.humanName2 = QStringLiteral("上手さん");
        data.engineList = {StartGameDialogData::Engine{QStringLiteral("/a")},
                           StartGameDialogData::Engine{QStringLiteral("/b")}};
        data.engineNumber1 = 0;
        data.engineNumber2 = 1;
        data.engineName1 = QStringLiteral("A");
        data.engineName2 = QStringLiteral("B");
        const MatchPlayers players = GameStartOptionsBuilder::playersFromDialog(data);
        QCOMPARE(players.blackIsEngine, blackIsEngine);
        QCOMPARE(players.whiteIsEngine, whiteIsEngine);
        QCOMPARE(players.name(Turn::Black), blackIsEngine ? QStringLiteral("A") : QStringLiteral("下手さん"));
        QCOMPARE(players.name(Turn::White), whiteIsEngine ? QStringLiteral("B") : QStringLiteral("上手さん"));
    }

    /// 対局者は手番によらず座席（先手・後手）で決まる。駒落ちで上手（後手）から指す局面でも同じ
    void players_doNotDependOnSideToMove()
    {
        StartGameDialogData data;
        data.isHuman1 = true;
        data.isEngine2 = true;
        const QString handicap = GameStartOptionsBuilder::startingPositionSfen(7);
        const MatchStartOptions opt = GameStartOptionsBuilder::buildStartOptions(handicap, data);
        QCOMPARE(opt.sfenStart, handicap);
        QVERIFY(opt.players.isHuman(Turn::Black));
        QVERIFY(opt.players.isEngine(Turn::White));
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
        p.opt.sfenStart = kHirateSfen;

        gsc.start(p);

        QCOMPARE(spy.count(), 0);
    }

    // ============================================================
    // C) start() 正常系: HvH, HvE, EvE
    // ============================================================

    void start_invalidKings_data()
    {
        QTest::addColumn<QString>("sfen");
        for (const QString& turn : {QStringLiteral("b"), QStringLiteral("w")}) {
            const QStringList boards = {
                QStringLiteral("9/4+r4/9/9/9/9/9/9/9"),
                QStringLiteral("9/9/9/9/9/9/9/9/9"),
                QStringLiteral("4k4/9/9/9/9/9/9/9/9"),
                QStringLiteral("9/9/9/9/9/9/9/9/4K4"),
                QStringLiteral("4K4/9/9/9/9/9/9/9/4K4"),
                QStringLiteral("4k4/9/9/9/9/9/9/9/4k4")
            };
            for (const QString& board : boards) {
                const QString sfen = board + QLatin1Char(' ') + turn + QStringLiteral(" - 1");
                QTest::newRow(qPrintable(sfen)) << sfen;
            }
        }
    }

    void start_invalidKings()
    {
        QFETCH(QString, sfen);
        TestHarness h;
        auto p = h.makeEveParams();
        p.opt.sfenStart = sfen;
        p.tc.enabled = true;
        p.tc.rules.black.mainMs = 300000;
        QSignalSpy started(h.gsc, &GameStartCoordinator::started);
        QSignalSpy timeControl(h.gsc, &GameStartCoordinator::requestApplyTimeControl);

        h.gsc->start(p);

        QCOMPARE(started.count(), 0);
        QCOMPARE(timeControl.count(), 0);
        QVERIFY(!(h.initializedCount() > 0));
        QVERIFY(!h.clock.isRunning());
    }

    void start_validKings_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("default-position") << QString();
        QTest::newRow("startpos") << QStringLiteral("startpos");
        QTest::newRow("kings-only-black") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("kings-only-white") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1");
        for (int preset = 1; preset <= GameStartOptionsBuilder::kStartingPositionCount; ++preset) {
            QTest::newRow(qPrintable(QStringLiteral("preset-%1").arg(preset)))
                << GameStartOptionsBuilder::startingPositionSfen(preset);
        }
    }

    void start_validKings()
    {
        QFETCH(QString, sfen);
        TestHarness h;
        auto p = h.makeHvhParams();
        p.opt.sfenStart = sfen;
        QSignalSpy started(h.gsc, &GameStartCoordinator::started);
        h.gsc->start(p);
        QCOMPARE(started.count(), 1);
        QVERIFY((h.initializedCount() > 0));
    }

    void start_hvh_emitsStartedSignal()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        auto p = h.makeHvhParams();
        h.gsc->start(p);

        QCOMPARE(spy.count(), 1);
        QVERIFY((h.initializedCount() > 0));
        QVERIFY(h.match->players().humanVsHuman());
    }

    void start_hve_configuresCalled()
    {
        TestHarness h;
        QSignalSpy spy(h.gsc, &GameStartCoordinator::started);

        auto p = h.makeHveParams();
        h.gsc->start(p);

        QCOMPARE(spy.count(), 1);
        QVERIFY((h.initializedCount() > 0));
        QVERIFY(h.match->players().isHuman(Turn::Black));
        QVERIFY(h.match->players().isEngine(Turn::White));
    }

    void start_eve_triggersInitialEngineMove()
    {
        TestHarness h;

        auto p = h.makeEveParams();
        h.gsc->start(p);

        QVERIFY((h.initializedCount() > 0));
        QVERIFY(h.engines.engineInSlot(1));
        QSignalSpy ready(h.engines.engineInSlot(1), &Usi::matchMoveReady);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 5000);
    }

    void start_hvh_noInitialEngineMove()
    {
        TestHarness h;

        auto p = h.makeHvhParams();
        p.autoStartEngineMove = false;
        h.gsc->start(p);

        QVERIFY(!h.clock.isRunning());
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
        p.tc.rules.black.mainMs = 600000;      // 10分
        p.tc.rules.white.mainMs = 600000;
        p.tc.rules.black.byoyomiMs = 30000;    // 30秒秒読み
        p.tc.rules.white.byoyomiMs = 30000;
        p.tc.rules.black.incrementMs = 10000;  // 10秒加算（秒読みがあるので無視されるべき）
        p.tc.rules.white.incrementMs = 10000;

        const AppliedTimeControl applied = startAndCaptureTimeControl(h.gsc, p);

        QCOMPARE(applied.count, 1);
        QVERIFY2(applied.last.rules.usesByoyomi(), "byoyomi should be used when both specified");
        QCOMPARE(applied.last.rules.black.byoyomiMs, 30000LL);
        QCOMPARE(applied.last.rules.white.byoyomiMs, 30000LL);
        // increment は 0 にリセットされるべき
        QCOMPARE(applied.last.rules.black.incrementMs, 0LL);
        QCOMPARE(applied.last.rules.white.incrementMs, 0LL);
    }

    void start_incrementOnlyWhenNoByoyomi()
    {
        // 秒読みなし、加算ありの場合
        TestHarness h;

        auto p = h.makeHvhParams();
        p.tc.enabled = true;
        p.tc.rules.black.mainMs = 600000;
        p.tc.rules.white.mainMs = 600000;
        p.tc.rules.black.byoyomiMs = 0;
        p.tc.rules.white.byoyomiMs = 0;
        p.tc.rules.black.incrementMs = 5000;   // 5秒加算
        p.tc.rules.white.incrementMs = 5000;

        const AppliedTimeControl applied = startAndCaptureTimeControl(h.gsc, p);

        QCOMPARE(applied.count, 1);
        QVERIFY2(!applied.last.rules.usesByoyomi(), "byoyomi should not be used");
        QCOMPARE(applied.last.rules.black.incrementMs, 5000LL);
        QCOMPARE(applied.last.rules.white.incrementMs, 5000LL);
    }

    void start_autoEnableWhenValuesPresent()
    {
        // enabled=false だが時間値が設定されている場合、自動で有効化される
        TestHarness h;
        QSignalSpy tcSpy(h.gsc, &GameStartCoordinator::requestApplyTimeControl);

        auto p = h.makeHvhParams();
        p.tc.enabled = false;            // 明示的にfalse
        p.tc.rules.black.mainMs = 3600000;        // 1時間（値あり）
        p.tc.rules.white.mainMs = 3600000;

        h.gsc->start(p);

        // requestApplyTimeControl が発火していること（値があるので有効化）
        QVERIFY(tcSpy.count() > 0);
    }

    void start_noTimeControlWhenAllZero()
    {
        // 全て0でも前局の時間制限を解除するため、一度通知する
        TestHarness h;
        QSignalSpy tcSpy(h.gsc, &GameStartCoordinator::requestApplyTimeControl);

        auto p = h.makeHvhParams();
        p.tc.enabled = false;
        p.tc.rules.black.mainMs = 0;
        p.tc.rules.white.mainMs = 0;
        p.tc.rules.black.byoyomiMs = 0;
        p.tc.rules.white.byoyomiMs = 0;
        p.tc.rules.black.incrementMs = 0;
        p.tc.rules.white.incrementMs = 0;

        h.gsc->start(p);

        QCOMPARE(tcSpy.count(), 1);
    }

    void start_asymmetricTimeControl()
    {
        // 先手と後手で異なる持ち時間設定
        TestHarness h;

        auto p = h.makeHvhParams();
        p.tc.enabled = true;
        p.tc.rules.black.mainMs = 3600000;     // 先手: 1時間
        p.tc.rules.black.byoyomiMs = 60000;    // 先手: 60秒秒読み
        p.tc.rules.white.mainMs = 1800000;     // 後手: 30分
        p.tc.rules.white.byoyomiMs = 30000;    // 後手: 30秒秒読み

        const AppliedTimeControl applied = startAndCaptureTimeControl(h.gsc, p);

        QCOMPARE(applied.count, 1);
        QCOMPARE(applied.last.rules.black.byoyomiMs, 60000LL);
        QCOMPARE(applied.last.rules.white.byoyomiMs, 30000LL);
        QCOMPARE(applied.last.rules.black.mainMs, 3600000LL);
        QCOMPARE(applied.last.rules.white.mainMs, 1800000LL);
    }

    // ============================================================
    // E) prepare() シグナルテスト
    // ============================================================





    // ============================================================
    // F) 連続対局設定（StartOptions プロパティ検証）
    // ============================================================

    void start_maxMovesPassedThrough()
    {
        TestHarness h;

        auto p = h.makeHvhParams();
        p.opt.maxMoves = 256;

        h.gsc->start(p);

        QVERIFY((h.initializedCount() > 0));
        QCOMPARE(h.match->options().maxMoves, 256);
    }



    // ============================================================
    // G) 開始要求の検証と一度だけの適用
    // ============================================================







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
