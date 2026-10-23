/// @file tst_matchcoordinator.cpp
/// @brief MatchCoordinator 単体テスト
///
/// 対局の司令塔の主要フロー（開始・着手の記録・終局の記録と知らせ・待った・エンジンの投了と異常）を、
/// 本物の盤・時計・エンジンの枠と、棋譜の行を覚える受け手で検証する。

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "match_test_helper.h"
#include "terminaltype.h"
#include "usi.h"
#include "test_replay.h"

class Tst_MatchCoordinator : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    // === 初期状態 ===
    void initialState_notStartedAndNotOver();

    // === 開始 ===
    void start_preparesHumanGame();
    void start_keepsRecordLineUpToCurrentPly();

    // === 着手 ===
    void humanMove_isRecordedAndAnnounced();
    void humanMove_rejectedWhenSinkIsClosed();

    // === 終局の記録 ===
    void resign_recordsTerminalAndAnnounces();
    void resign_secondCallIgnored();
    void clearGameOverState_clears();
    void breakOff_appendsTerminalOnceWithoutMessage();
    void playerTimeOut_recordsTimeout();
    void nyugyokuDeclaration_success_data();
    void nyugyokuDeclaration_success();

    // === 待った ===
    void undoTwoPlies_restoresBoardAndRecord();
    void undoTwoPlies_needsTwoMoves();
    void undoTwoPlies_onlyOnHumanTurn();

    // === エンジン ===
    void humanVsEngine_engineResignLosesEngineSide_data();
    void humanVsEngine_engineResignLosesEngineSide();
    void humanVsEngine_engineWinIsJudgedOnBoard();
    void humanVsEngine_engineStartFailure_breaksOffAfterStart();
    void engineVsEngine_engineStartFailure_breaksOffOnce();
    void humanVsEngine_engineExit_breaksOff();
};

namespace {

/// テスト中だけ環境変数を設定する（失敗で抜けても戻す）
struct ScopedEnv {
    const char* name;
    ScopedEnv(const char* n, const QByteArray& value) : name(n) { qputenv(name, value); }
    ~ScopedEnv() { qunsetenv(name); }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;
};

MatchCoordinator::StartOptions humanVsEngine(bool engineIsBlack, const QString& path, const QString& name)
{
    MatchCoordinator::StartOptions opt;
    opt.players.seat(Turn::Black).engine = engineIsBlack;
    opt.players.seat(Turn::White).engine = !engineIsBlack;
    opt.sfenStart = SfenUtils::hirateSfen();
    (engineIsBlack ? opt.engine(Turn::Black).path : opt.engine(Turn::White).path) = path;
    (engineIsBlack ? opt.players.seat(Turn::Black).engineName : opt.players.seat(Turn::White).engineName) = name;
    return opt;
}

} // namespace

// ============================================================
// 初期状態
// ============================================================

void Tst_MatchCoordinator::initialState_notStartedAndNotOver()
{
    MatchHarness h;
    QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    QVERIFY(!h.mc->isGameOver());
    QVERIFY(!h.mc->gameEnd());
    QCOMPARE(h.mc->currentPly(), 0);
    QVERIFY(h.sink.records.isEmpty());
}

// ============================================================
// 開始
// ============================================================

void Tst_MatchCoordinator::start_preparesHumanGame()
{
    MatchHarness h;
    h.startHumanVsHuman();
    QVERIFY(h.mc->players().humanVsHuman());
    QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringGame);
    QCOMPARE(h.gc.currentPlayer(), Turn::Black);
    QCOMPARE(h.mc->record().startSfen, SfenUtils::hirateSfen());
    QVERIFY(h.mc->isHumanSide(Turn::Black));
    QVERIFY(h.mc->isHumanSide(Turn::White));
    QVERIFY(!h.mc->isGameOver());
}

void Tst_MatchCoordinator::start_keepsRecordLineUpToCurrentPly()
{
    MatchHarness h;
    MatchCoordinator::StartOptions opt;
    opt.recordLine = TestReplay::recordLine(
        TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), {"7g7f", "3c3d", "2g2f"}, false),
        TestReplay::buildGameMoves(SfenUtils::hirateSfen(), {"7g7f", "3c3d", "2g2f"}));
    opt.recordLine.currentPly = 2;
    h.start(opt);
    // 表示中の手（2手目）までを対局の手順にし、後続の手は捨てる
    QCOMPARE(h.mc->currentPly(), 2);
    QCOMPARE(h.mc->options().sfenStart, h.mc->record().lastSfen());
    QCOMPARE(h.gc.currentPlayer(), Turn::Black);
    QCOMPARE(h.gc.board()->toSfen(), h.mc->record().lastSfen());
}

// ============================================================
// 着手
// ============================================================

void Tst_MatchCoordinator::humanMove_isRecordedAndAnnounced()
{
    MatchHarness h;
    h.startHumanVsHuman();
    // 手番の正本は盤。盤が手番の変化を1回だけ知らせる
    QSignalSpy turn(h.gc.board(), &ShogiBoard::turnChanged);
    QSignalSpy committed(&h.gc, &ShogiGameController::moveCommitted);

    QVERIFY(h.playHuman(QStringLiteral("7g7f")));

    QCOMPARE(h.mc->currentPly(), 1);
    QCOMPARE(h.mc->record().usiMove(1).toString(), QStringLiteral("7g7f"));
    QCOMPARE(h.sink.records.size(), 1);
    QCOMPARE(h.sink.records.first().displayText, QStringLiteral("▲７六歩(77)"));
    QCOMPARE(h.sink.records.first().mover, Turn::Black);
    QCOMPARE(h.sink.records.first().move.move.to, (BoardSquare{7, 6}));
    QCOMPARE(turn.count(), 1);
    QCOMPARE(turn.first().first().value<Turn>(), Turn::White);
    QCOMPARE(h.gc.currentPlayer(), Turn::White);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.first().at(1).toInt(), 1);
}

void Tst_MatchCoordinator::humanMove_rejectedWhenSinkIsClosed()
{
    MatchHarness h;
    h.startHumanVsHuman();
    h.sink.acceptRecords = false;
    // 棋譜に書けない（ライブ対局のセッションが終わった）ときは手を受け付けない
    QVERIFY(!h.mc->canAcceptMove());
}

// ============================================================
// 終局の記録
// ============================================================

void Tst_MatchCoordinator::resign_recordsTerminalAndAnnounces()
{
    MatchHarness h;
    h.startHumanVsHuman();
    QSignalSpy ended(h.mc.get(), &MatchCoordinator::gameEnded);
    QSignalSpy processed(h.mc.get(), &MatchCoordinator::gameEndProcessed);

    h.mc->handleResign();

    QVERIFY(h.mc->isGameOver());
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.mc->gameEnd()->loser, Turn::Black);
    // 終局の行を棋譜に書いた
    QCOMPARE(h.sink.records.last().terminal, TerminalType::Resign);
    // 終局したら動作状態は待機に戻る（終局を見る部品はこの変化で知る）
    QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    QCOMPARE(ended.count(), 1);
    const auto info = ended.first().first().value<MatchCoordinator::GameEndInfo>();
    QCOMPARE(info.cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(info.loser, Turn::Black);
    QCOMPARE(h.sink.records.size(), 1);
    QCOMPARE(h.sink.records.first().terminal, TerminalType::Resign);
    QCOMPARE(h.sink.records.first().mover, Turn::Black);
    QCOMPARE(h.messageCount(), 1);
    QCOMPARE(h.lastMessageTitle(), QStringLiteral("対局終了"));
    QVERIFY(h.lastMessageText().contains(QStringLiteral("先手の投了")));
    QCOMPARE(processed.count(), 1);
}

void Tst_MatchCoordinator::resign_secondCallIgnored()
{
    MatchHarness h;
    h.startHumanVsHuman();
    h.mc->handleResign();
    QSignalSpy ended(h.mc.get(), &MatchCoordinator::gameEnded);
    h.mc->handleResign();
    h.mc->handleBreakOff();
    QCOMPARE(ended.count(), 0);
    QCOMPARE(h.sink.records.size(), 1);
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Resignation);
}

void Tst_MatchCoordinator::clearGameOverState_clears()
{
    MatchHarness h;
    h.mc->clearGameEnd();   // 対局していなくても何も起きない
    h.startHumanVsHuman();
    h.mc->handleResign();

    h.mc->clearGameEnd();

    QVERIFY(!h.mc->isGameOver());
    QVERIFY(!h.mc->gameEnd().has_value());
}

void Tst_MatchCoordinator::breakOff_appendsTerminalOnceWithoutMessage()
{
    MatchHarness h;
    h.startHumanVsHuman();
    QVERIFY(h.playHuman(QStringLiteral("7g7f")));
    QSignalSpy interrupted(h.mc.get(), &MatchCoordinator::gameInterrupted);

    h.mc->handleBreakOff();

    QVERIFY(h.mc->isGameOver());
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::BreakOff);
    QCOMPARE(h.sink.records.size(), 2);
    QCOMPARE(h.sink.records.last().terminal, TerminalType::Interrupt);
    // 中断は知らせない。再開できる
    QCOMPARE(h.messageCount(), 0);
    QCOMPARE(interrupted.count(), 1);
    QVERIFY(h.mc->interruptedGame());
    QCOMPARE(h.mc->interruptedGame()->record.moveCount(), 1);
}

void Tst_MatchCoordinator::playerTimeOut_recordsTimeout()
{
    MatchHarness h;
    h.startHumanVsHuman();
    h.mc->handlePlayerTimeOut(Turn::White);
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Timeout);
    QCOMPARE(h.mc->gameEnd()->loser, Turn::White);
    QCOMPARE(h.sink.records.last().terminal, TerminalType::Timeout);
    QCOMPARE(h.sink.records.last().mover, Turn::White);
    QVERIFY(h.lastMessageText().contains(QStringLiteral("後手の時間切れ")));
}

void Tst_MatchCoordinator::nyugyokuDeclaration_success_data()
{
    QTest::addColumn<bool>("success");
    QTest::addColumn<bool>("isDraw");
    QTest::addColumn<int>("cause");
    QTest::addColumn<int>("loser");
    QTest::addColumn<int>("terminal");
    QTest::newRow("win") << true << false << int(MatchCause::NyugyokuWin) << int(Turn::White)
                         << int(TerminalType::DeclarationWin);
    QTest::newRow("draw") << false << true << int(MatchCause::Jishogi) << int(Turn::Black)
                          << int(TerminalType::Impasse);
    QTest::newRow("fail") << false << false << int(MatchCause::IllegalMove) << int(Turn::Black)
                          << int(TerminalType::IllegalLoss);
}

void Tst_MatchCoordinator::nyugyokuDeclaration_success()
{
    QFETCH(bool, success);
    QFETCH(bool, isDraw);
    QFETCH(int, cause);
    QFETCH(int, loser);
    QFETCH(int, terminal);
    MatchHarness h;
    h.startHumanVsHuman();
    QSignalSpy processed(h.mc.get(), &MatchCoordinator::gameEndProcessed);
    h.mc->handleNyugyokuDeclaration(Turn::Black, success, isDraw);
    QCOMPARE(int(h.mc->gameEnd()->cause), cause);
    QCOMPARE(int(h.mc->gameEnd()->loser), loser);
    QCOMPARE(int(h.sink.records.last().terminal), terminal);
    // 宣言の結果は呼び出し側のダイアログで知らせるので、ここでは知らせない
    QCOMPARE(h.messageCount(), 0);
    QCOMPARE(processed.count(), 1);
}

// ============================================================
// 待った
// ============================================================

void Tst_MatchCoordinator::undoTwoPlies_restoresBoardAndRecord()
{
    MatchHarness h;
    h.startHumanVsHuman();
    QVERIFY(h.playHuman(QStringLiteral("7g7f")));
    QVERIFY(h.playHuman(QStringLiteral("3c3d")));
    QVERIFY(h.playHuman(QStringLiteral("2g2f")));
    QSignalSpy undone(h.mc.get(), &MatchCoordinator::movesUndone);

    QVERIFY(h.mc->undoTwoPlies());

    QCOMPARE(h.mc->currentPly(), 1);
    QCOMPARE(h.sink.records.size(), 1);
    QCOMPARE(undone.count(), 1);
    QCOMPARE(undone.first().first().toInt(), 1);
    QCOMPARE(h.gc.currentPlayer(), Turn::White);
    QCOMPARE(h.gc.board()->toSfen(), h.mc->record().lastSfen());
    // 次の手の「同」表記は、戻した局面に至った手（７六歩）の移動先を基準にする
    QCOMPARE(h.mc->lastMoveDestination(), (BoardSquare{7, 6}));
}

void Tst_MatchCoordinator::undoTwoPlies_needsTwoMoves()
{
    MatchHarness h;
    h.startHumanVsHuman();
    QVERIFY(h.playHuman(QStringLiteral("7g7f")));
    QVERIFY(!h.mc->undoTwoPlies());
    QCOMPARE(h.mc->currentPly(), 1);
    QCOMPARE(h.sink.records.size(), 1);
}

void Tst_MatchCoordinator::undoTwoPlies_onlyOnHumanTurn()
{
    MatchHarness h;
    MatchCoordinator::StartOptions opt = humanVsEngine(false, MatchHarness::mockEnginePath(), QStringLiteral("UndoTest"));
    const QStringList opening{QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    opt.recordLine = TestReplay::recordLine(TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), opening, false),
                                               TestReplay::buildGameMoves(SfenUtils::hirateSfen(), opening));
    opt.recordLine.currentPly = 2;
    h.mc->start(opt);
    QVERIFY(h.playHuman(QStringLiteral("2g2f")));
    // エンジン（後手）の手番では待ったできない
    QVERIFY(!h.mc->undoTwoPlies());
    QCOMPARE(h.mc->currentPly(), 3);
    QVERIFY(h.mc->strategyHost().applyEngineMove(engineMove(QStringLiteral("8c8d"), Turn::White)));
    QVERIFY(h.mc->undoTwoPlies());
    QCOMPARE(h.mc->currentPly(), 2);
    QVERIFY(h.sink.records.isEmpty());
}

// ============================================================
// エンジン
// ============================================================

void Tst_MatchCoordinator::humanVsEngine_engineResignLosesEngineSide_data()
{
    QTest::addColumn<bool>("engineIsBlack");
    QTest::newRow("engine-black") << true;
    QTest::newRow("engine-white") << false;
}

void Tst_MatchCoordinator::humanVsEngine_engineResignLosesEngineSide()
{
    // 人間対エンジンでは、エンジンが後手でもエンジンの枠は E1 を使う。
    // そのエンジンの投了は、枠ではなくエンジンの手番の負けになる
    QFETCH(bool, engineIsBlack);
    MatchHarness h;
    h.mc->start(humanVsEngine(engineIsBlack, MatchHarness::mockEnginePath(), QStringLiteral("ResignSideTest")));
    Usi* engine = h.engines.engineInSlot(1);
    QVERIFY(engine);
    QCOMPARE(h.engines.engineInSlot(2), nullptr);
    QSignalSpy ready(engine, &Usi::engineInitialized);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);

    emit engine->resigned(engineIsBlack ? Turn::Black : Turn::White);

    QVERIFY(h.mc->isGameOver());
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::Resignation);
    QCOMPARE(h.mc->gameEnd()->loser, engineIsBlack ? Turn::Black : Turn::White);
}

void Tst_MatchCoordinator::humanVsEngine_engineWinIsJudgedOnBoard()
{
    MatchHarness h;
    h.mc->start(humanVsEngine(true, MatchHarness::mockEnginePath(), QStringLiteral("WinTest")));
    Usi* engine = h.engines.engineInSlot(1);
    QVERIFY(engine);
    QSignalSpy ready(engine, &Usi::engineInitialized);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);

    // エンジンの入玉宣言（bestmove win）も盤面で判定する。初期局面では条件を満たさず宣言側の負け
    emit engine->declaredWin(Turn::Black);
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::IllegalMove);
    QCOMPARE(h.mc->gameEnd()->loser, Turn::Black);
    QCOMPARE(h.messageCount(), 1);

    // 先手玉２二、敵陣に15枚・持ち駒と合わせて28点（27点法で宣言勝ち）
    MatchHarness h2;
    MatchCoordinator::StartOptions opt = humanVsEngine(true, MatchHarness::mockEnginePath(), QStringLiteral("WinTest"));
    opt.sfenStart = QStringLiteral(
        "5+B+RGS/6GKS/+P+P+P+P+P+P+P+P+P/9/p1p1p1p1p/1s1g1g1s1/2n3n1n/1r2k4/2b6 b NL3P3lp 1");
    h2.mc->start(opt);
    Usi* engine2 = h2.engines.engineInSlot(1);
    QVERIFY(engine2);
    QSignalSpy ready2(engine2, &Usi::engineInitialized);
    QTRY_COMPARE_WITH_TIMEOUT(ready2.count(), 1, 5000);
    emit engine2->declaredWin(Turn::Black);
    QCOMPARE(h2.mc->gameEnd()->cause, MatchCoordinator::Cause::NyugyokuWin);
    QCOMPARE(h2.mc->gameEnd()->loser, Turn::White);
    QCOMPARE(h2.sink.records.last().terminal, TerminalType::DeclarationWin);
}

void Tst_MatchCoordinator::humanVsEngine_engineStartFailure_breaksOffAfterStart()
{
    MatchHarness h;
    const MatchCoordinator::StartOptions opt =
        humanVsEngine(false, m_config.filePath(QStringLiteral("missing-engine")), QStringLiteral("MissingEngine"));
    h.mc->start(opt);

    // 起動の失敗は開始処理の中で通知されるが、中断は開始処理を終えてから行う
    QCOMPARE(h.messageCount(), 0);
    QVERIFY(!h.mc->isGameOver());

    QTRY_VERIFY_WITH_TIMEOUT(h.messageCount() > 0, 1000);
    QCOMPARE(h.lastMessageTitle(), QStringLiteral("対局中断"));
    QVERIFY2(h.lastMessageText().contains(opt.engine(Turn::White).path), qPrintable(h.lastMessageText()));
    QVERIFY(h.mc->isGameOver());
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::BreakOff);
}

void Tst_MatchCoordinator::engineVsEngine_engineStartFailure_breaksOffOnce()
{
    MatchHarness h;
    MatchCoordinator::StartOptions opt;
    opt.players.seat(Turn::Black).engine = true;
    opt.players.seat(Turn::White).engine = true;
    opt.sfenStart = SfenUtils::hirateSfen();
    opt.players.seat(Turn::Black).engineName = QStringLiteral("MissingEngine1");
    opt.engine(Turn::Black).path = m_config.filePath(QStringLiteral("missing-engine-1"));
    opt.players.seat(Turn::White).engineName = QStringLiteral("MissingEngine2");
    opt.engine(Turn::White).path = m_config.filePath(QStringLiteral("missing-engine-2"));
    h.mc->start(opt);

    QTRY_VERIFY_WITH_TIMEOUT(h.messageCount() > 0, 1000);
    QCoreApplication::processEvents();
    // 2つ目のエンジンのエラーは、中断した後なので知らせない
    QCOMPARE(h.messageCount(), 1);
    QCOMPARE(h.lastMessageTitle(), QStringLiteral("対局中断"));
    QVERIFY2(h.lastMessageText().contains(opt.engine(Turn::Black).path), qPrintable(h.lastMessageText()));
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::BreakOff);
}

void Tst_MatchCoordinator::humanVsEngine_engineExit_breaksOff()
{
    const ScopedEnv exitOnGo("SBQ_MATCH_EXIT_ON_GO", "1");
    MatchHarness h;
    h.start(humanVsEngine(true, MatchHarness::mockEnginePath(), QStringLiteral("ExitingEngine")));

    // 起動は成功し、初手の go でエンジンが終了する
    QTRY_VERIFY_WITH_TIMEOUT(h.messageCount() > 0, 5000);
    QCOMPARE(h.lastMessageTitle(), QStringLiteral("対局中断"));
    QVERIFY2(h.lastMessageText().contains(QStringLiteral("Engine exited unexpectedly.")),
             qPrintable(h.lastMessageText()));
    QCOMPARE(h.mc->gameEnd()->cause, MatchCoordinator::Cause::BreakOff);
}

// ============================================================
QTEST_MAIN(Tst_MatchCoordinator)
#include "tst_matchcoordinator.moc"
