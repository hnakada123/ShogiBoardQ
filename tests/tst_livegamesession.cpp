#include <QtTest>
#include <QSignalSpy>

#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class SessionObserver : public QObject
{
    Q_OBJECT
public:
    LiveGameSession* session = nullptr;
    int notifications = 0;
    bool consistent = true;
public slots:
    void treeChanged()
    {
        ++notifications;
        const auto* node = session->liveNode();
        consistent = consistent && node && node->ply() == session->totalPly()
            && node->sfen() == session->currentSfen()
            && session->moveCount() == session->moves().size()
            && node->isTerminal() == !session->canAddMove();
    }
};

class TestLiveGameSession : public QObject
{
    Q_OBJECT

private slots:
    void typedRecordsAreAtomicAndIndependentOfLabel()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();
        SessionObserver observer;
        observer.session = &session;
        connect(&tree, &KifuBranchTree::treeChanged, &observer, &SessionObserver::treeChanged);
        RecordEntry move;
        move.displayText = QStringLiteral("投了という文字を含む通常手");
        move.sfen = QStringLiteral("next-sfen");
        move.time = RecordTime::fromMilliseconds(1250, 1250);
        session.appendRecord(move);
        QVERIFY(session.canAddMove());
        QCOMPARE(session.liveNode()->terminalType(), TerminalType::None);
        QCOMPARE(session.liveNode()->time().elapsedMs, 1250);
        auto ending = RecordEntry::ending(TerminalType::Timeout, Turn::White);
        ending.displayText = QStringLiteral("localized ending");
        session.appendRecord(ending);
        QCOMPARE(session.liveNode()->terminalType(), TerminalType::Timeout);
        QCOMPARE(session.currentSfen(), move.sfen);
        QVERIFY(!session.canAddMove());
        QVERIFY(observer.consistent);
        QCOMPARE(observer.notifications, 2);
        session.appendRecord(move);
        QCOMPARE(observer.notifications, 2);
        QCOMPARE(session.moveCount(), 2);
    }

    void typedRecordsReuseByValue()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto ending = RecordEntry::ending(TerminalType::Resign, Turn::Black);
        auto* original = tree.addRecord(tree.root(), ending);
        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();
        ending.displayText = QStringLiteral("Resigned");
        session.appendRecord(ending);
        QCOMPARE(session.liveNode(), original);
        QCOMPARE(tree.nodeCount(), 2);
        session.discard();
        session.startFromRoot();
        RecordEntry normal;
        normal.displayText = QStringLiteral("投了");
        normal.sfen = QStringLiteral("different position");
        session.appendRecord(normal);
        QVERIFY(session.liveNode() != original);
        QVERIFY(!session.liveNode()->isTerminal());
        QVERIFY(session.canAddMove());
        QCOMPARE(tree.nodeCount(), 3);
    }

    void recordTimeParsesAtImportBoundary()
    {
        const auto time = RecordTime::fromText(QStringLiteral("( 1:02.345/12:34:56.789)"));
        QVERIFY(time.valid);
        QCOMPARE(time.elapsedMs, 62345);
        QCOMPARE(time.totalMs, 45296789);
        QCOMPARE(time.toText(), QStringLiteral("( 1:02.345/12:34:56.789)"));
        const auto generated = RecordTime::fromMilliseconds(time.elapsedMs, time.totalMs);
        QCOMPARE(generated.toText(), QStringLiteral("01:02.345/12:34:56.789"));
        const auto roundTrip = RecordTime::fromText(generated.toText());
        QCOMPARE(roundTrip.elapsedMs, time.elapsedMs);
        QCOMPARE(roundTrip.totalMs, time.totalMs);
        for (const auto* invalid : {"-", "01:99/00:00:00", "-1:00/00:00:00", "1:00/1:2",
                                   "9223372036854775807:00/00:00:00", "00:00/00:00:00.x"}) {
            const auto unknown = RecordTime::fromText(QString::fromLatin1(invalid));
            QVERIFY(!unknown.valid);
            QCOMPARE(unknown.toText(), QString::fromLatin1(invalid));
        }
    }

    void startFromRoot()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        QVERIFY(session.isActive());
        QCOMPARE(session.anchorPly(), 0);
    }

    void startFromNode()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("move1"), QStringLiteral("sfen1"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("move2"), QStringLiteral("sfen2"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n2);

        QVERIFY(session.isActive());
        QCOMPARE(session.anchorPly(), 2);
    }

    void addMove_incrementsMoveCount()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        QCOMPARE(session.moveCount(), 0);

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QStringLiteral("0:10"));
        QCOMPARE(session.moveCount(), 1);

        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("sfen2"), QStringLiteral("0:05"));
        QCOMPARE(session.moveCount(), 2);
    }

    void commit_addsToTree()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        int nodesBefore = tree.nodeCount();

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QString());
        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("sfen2"), QString());

        auto* result = session.commit();
        QVERIFY(result != nullptr);
        QVERIFY(!session.isActive());

        // Tree should have more nodes now
        QVERIFY(tree.nodeCount() > nodesBefore);
    }

    void discard_resetsSession()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromRoot();
        QVERIFY(session.isActive());

        ShogiMove move;
        session.addMove(move, QStringLiteral("▲７六歩"), QStringLiteral("sfen1"), QString());
        QCOMPARE(session.moveCount(), 1);

        // discard resets session state (nodes already added stay in tree)
        session.discard();
        QVERIFY(!session.isActive());
        QCOMPARE(session.moveCount(), 0);
    }

    void stressTest_repeatedSessions()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        LiveGameSession session;
        session.setTree(&tree);

        for (int cycle = 0; cycle < 50; ++cycle) {
            session.startFromRoot();
            QVERIFY(session.isActive());

            ShogiMove move;
            for (int i = 0; i < 10; ++i) {
                session.addMove(move,
                               QStringLiteral("move%1_%2").arg(cycle).arg(i),
                               QStringLiteral("sfen%1_%2").arg(cycle).arg(i),
                               QString());
            }
            QCOMPARE(session.moveCount(), 10);

            session.commit();
            QVERIFY(!session.isActive());
        }

        // Tree should have all committed moves
        QVERIFY(tree.lineCount() >= 1);
    }

    void totalPly()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);

        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("m1"), QStringLiteral("s1"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);

        session.addMove(move, QStringLiteral("m2"), QStringLiteral("s2"), QString());
        session.addMove(move, QStringLiteral("m3"), QStringLiteral("s3"), QString());

        QCOMPARE(session.totalPly(), 3); // anchorPly(1) + moveCount(2)
    }

    void addMove_reusesExistingChild()
    {
        // 対局後に途中局面へ戻り、既存の手と同じ手を指し直しても重複ノードを作らない
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"));
        tree.addMove(n2, move, QStringLiteral("▲投了"), QStringLiteral("board2 b - 4"));
        const int nodesBefore = tree.nodeCount();

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);
        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"), QStringLiteral("0:15"));

        QCOMPARE(tree.nodeCount(), nodesBefore);   // ノードは増えない
        QCOMPARE(session.liveNode(), n2);           // 既存ノードを再利用
        QCOMPARE(n1->childCount(), 1);
        QCOMPARE(tree.lineCount(), 1);
        QCOMPARE(session.moveCount(), 1);
        QCOMPARE(session.currentLineIndex(), 0);
    }

    void addMove_branchesAtDivergence()
    {
        // 同じ手を1手指し直した後に別の手を指すと、分岐は実際に手が分かれた地点にできる
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"));
        auto* n3 = tree.addMove(n2, move, QStringLiteral("▲投了"), QStringLiteral("board2 b - 4"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);
        session.addMove(move, QStringLiteral("△３四歩"), QStringLiteral("board2 b - 3"), QString());
        session.addMove(move, QStringLiteral("▲２六歩"), QStringLiteral("board3 w - 4"), QString());

        QCOMPARE(n1->childCount(), 1);              // ３四歩は重複しない
        QCOMPARE(n2->childCount(), 2);              // 投了 と ２六歩 の分岐
        QVERIFY(n2->hasBranch());
        QCOMPARE(n2->childAt(0), n3);
        QCOMPARE(session.liveNode(), n2->childAt(1));
        QCOMPARE(session.liveNode()->ply(), 3);
        QCOMPARE(tree.lineCount(), 2);
        QCOMPARE(tree.allLines().at(1).branchPly, 3);

        auto* end = session.commit();
        QCOMPARE(end, n2->childAt(1));
        QVERIFY(!session.isActive());
    }

    void addMove_terminalCarriesParentSfen()
    {
        // ライブ対局の終局手は盤面から組んだ次手番号付き SFEN で渡されるが、
        // ツリー上のノードは親と同じ局面を持つこと
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);
        session.addMove(move, QStringLiteral("△投了"), QStringLiteral("board1 w - 3"), QStringLiteral("0:05"));

        KifuBranchNode* terminal = session.liveNode();
        QVERIFY(terminal != nullptr);
        QVERIFY(terminal->isTerminal());
        QCOMPARE(terminal->sfen(), n1->sfen());
        QCOMPARE(terminal->timeText(), QStringLiteral("0:05"));
        // 正本とセッションは終局行でも同じ局面を観測する。
        QCOMPARE(session.currentSfen(), terminal->sfen());
    }

    void addMove_terminalStopsFurtherMoves()
    {
        // 終局手を追加するとセッションは終了状態になり、以降の追加は無視される
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);
        QVERIFY(session.canAddMove());

        session.addMove(move, QStringLiteral("△投了"), QStringLiteral("board1 w - 3"), QString());
        QVERIFY(!session.canAddMove());
        KifuBranchNode* terminal = session.liveNode();
        QVERIFY(terminal != nullptr && terminal->isTerminal());
        QCOMPARE(session.moveCount(), 1);

        session.addMove(move, QStringLiteral("▲２六歩"), QStringLiteral("board2 b - 4"), QString());
        QCOMPARE(session.moveCount(), 1);             // 追加されない
        QCOMPARE(session.liveNode(), terminal);       // 位置も動かない
        QCOMPARE(terminal->childCount(), 0);

        // commit はリアルタイム追加済みの終局手ノードを返す
        QCOMPARE(session.commit(), terminal);
        QVERIFY(!session.isActive());
    }

    void addMove_reusesTerminalOfSameType()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩"), QStringLiteral("board1 w - 2"));
        auto* resign = tree.addMove(n1, move, QStringLiteral("△投了"), QStringLiteral("board1 w - 3"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(n1);
        session.addMove(move, QStringLiteral("△投了"), QStringLiteral("board1 w - 3"), QString());

        QCOMPARE(n1->childCount(), 1);
        QCOMPARE(session.liveNode(), resign);
    }

    // 棋譜の途中から対局しても、累計時間は同じ側の直前の累計から続ける
    void continuedElapsedTextContinuesRecordTotals()
    {
        KifuBranchTree tree;
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        auto* m1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"),
                                QStringLiteral("00:05/00:00:05"));
        auto* m2 = tree.addMove(m1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("s2"),
                                QStringLiteral("00:03/00:00:03"));

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(m2);
        // 先手：棋譜の累計 5 秒 + 今回 2 秒
        const QString first = session.continuedElapsedText(QStringLiteral("00:02/00:00:02"));
        QCOMPARE(first, QStringLiteral("00:02/00:00:07"));
        session.setFirstMoveNote(QStringLiteral("対局メモ"));
        session.addMove(ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("s3"), first);
        // 後手：棋譜の累計 3 秒 + 今回 4 秒
        QCOMPARE(session.continuedElapsedText(QStringLiteral("00:04/00:00:04")), QStringLiteral("00:04/00:00:07"));
        // 形式が違う表記はそのまま
        QCOMPARE(session.continuedElapsedText(QStringLiteral("-")), QStringLiteral("-"));
        // 最初に新しく作った手にメモが付く
        QCOMPARE(m2->childCount(), 1);
        QCOMPARE(m2->childAt(0)->comment(), QStringLiteral("対局メモ"));
    }
};

QTEST_MAIN(TestLiveGameSession)
#include "tst_livegamesession.moc"
