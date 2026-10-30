/// @file tst_game_start_flow.cpp
/// @brief 対局開始の流れの回帰テスト
///
/// 対局開始ダイアログの値から開始の計画（GameStartOptionsBuilder::plan）を作り、
/// 時計に持ち時間を設定して対局の司令塔（MatchCoordinator）で始める流れを確かめる。
/// 本体では GameSession::startGame がこの順に1つの関数で行う。

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>

#include "appactivitystate.h"
#include "engineslots.h"
#include "gamestartoptionsbuilder.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "gameboard.h"
#include "usi.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 対局の司令塔と、その部品（盤・時計・動作状態・エンジンの枠）
struct TestHarness {
    ShogiClock clock;
    GameBoard gc;
    AppActivityState activity;
    EngineSlots engines;
    std::unique_ptr<MatchCoordinator> match;

    TestHarness()
    {
        MatchCoordinator::Deps matchDeps;
        matchDeps.gameBoard = &gc;
        matchDeps.clock = &clock;
        matchDeps.activity = &activity;
        matchDeps.engines = &engines;
        match = std::make_unique<MatchCoordinator>(matchDeps);
    }

    ~TestHarness()
    {
        match.reset();
        engines.destroyEngines();
    }

    /// 本体（GameSession::startGame）と同じく対局を始める（時計は司令塔が options.time で設定する）
    void start(const MatchStartOptions& options, bool beginPlay = true)
    {
        match->start(options);
        if (beginPlay) match->beginPlay();
    }

    /// 時間の決まりを変えて start する
    void start(MatchStartOptions options, const MatchTimeSettings& time)
    {
        options.time = time;
        start(options);
    }

    static MatchStartOptions hvh()
    {
        MatchStartOptions opt;
        opt.recordLine.startSfen = kHirateSfen;
        return opt;
    }

    static MatchStartOptions hve()
    {
        MatchStartOptions opt = hvh();
        opt.players.seat(Turn::White).engine = true;
        opt.engine(Turn::White).path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        opt.players.seat(Turn::White).engineName = QStringLiteral("TestEngine");
        return opt;
    }

    static MatchStartOptions eve()
    {
        MatchStartOptions opt = hvh();
        opt.players.seat(Turn::Black).engine = true;
        opt.players.seat(Turn::White).engine = true;
        opt.engine(Turn::Black).path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        opt.engine(Turn::White).path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        opt.players.seat(Turn::Black).engineName = QStringLiteral("Engine1");
        opt.players.seat(Turn::White).engineName = QStringLiteral("Engine2");
        return opt;
    }
};

/// 人間どうしの対局開始ダイアログの値（手合割の表 Handicaps::kAll の添字。-1 は現在の局面）
static StartGameDialogData humanDialog(int handicapIndex)
{
    StartGameDialogData data;
    data.startingHandicap = handicapIndex < 0 ? nullptr : &Handicaps::kAll[handicapIndex];
    return data;
}

/// 秒（分・時間）からミリ秒
static constexpr qint64 seconds(qint64 value) { return value * 1000; }
static constexpr qint64 minutes(qint64 value) { return seconds(value * 60); }

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

    // ============================================================
    // A) 開始の計画
    // ============================================================

    /// 指せない局面（玉が無い）からは計画を作らず、理由を返す（棋譜も時計も変えない）
    void plan_rejectsInvalidPositionBeforeAnyChange()
    {
        GameStartRequest request;
        request.dialogData = humanDialog(-1);
        request.recordLine.startSfen = QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1");
        QString error;
        QVERIFY(!GameStartOptionsBuilder::plan(request, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    /// 手合割を選んだ対局は新しい棋譜、現在局面からの対局は表示中の手から始める
    void plan_choosesStartPosition()
    {
        GameStartRequest request;
        request.dialogData = humanDialog(0);
        request.dialogData.players[0].time.mainMs = minutes(5);
        request.startNodeId = 42;
        QString error;
        std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
        QVERIFY2(plan.has_value(), qPrintable(error));
        QCOMPARE(plan->preparation.mode, GameStartPreparation::Mode::NewGame);
        QCOMPARE(plan->preparation.startNodeId, -1);
        QCOMPARE(plan->options.recordLine.lastSfen(), kHirateSfen);
        QCOMPARE(plan->options.time.rules.black.mainMs, 300000LL);

        request.dialogData.startingHandicap = nullptr;
        request.recordLine.startSfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1");
        plan = GameStartOptionsBuilder::plan(request, &error);
        QVERIFY2(plan.has_value(), qPrintable(error));
        QCOMPARE(plan->preparation.mode, GameStartPreparation::Mode::CurrentPosition);
        QCOMPARE(plan->preparation.startNodeId, 42);
        QCOMPARE(plan->options.recordLine.lastSfen(), request.recordLine.startSfen);
    }

    /// 計画どおりに時計を1回設定してから対局を始める
    void startAppliesClockOnce()
    {
        TestHarness h;
        GameStartRequest request;
        request.dialogData = humanDialog(0);
        request.dialogData.players[0].time.mainMs = minutes(5);
        QString error;
        const std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
        QVERIFY2(plan.has_value(), qPrintable(error));
        h.start(plan->options);
        QCOMPARE(h.clock.timeControl().black.mainMs, 300000);
        QVERIFY(h.clock.isRunning());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringGame);
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
        data.players[0].time.byoyomiMs = seconds(60);
        data.players[1].time.byoyomiMs = seconds(120);
        data.isLoseOnTimeout = loseOnTimeout;

        const MatchTimeSettings time = GameStartOptionsBuilder::buildTimeSettings(data);
        QVERIFY(!time.rules.isUnlimited());
        QCOMPARE(time.loseOnTimeout, loseOnTimeout);
        QCOMPARE(time.rules.black.byoyomiMs, 60000LL);
        QCOMPARE(time.rules.white.byoyomiMs, 120000LL);

        TestHarness h;
        h.start(TestHarness::hvh(), time);
        QCOMPARE(h.clock.timeControl().white.byoyomiMs, 120000LL);
    }

    // ============================================================
    // B) 対局者の読み取り: playersFromDialog()
    // ============================================================

    void players_fromDialog_data()
    {
        QTest::addColumn<bool>("blackIsEngine");
        QTest::addColumn<bool>("whiteIsEngine");
        QTest::newRow("human-human") << false << false;
        QTest::newRow("human-engine") << false << true;
        QTest::newRow("engine-human") << true << false;
        QTest::newRow("engine-engine") << true << true;
    }

    void players_fromDialog()
    {
        QFETCH(bool, blackIsEngine);
        QFETCH(bool, whiteIsEngine);
        StartGameDialogData data;
        data.players[0].kind = blackIsEngine ? PlayerChoice::Kind::Engine : PlayerChoice::Kind::Human;
        data.players[1].kind = whiteIsEngine ? PlayerChoice::Kind::Engine : PlayerChoice::Kind::Human;
        data.players[0].humanName = QStringLiteral("下手さん");
        data.players[1].humanName = QStringLiteral("上手さん");
        data.players[0].engine = {QStringLiteral("A"), QStringLiteral("/a"), QString()};
        data.players[1].engine = {QStringLiteral("B"), QStringLiteral("/b"), QString()};
        const MatchPlayers players = GameStartOptionsBuilder::playersFromDialog(data);
        QCOMPARE(players.seat(Turn::Black).engine, blackIsEngine);
        QCOMPARE(players.seat(Turn::White).engine, whiteIsEngine);
        QCOMPARE(players.name(Turn::Black), blackIsEngine ? QStringLiteral("A") : QStringLiteral("下手さん"));
        QCOMPARE(players.name(Turn::White), whiteIsEngine ? QStringLiteral("B") : QStringLiteral("上手さん"));
    }

    /// 対局者は手番によらず座席（先手・後手）で決まる。駒落ちで上手（後手）から指す局面でも同じ
    void players_doNotDependOnSideToMove()
    {
        GameStartRequest request;
        request.dialogData.players[1].kind = PlayerChoice::Kind::Engine;
        request.dialogData.startingHandicap = Handicaps::byLabel(u"二枚落ち");
        QString error;
        const std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
        QVERIFY2(plan.has_value(), qPrintable(error));
        const MatchStartOptions& opt = plan->options;
        QCOMPARE(opt.recordLine.lastSfen(), QString::fromLatin1(request.dialogData.startingHandicap->sfen));
        QVERIFY(opt.players.isHuman(Turn::Black));
        QVERIFY(opt.players.isEngine(Turn::White));
    }

    // ============================================================
    // C) 局面の検査
    // ============================================================

    void plan_invalidKings_data()
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
        // 玉が1枚ずつでも、手番でない側の玉に王手がかかっている局面・駒の数が上限を超える局面は始められない
        QTest::newRow("opponent-in-check") << QStringLiteral("4k4/4G4/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("too-many-pawns") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b 19P 1");
    }

    void plan_invalidKings()
    {
        QFETCH(QString, sfen);
        QString why;
        QVERIFY(!GameStartOptionsBuilder::validatePosition(sfen, &why));
        QVERIFY(!why.isEmpty());
        GameStartRequest request;
        request.dialogData = humanDialog(-1);
        request.recordLine.startSfen = sfen;
        QString error;
        QVERIFY(!GameStartOptionsBuilder::plan(request, &error).has_value());
    }

    void plan_validKings_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("default-position") << QString();
        QTest::newRow("startpos") << QStringLiteral("startpos");
        QTest::newRow("kings-only-black") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("kings-only-white") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1");
        for (const Handicaps::Handicap& handicap : Handicaps::kAll) {
            QTest::newRow(handicap.jkfPreset) << QString::fromLatin1(handicap.sfen);
        }
    }

    void plan_validKings()
    {
        QFETCH(QString, sfen);
        GameStartRequest request;
        request.dialogData = humanDialog(-1);
        request.recordLine.startSfen = sfen;
        QString error;
        const std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
        QVERIFY2(plan.has_value(), qPrintable(error));
        TestHarness h;
        h.start(plan->options);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringGame);
        QCOMPARE(h.match->record().startSfen, GameStartOptionsBuilder::canonicalizeSfen(sfen));
    }

    // ============================================================
    // D) 対局者の組ごとの開始
    // ============================================================

    void start_hvh()
    {
        TestHarness h;
        h.start(TestHarness::hvh());
        QVERIFY(h.match->players().humanVsHuman());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringGame);
    }

    void start_hve()
    {
        TestHarness h;
        h.start(TestHarness::hve());
        QVERIFY(h.match->players().isHuman(Turn::Black));
        QVERIFY(h.match->players().isEngine(Turn::White));
    }

    void start_eve_triggersInitialEngineMove()
    {
        TestHarness h;
        h.start(TestHarness::eve());
        QVERIFY(h.engines.engineInSlot(1));
        QSignalSpy ready(h.engines.engineInSlot(1), &Usi::matchMoveReady);
        QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(), 5000);
    }

    /// 時計は準備の後、指し始めるときに動かす
    void clockStartsWithPlay()
    {
        TestHarness h;
        h.start(TestHarness::hvh(), /*beginPlay=*/false);
        QVERIFY(!h.clock.isRunning());
        h.match->beginPlay();
        QVERIFY(h.clock.isRunning());
    }

    // ============================================================
    // E) 持ち時間の正規化
    // ============================================================

    /// 秒読みと加算が同時に指定された場合、秒読みが優先される
    void time_byoyomiPriorityOverIncrement()
    {
        StartGameDialogData data;
        for (PlayerChoice& player : data.players) {
            player.time.mainMs = minutes(10);
            player.time.byoyomiMs = seconds(30);
            player.time.incrementMs = seconds(10);
        }
        const MatchTimeSettings time = GameStartOptionsBuilder::buildTimeSettings(data);
        QVERIFY2(time.rules.usesByoyomi(), "byoyomi should be used when both specified");
        QCOMPARE(time.rules.black.byoyomiMs, 30000LL);
        QCOMPARE(time.rules.white.byoyomiMs, 30000LL);
        QCOMPARE(time.rules.black.incrementMs, 0LL);
        QCOMPARE(time.rules.white.incrementMs, 0LL);
    }

    void time_incrementOnlyWhenNoByoyomi()
    {
        StartGameDialogData data;
        for (PlayerChoice& player : data.players) {
            player.time.mainMs = minutes(10);
            player.time.incrementMs = seconds(5);
        }
        const MatchTimeSettings time = GameStartOptionsBuilder::buildTimeSettings(data);
        QVERIFY2(!time.rules.usesByoyomi(), "byoyomi should not be used");
        QCOMPARE(time.rules.black.incrementMs, 5000LL);
        QCOMPARE(time.rules.white.incrementMs, 5000LL);
    }

    /// 値が1つでもあれば持ち時間のある対局。全部 0 なら消費時間だけを数える（前局の持ち時間は残さない）
    void time_unlimitedOnlyWhenAllZero()
    {
        StartGameDialogData data;
        data.players[0].time.mainMs = data.players[1].time.mainMs = minutes(60);
        QVERIFY(!GameStartOptionsBuilder::buildTimeSettings(data).rules.isUnlimited());

        TestHarness h;
        h.start(TestHarness::hvh(), GameStartOptionsBuilder::buildTimeSettings(data));
        QVERIFY(!h.clock.isUnlimited());
        h.match->handleBreakOff();
        h.start(TestHarness::hvh(), GameStartOptionsBuilder::buildTimeSettings(StartGameDialogData{}));
        QVERIFY(h.clock.isUnlimited());
    }

    void time_asymmetric()
    {
        StartGameDialogData data;
        data.players[0].time.mainMs = minutes(60);
        data.players[0].time.byoyomiMs = seconds(60);
        data.players[1].time.mainMs = minutes(30);
        data.players[1].time.byoyomiMs = seconds(30);
        const MatchTimeSettings time = GameStartOptionsBuilder::buildTimeSettings(data);
        QCOMPARE(time.rules.black.byoyomiMs, 60000LL);
        QCOMPARE(time.rules.white.byoyomiMs, 30000LL);
        QCOMPARE(time.rules.black.mainMs, 3600000LL);
        QCOMPARE(time.rules.white.mainMs, 1800000LL);
    }

    // ============================================================
    // F) 開始の設定の受け渡し
    // ============================================================

    void start_maxMovesPassedThrough()
    {
        TestHarness h;
        MatchStartOptions opt = TestHarness::hvh();
        opt.maxMoves = 256;
        h.start(opt);
        QCOMPARE(h.match->options().maxMoves, 256);
    }

    /// 続けて別の組で始め直しても、そのたびに対局者が入れ替わる
    void start_multipleCallsSafe()
    {
        TestHarness h;
        h.start(TestHarness::hvh());
        QVERIFY(h.match->players().humanVsHuman());
        h.start(TestHarness::hve());
        QVERIFY(h.match->players().isEngine(Turn::White));
        h.start(TestHarness::eve());
        QVERIFY(h.match->players().engineVsEngine());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringGame);
    }
};

QTEST_MAIN(TestGameStartFlow)
#include "tst_game_start_flow.moc"
