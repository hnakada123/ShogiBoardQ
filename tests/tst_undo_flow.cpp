#include <QtTest>

#include "usimove.h"
#include "timecontrol_test_helper.h"
#include <QGraphicsView>
#include <QTableView>

#include "appactivitystate.h"
#include "engineslots.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"
#include "recordline.h"
#include "livegamesession.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaypresenter.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "recordpane.h"
#include "branchtreeview.h"
#include "gameboard.h"
#include "shogiboard.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "test_replay.h"

namespace {

const QStringList opening = QStringLiteral(
    "7g7f 4a3b 2g2f 7a6b 2f2e 5a4a 2e2d 2c2d 2h2d P*2c").split(' ');

/// 司令塔の棋譜の行をライブ対局のセッション（分岐ツリー）に書く受け手（GameSession と同じ）
class LiveSessionSink final : public MatchRecordSink {
public:
    explicit LiveSessionSink(LiveGameSession& session) : m_session(session) {}
    void beginRecord(const MatchRecordStart&) override {}
    bool canAppendRecord() const override { return m_session.canAddMove(); }
    void appendRecord(const RecordEntry& entry) override { m_session.appendRecord(entry); }
    bool canUndoRecords(int count) const override
    {
        return !m_session.isActive() || m_session.canUndoMoves(count);
    }
    void undoRecords(int count) override
    {
        if (m_session.isActive()) QVERIFY(m_session.undoMoves(count));
    }
    void finishRecord(bool) override {}

private:
    LiveGameSession& m_session;
};

/// 人間対人間の対局を、本物の司令塔・ライブ対局のセッション・棋譜欄・分岐ツリーで進める
struct Harness : QObject {
    KifuBranchTree tree;
    KifuNavigationState state{&tree};
    KifuNavigationController nav{&state};
    KifuRecordListModel record{&tree};
    KifuBranchListModel branches{&tree};
    RecordPane pane;
    BranchTreeView treeManager;
    GameBoard gc;
    LiveGameSession session{&tree, &state};
    std::unique_ptr<KifuDisplayPresenter> display;
    ShogiClock clock;
    AppActivityState activity;
    EngineSlots engines;
    LiveSessionSink sink{session};
    std::unique_ptr<MatchCoordinator> match;
    int highlightPly = -1;
    bool coherentUndo = false;

    const RecordLine& line() const { return match->record(); }
    QString position() const { return line().usiPosition(line().moveCount())->command(); }

    /// 待ったを知らせた時点で、手順・ライブ対局・表示中の手が揃っている
    void observeUndo(int ply)
    {
        highlightPly = ply;
        coherentUndo = session.liveNode()->sfen() == line().lastSfen()
            && session.liveNode()->ply() == line().moveCount()
            && state.currentNode() == session.liveNode();
    }

    Harness()
    {
        const QString initial = SfenUtils::hirateSfen();
        tree.setRootSfen(initial);
        pane.setModels(&record, &branches);
        KifuDisplayPresenter::Deps dd;
        dd.navState = &state;
        dd.liveSession = &session;
        dd.recordModel = &record;
        dd.branchModel = &branches;
        dd.recordView = pane.kifuView();
        dd.treeView = &treeManager;
        display = std::make_unique<KifuDisplayPresenter>(dd);
        display->refresh();
        session.startFromRoot();

        MatchCoordinator::Deps deps;
        deps.gameBoard = &gc;
        deps.clock = &clock;
        deps.activity = &activity;
        deps.engines = &engines;
        match = std::make_unique<MatchCoordinator>(deps);
        match->setRecordSink(&sink);
        QObject::connect(match.get(), &MatchCoordinator::movesUndone, this, &Harness::observeUndo);
        RecordLine initialLine;
        initialLine.startSfen = initial;
        startFrom(initialLine);
    }

    /// 手順 from の最後の局面から対局を始める
    void startFrom(const RecordLine& from)
    {
        MatchStartOptions opt;
        opt.recordLine = from;
        opt.recordLine.currentPly = from.moveCount();
        match->start(opt);
    }

    /// 着手し、その手の考慮時間を時計に確定する（対局中の着手と同じ経路）
    bool playTimed(const QString& usi, qint64 ms)
    {
        clock.setMeasuredConsiderationTime(gc.board()->currentPlayer(), ms);
        return play(usi);
    }

    bool play(const QString& usi)
    {
        const auto move = UsiMove::parse(usi);
        if (!move) return false;
        const std::optional<RecordEntry> played = gc.applyMove(*move, match->lastMoveDestination());
        if (!played) return false;
        match->onHumanMove(*played);
        return true;
    }

    bool undo() { return match->undoTwoPlies(); }
};

} // namespace

class TestUndoFlow : public QObject
{
    Q_OBJECT

    static void verifyPosition(Harness& h, int ply)
    {
        QCOMPARE(h.line().moveCount(), ply);
        QCOMPARE(h.record.rowCount(), ply + 1);
        QCOMPARE(h.record.currentHighlightRow(), ply);
        QCOMPARE(h.pane.kifuView()->currentIndex().row(), ply);
        QCOMPARE(h.state.currentPly(), ply);
        QCOMPARE(h.session.liveNode()->ply(), ply);
        QCOMPARE(h.state.currentSfen(), h.line().lastSfen());
        QCOMPARE(h.gc.board()->toSfen(), h.line().lastSfen());
        QCOMPARE(h.treeManager.currentNodeId(), h.state.currentNode()->nodeId());
        // 棋譜欄の各行は、表示中の手までの手順のノード
        const QList<KifuBranchNode*> path = h.tree.pathToNode(h.state.currentNode());
        for (int row = 0; row < h.record.rowCount(); ++row) QCOMPARE(h.record.nodeAt(row), path.at(row));
        QCOMPARE(h.branches.candidateCount(), static_cast<int>(h.state.branchCandidatesAtCurrent().size()));
    }

private slots:
    void undoAndReplay_data()
    {
        QTest::addColumn<int>("count");
        QTest::newRow("black-to-move-captures-and-drop") << 10;
        QTest::newRow("white-to-move") << 9;
        QTest::newRow("white-first-turn") << 3;
        QTest::newRow("back-to-root") << 2;
    }

    void undoAndReplay()
    {
        QFETCH(int, count);
        Harness h;
        for (int i = 0; i < count; ++i) QVERIFY(h.play(opening.at(i)));
        const QString targetSfen = h.line().sfenAt(count - 2);
        const int undoneId = h.session.liveNode()->nodeId();
        QVERIFY(h.undo());
        verifyPosition(h, count - 2);
        QVERIFY(h.coherentUndo);
        QCOMPARE(h.line().lastSfen(), targetSfen);
        // 取り消した手はツリーに残し、棋譜欄は現在手までを表示する。
        QCOMPARE(h.tree.nodeCount(), count + 1);
        QCOMPARE(h.tree.exportLine(0).entries.size(), count);
        QVERIFY(h.tree.nodeAt(undoneId) != nullptr);
        // 戻した局面までの指し手は、分岐ツリー（棋譜の正本）の表示中の手順から取れる
        const RecordLine line = h.state.currentRecordLine();
        QCOMPARE(line.currentPly, count - 2);
        QCOMPARE(line.usiMoves(line.currentPly), opening.mid(0, count - 2));
        QCOMPARE(h.highlightPly, count - 2);
        const QString expectedPosition = count == 2 ? QStringLiteral("position startpos")
            : QStringLiteral("position startpos moves ") + opening.mid(0, count - 2).join(' ');
        QCOMPARE(h.position(), expectedPosition);
        QVERIFY(h.play(opening.at(count - 2)));
        QVERIFY(h.play(opening.at(count - 1)));
        verifyPosition(h, count);
        // 同じ手を指し直すと取り消した手のノードを再利用し、分岐は増えない。
        QCOMPARE(h.session.liveNode()->nodeId(), undoneId);
        QCOMPARE(h.tree.nodeCount(), count + 1);
        QCOMPARE(h.tree.lineCount(), 1);
        // 同じ局面へ戻るので、駒取り・駒打ちを含めて全盤面と持ち駒が一致する。
        QCOMPARE(h.line().sfens(), TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), opening.mid(0, count), false));
    }

    void repeatedUndoStopsAtRoot()
    {
        Harness h;
        for (const QString& move : opening) QVERIFY(h.play(move));
        for (int ply = 8; ply >= 0; ply -= 2) {
            QVERIFY(h.undo());
            verifyPosition(h, ply);
        }
        QVERIFY(!h.undo());
        verifyPosition(h, 0);
        QCOMPARE(h.tree.nodeCount(), opening.size() + 1);
        QVERIFY(h.play(QStringLiteral("2g2f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        verifyPosition(h, 2);
        // 指し直した手順が本譜、取り消した手順が分岐になる。
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.mainLine().size(), 3);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
        QCOMPARE(h.tree.root()->childAt(1)->displayText(), h.tree.exportLine(1).entries.at(0).displayText);
        QCOMPARE(h.tree.exportLine(1).entries.size(), opening.size());
    }

    // 「待った」で、時計も取り消した自分の手を指し始めたときへ戻る。続けて使うと2手ずつ戻る。
    void undoRestoresClockOfRetractedTurn()
    {
        Harness h;
        h.clock.setTimeControl(TestTime::rules(600, 600, 0, 0, 10, 10)); // 持ち時間10分＋加算10秒
        h.clock.setCurrentPlayer(Turn::Black);
        const auto verifyClock = [&h](const ShogiClock::Snapshot& expected) {
            const auto now = h.clock.pauseAndSnapshot();
            QCOMPARE(now.currentPlayer, Turn::Black);
            QCOMPARE(now.of(Turn::Black).remainingMs, expected.of(Turn::Black).remainingMs);
            QCOMPARE(now.of(Turn::White).remainingMs, expected.of(Turn::White).remainingMs);
            QCOMPARE(now.of(Turn::Black).totalConsiderationMs, expected.of(Turn::Black).totalConsiderationMs);
            QCOMPARE(now.of(Turn::White).totalConsiderationMs, expected.of(Turn::White).totalConsiderationMs);
            QCOMPARE(now.of(Turn::Black).considerationMs, 0LL);
        };

        const auto firstTurn = h.clock.pauseAndSnapshot();
        QVERIFY(h.playTimed(opening.at(0), 3000));
        // 1手だけでは「待った」できず、時計も変わらない
        const auto afterFirstMove = h.clock.pauseAndSnapshot();
        QVERIFY(!h.undo());
        verifyPosition(h, 1);
        QCOMPARE(h.clock.pauseAndSnapshot().of(Turn::Black).remainingMs, afterFirstMove.of(Turn::Black).remainingMs);

        QVERIFY(h.playTimed(opening.at(1), 1000));
        const auto secondTurn = h.clock.pauseAndSnapshot();
        QVERIFY(h.playTimed(opening.at(2), 5000));
        QVERIFY(h.playTimed(opening.at(3), 2000));
        h.clock.setConsiderationTimeMs(Turn::Black, 700); // 考え中の時間も返す

        QVERIFY(h.undo());
        verifyPosition(h, 2);
        verifyClock(secondTurn);
        QVERIFY(h.undo());
        verifyPosition(h, 0);
        verifyClock(firstTurn);
        QVERIFY(!h.undo()); // 開始局面より前には戻らない
        verifyPosition(h, 0);
        verifyClock(firstTurn);

        // 指し直した後の「待った」も、指し直した手の前へ戻る
        QVERIFY(h.playTimed(QStringLiteral("2g2f"), 4000));
        QVERIFY(h.playTimed(QStringLiteral("8c8d"), 1000));
        QVERIFY(h.undo());
        verifyPosition(h, 0);
        verifyClock(firstTurn);
    }

    void undoThenDifferentMoveKeepsUndoneLineAsVariation()
    {
        Harness h;
        for (int i = 0; i < 6; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* undoneHead = h.session.liveNode()->parent();
        const QString undoneText = undoneHead->displayText();
        QVERIFY(h.undo());
        verifyPosition(h, 4);
        QCOMPARE(h.tree.lineCount(), 1);
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        verifyPosition(h, 6);

        // 実際に指した手順が本譜（先頭の子）、待ったで取り消した手順が分岐として残る。
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.nodeCount(), 9);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
        KifuBranchNode* branchPoint = TestTree::findByPlyOnMainLine(h.tree, 4);
        QCOMPARE(branchPoint->childCount(), 2);
        QCOMPARE(branchPoint->childAt(1), undoneHead);
        QCOMPARE(h.tree.exportLine(1).entries.at(4).displayText, undoneText);
        QCOMPARE(h.tree.exportLine(1).entries.size(), 6);
        QCOMPARE(h.record.rowCount(), 7);
        // 分岐のある手は棋譜欄で「+」付きになる。
        QVERIFY(h.record.data(h.record.index(5, 0)).toString().endsWith(QLatin1Char('+')));

        // 対局終了後も実際の手順が本譜のまま確定する。
        KifuBranchNode* end = h.session.commit();
        QCOMPARE(end, h.tree.mainLine().last());
        QCOMPARE(h.tree.mainLine().size(), 7);
    }

    void replayingUndoneLineRestoresItAsMainLine()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* firstHead = h.session.liveNode()->parent();
        QVERIFY(h.undo());
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        KifuBranchNode* secondHead = h.session.liveNode()->parent();
        KifuBranchNode* branchPoint = TestTree::findByPlyOnMainLine(h.tree, 2);
        QCOMPARE(branchPoint->childAt(0), secondHead);
        QCOMPARE(branchPoint->childAt(1), firstHead);

        // 最初の手順に戻ると、その手順が再び本譜になる。
        QVERIFY(h.undo());
        QVERIFY(h.play(opening.at(2)));
        QVERIFY(h.play(opening.at(3)));
        verifyPosition(h, 4);
        QCOMPARE(h.tree.nodeCount(), 7);
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(branchPoint->childAt(0), firstHead);
        QCOMPARE(branchPoint->childAt(1), secondHead);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
    }

    void terminalAfterUndoBecomesMainLine()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* undoneHead = h.session.liveNode()->parent();
        QVERIFY(h.undo());
        // 待ったの直後に投了しても、投了で終わる手順が本譜になる。
        h.session.appendRecord(RecordEntry::ending(TerminalType::Resign, Turn::Black,
            RecordTime::fromMilliseconds(1000, 1000)));
        KifuBranchNode* end = h.session.commit();
        QVERIFY(end != nullptr && end->isTerminal());
        QCOMPARE(h.tree.mainLine().last(), end);
        QCOMPARE(TestTree::findByPlyOnMainLine(h.tree, 2)->childAt(1), undoneHead);
        QCOMPARE(h.tree.lineCount(), 2);
    }

    void insufficientMovesDoNotChangeState()
    {
        Harness h;
        QVERIFY(!h.undo());
        QCOMPARE(h.record.rowCount(), 1);
        QVERIFY(h.play(opening.at(0)));
        const QString position = h.position();
        QVERIFY(!h.undo());
        verifyPosition(h, 1);
        QCOMPARE(h.position(), position);
    }

    void midgameUndoPreservesExistingContinuation()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        auto* originalEnd = h.session.commit();
        QVERIFY(originalEnd != nullptr);
        auto* anchor = TestTree::findByPlyOnMainLine(h.tree, 2);
        // 以前の棋譜の2手目の局面から対局を始める
        RecordLine from = h.line().upTo(2);
        h.startFrom(from);
        h.session.startFromNode(anchor);
        // 同じ手を再利用しても、以前の棋譜のノードは削除しない。
        QVERIFY(h.play(opening.at(2)));
        QVERIFY(h.play(opening.at(3)));
        QCOMPARE(h.session.liveNode(), originalEnd);
        QVERIFY(h.undo());
        verifyPosition(h, 2);
        QCOMPARE(h.tree.nodeCount(), 5);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        QCOMPARE(h.position(), QStringLiteral("position startpos moves ") + opening.mid(0, 2).join(QLatin1Char(' ')));
        QVERIFY(!h.undo()); // 起点より前の棋譜を取り消さない
        verifyPosition(h, 2);
        // 別の手を指し直すと以前の棋譜の分岐になり、取り消しても分岐として残る。
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        QCOMPARE(h.tree.lineCount(), 2);
        QVERIFY(h.undo());
        verifyPosition(h, 2);
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        // 以前の棋譜の本譜より前には並べない。
        QVERIFY(h.play(QStringLiteral("9g9f")));
        QVERIFY(h.play(QStringLiteral("3c3d")));
        QCOMPARE(h.tree.lineCount(), 3);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        QCOMPARE(anchor->childAt(1), h.session.liveNode()->parent());
    }

    void undoRestoresPreviousDestinationForSameSquareNotation()
    {
        Harness h;
        for (const QString& move : opening) QVERIFY(h.play(move));
        QVERIFY(h.undo());
        QVERIFY(h.play(QStringLiteral("2h2d")));
        QVERIFY(h.record.nodeAt(9)->displayText().contains(QStringLiteral("同")));
    }
};

QTEST_MAIN(TestUndoFlow)
#include "tst_undo_flow.moc"
