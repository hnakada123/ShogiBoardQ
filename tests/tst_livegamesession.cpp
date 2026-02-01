/**
 * @file tst_livegamesession.cpp
 * @brief LiveGameSession と分岐ツリー表示のユニットテスト
 *
 * テスト対象:
 * 1. LiveGameSession::addMove() が KifuBranchTree に正しくノードを追加するか
 * 2. 分岐マークが正しく計算されるか（単一ラインでは '+' なし）
 * 3. 分岐ツリー表示用のデータ構造が正しいか（disp[0] = 開始局面エントリ）
 */

#include <QtTest>
#include <QSignalSpy>

#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifdisplayitem.h"
#include "shogimove.h"

// EngineAnalysisTab::ResolvedRowLite と同等の構造（テスト用）
struct TestResolvedRowLite {
    int startPly = 1;
    int parent = -1;
    QList<KifDisplayItem> disp;
    QStringList sfen;
};

class TestLiveGameSession : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // LiveGameSession のテスト
    void testStartFromRoot();
    void testAddMoveToTree();
    void testAddMultipleMoves();
    void testBranchMarksEmptyForSingleLine();
    void testResolvedRowLiteConversion();

    // 途中局面からの分岐テスト
    void testStartFromMidPosition();
    void testBranchFromPly4();

private:
    KifuBranchTree* m_tree = nullptr;
    LiveGameSession* m_session = nullptr;

    // ヘルパー: KifuBranchTree から TestResolvedRowLite 形式に変換
    // これは KifuDisplayCoordinator::onLiveGameMoveAdded() と同じロジック
    QVector<TestResolvedRowLite> convertToResolvedRows();
};

void TestLiveGameSession::initTestCase()
{
    // テスト全体の初期化
}

void TestLiveGameSession::cleanupTestCase()
{
    // テスト全体の後処理
}

void TestLiveGameSession::init()
{
    // 各テスト前に新しいツリーとセッションを作成
    m_tree = new KifuBranchTree(this);
    m_tree->setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    m_session = new LiveGameSession(this);
    m_session->setTree(m_tree);
}

void TestLiveGameSession::cleanup()
{
    // 各テスト後にクリーンアップ
    delete m_session;
    m_session = nullptr;
    delete m_tree;
    m_tree = nullptr;
}

void TestLiveGameSession::testStartFromRoot()
{
    // セッションがアクティブでないことを確認
    QVERIFY(!m_session->isActive());

    // ルートからセッションを開始
    m_session->startFromRoot();

    // セッションがアクティブになったことを確認
    QVERIFY(m_session->isActive());
    QCOMPARE(m_session->anchorPly(), 0);
    QVERIFY(!m_session->anchorSfen().isEmpty());
}

void TestLiveGameSession::testAddMoveToTree()
{
    m_session->startFromRoot();

    // ツリーの初期状態を確認
    QVERIFY(m_tree->root() != nullptr);
    QCOMPARE(m_tree->root()->childCount(), 0);

    // 指し手を追加 (▲７六歩: 7七から7六へ)
    ShogiMove move(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);

    const QString displayText = QStringLiteral("▲７六歩(77)");
    const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");

    m_session->addMove(move, displayText, sfen, QString());

    // ツリーにノードが追加されたことを確認
    QCOMPARE(m_tree->root()->childCount(), 1);

    KifuBranchNode* addedNode = m_tree->root()->childAt(0);
    QVERIFY(addedNode != nullptr);
    QCOMPARE(addedNode->ply(), 1);
    QCOMPARE(addedNode->displayText(), displayText);
}

void TestLiveGameSession::testAddMultipleMoves()
{
    m_session->startFromRoot();

    // 1手目: ▲７六歩
    ShogiMove move1(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    m_session->addMove(move1, QStringLiteral("▲７六歩(77)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                       QString());

    // 2手目: △３四歩
    ShogiMove move2(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false);
    m_session->addMove(move2, QStringLiteral("△３四歩(33)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                       QString());

    // 3手目: ▲２六歩
    ShogiMove move3(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    m_session->addMove(move3, QStringLiteral("▲２六歩(27)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
                       QString());

    // セッションの状態を確認
    QCOMPARE(m_session->totalPly(), 3);
    QCOMPARE(m_session->moveCount(), 3);

    // ツリーの構造を確認
    // root -> move1 -> move2 -> move3
    QCOMPARE(m_tree->root()->childCount(), 1);

    KifuBranchNode* node1 = m_tree->root()->childAt(0);
    QVERIFY(node1 != nullptr);
    QCOMPARE(node1->ply(), 1);
    QCOMPARE(node1->childCount(), 1);

    KifuBranchNode* node2 = node1->childAt(0);
    QVERIFY(node2 != nullptr);
    QCOMPARE(node2->ply(), 2);
    QCOMPARE(node2->childCount(), 1);

    KifuBranchNode* node3 = node2->childAt(0);
    QVERIFY(node3 != nullptr);
    QCOMPARE(node3->ply(), 3);
}

void TestLiveGameSession::testBranchMarksEmptyForSingleLine()
{
    // 分岐マーク更新シグナルをスパイ
    QSignalSpy branchMarksSpy(m_session, &LiveGameSession::branchMarksUpdated);

    m_session->startFromRoot();

    // 1手目を追加
    ShogiMove move1(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    m_session->addMove(move1, QStringLiteral("▲７六歩(77)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                       QString());

    // シグナルが発行されたことを確認
    QVERIFY(branchMarksSpy.count() >= 1);

    // 最後のシグナルを取得
    QList<QVariant> lastArgs = branchMarksSpy.last();
    QSet<int> branchPlys = lastArgs.at(0).value<QSet<int>>();

    // 単一ラインなので分岐マークは空であるべき
    QVERIFY2(branchPlys.isEmpty(),
             qPrintable(QString("Expected empty branch marks, but got %1 marks").arg(branchPlys.size())));

    // 2手目を追加
    ShogiMove move2(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false);
    m_session->addMove(move2, QStringLiteral("△３四歩(33)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                       QString());

    // 再度確認
    lastArgs = branchMarksSpy.last();
    branchPlys = lastArgs.at(0).value<QSet<int>>();

    // まだ単一ラインなので分岐マークは空であるべき
    QVERIFY2(branchPlys.isEmpty(),
             qPrintable(QString("Expected empty branch marks after 2nd move, but got %1 marks").arg(branchPlys.size())));
}

QVector<TestResolvedRowLite> TestLiveGameSession::convertToResolvedRows()
{
    // KifuDisplayCoordinator::onLiveGameMoveAdded() と同じロジック
    QVector<TestResolvedRowLite> rows;
    QVector<BranchLine> lines = m_tree->allLines();

    for (const BranchLine& line : std::as_const(lines)) {
        TestResolvedRowLite row;
        row.startPly = (line.branchPly > 0) ? line.branchPly : 1;
        row.parent = -1;

        // rebuildBranchTree() は disp[0] が開始局面エントリ（prettyMove 空）、
        // disp[i] (i>=1) が i 手目に対応することを期待する。
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            KifDisplayItem item;
            if (node->ply() == 0) {
                // 開始局面: prettyMove を空にして追加
                item.prettyMove = QString();
            } else {
                item.prettyMove = node->displayText();
            }
            item.timeText = node->timeText();
            item.comment = node->comment();
            row.disp.append(item);
            row.sfen.append(node->sfen());
        }

        rows.append(row);
    }

    return rows;
}

void TestLiveGameSession::testResolvedRowLiteConversion()
{
    m_session->startFromRoot();

    // 1手目を追加
    ShogiMove move1(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    const QString move1Text = QStringLiteral("▲７六歩(77)");
    m_session->addMove(move1, move1Text,
                       QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                       QString());

    // 2手目を追加
    ShogiMove move2(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false);
    const QString move2Text = QStringLiteral("△３四歩(33)");
    m_session->addMove(move2, move2Text,
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                       QString());

    // ResolvedRowLite に変換
    QVector<TestResolvedRowLite> rows = convertToResolvedRows();

    // 1つのラインが存在すべき
    QCOMPARE(rows.size(), 1);

    const auto& row = rows.at(0);

    // disp のサイズは 3 であるべき（開始局面 + 2手）
    QCOMPARE(row.disp.size(), 3);

    // disp[0] は開始局面エントリ（prettyMove が空）
    QVERIFY2(row.disp.at(0).prettyMove.isEmpty(),
             qPrintable(QString("disp[0].prettyMove should be empty, but got: '%1'").arg(row.disp.at(0).prettyMove)));

    // disp[1] は 1手目
    QCOMPARE(row.disp.at(1).prettyMove, move1Text);

    // disp[2] は 2手目
    QCOMPARE(row.disp.at(2).prettyMove, move2Text);

    // startPly は 1 であるべき
    QCOMPARE(row.startPly, 1);
}

void TestLiveGameSession::testStartFromMidPosition()
{
    // 本譜を4手目まで作成
    m_session->startFromRoot();

    // 1手目: ▲７六歩
    ShogiMove move1(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    m_session->addMove(move1, QStringLiteral("▲７六歩(77)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                       QString());

    // 2手目: △３四歩
    ShogiMove move2(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false);
    m_session->addMove(move2, QStringLiteral("△３四歩(33)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                       QString());

    // 3手目: ▲２六歩
    ShogiMove move3(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false);
    m_session->addMove(move3, QStringLiteral("▲２六歩(27)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
                       QString());

    // 4手目: △８四歩
    ShogiMove move4(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false);
    const QString sfen4 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
    m_session->addMove(move4, QStringLiteral("△８四歩(83)"),
                       sfen4,
                       QString());

    // 4手目のノードを取得
    KifuBranchNode* node4 = m_tree->findBySfen(sfen4);
    QVERIFY2(node4 != nullptr, "node4 should exist");
    QCOMPARE(node4->ply(), 4);

    // セッションを終了
    m_session->discard();
    QVERIFY(!m_session->isActive());

    // 4手目から新しいセッションを開始
    m_session->startFromNode(node4);
    QVERIFY(m_session->isActive());
    QCOMPARE(m_session->anchorPly(), 4);

    // 5手目を追加: ▲２五歩（分岐の最初の手）
    ShogiMove move5(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false);
    const QString sfen5 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6");
    m_session->addMove(move5, QStringLiteral("▲２五歩(26)"),
                       sfen5,
                       QString());

    // ★ 重要: 追加されたノードのplyが5であることを確認
    KifuBranchNode* node5 = m_tree->findBySfen(sfen5);
    QVERIFY2(node5 != nullptr, "node5 (branch move) should exist in tree");
    QCOMPARE(node5->ply(), 5);  // Branch move should have ply=5, not ply=1

    // ノード5の親が4手目であることを確認
    QVERIFY2(node5->parent() == node4, "node5's parent should be node4");

    // 分岐ツリーの構造を確認
    QVector<BranchLine> lines = m_tree->allLines();
    qDebug() << "Number of lines:" << lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        qDebug() << "Line" << i << ": branchPly=" << line.branchPly
                 << "nodes=" << line.nodes.size();
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            qDebug() << "  Node ply=" << node->ply() << "text=" << node->displayText();
        }
    }
}

void TestLiveGameSession::testBranchFromPly4()
{
    // 本譜を7手目まで作成（最初の対局）
    m_session->startFromRoot();

    // 1手目: ▲７六歩
    m_session->addMove(ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                       QStringLiteral("▲７六歩(77)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                       QString());

    // 2手目: △３四歩
    m_session->addMove(ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                       QStringLiteral("△３四歩(33)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                       QString());

    // 3手目: ▲２六歩
    m_session->addMove(ShogiMove(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                       QStringLiteral("▲２六歩(27)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
                       QString());

    // 4手目: △８四歩
    const QString sfen4 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
    m_session->addMove(ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                       QStringLiteral("△８四歩(83)"),
                       sfen4,
                       QString());

    // 5手目: ▲２五歩
    m_session->addMove(ShogiMove(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false),
                       QStringLiteral("▲２五歩(26)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6"),
                       QString());

    // 6手目: △８五歩
    m_session->addMove(ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
                       QStringLiteral("△８五歩(84)"),
                       QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p5P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL b - 7"),
                       QString());

    // 7手目: ▲投了
    m_session->addTerminalMove(TerminalType::Resign, QStringLiteral("▲投了"), QString());

    m_session->discard();

    // 4手目のノードを取得
    KifuBranchNode* node4 = m_tree->findBySfen(sfen4);
    QVERIFY2(node4 != nullptr, "node4 should exist");
    QCOMPARE(node4->ply(), 4);

    qDebug() << "=== Starting second game from ply 4 ===";

    // 4手目から新しいセッションを開始（2局目）
    m_session->startFromNode(node4);
    QVERIFY(m_session->isActive());
    QCOMPARE(m_session->anchorPly(), 4);
    QCOMPARE(m_session->branchPoint(), node4);

    // 2局目の5手目: △８五歩（分岐）
    const QString branchSfen5 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p4P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6");
    m_session->addMove(ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
                       QStringLiteral("△８五歩(84)"),
                       branchSfen5,
                       QString());

    // ★★★ 重要: 追加されたノードのplyが5であることを確認 ★★★
    KifuBranchNode* branchNode5 = m_tree->findBySfen(branchSfen5);
    QVERIFY2(branchNode5 != nullptr, "branchNode5 should exist in tree");

    qDebug() << "branchNode5->ply() =" << branchNode5->ply();
    qDebug() << "branchNode5->parent() =" << (void*)branchNode5->parent();
    qDebug() << "node4 =" << (void*)node4;
    qDebug() << "branchNode5->parent()->ply() =" << (branchNode5->parent() ? branchNode5->parent()->ply() : -1);

    QCOMPARE(branchNode5->ply(), 5);  // Branch move should have ply=5
    QVERIFY2(branchNode5->parent() == node4, "branchNode5's parent should be node4 (ply=4)");

    // ライブ対局の現在ノードとラインインデックスを確認
    QCOMPARE(m_session->liveNode(), branchNode5);
    const int liveLineIndex = m_session->currentLineIndex();
    QCOMPARE(liveLineIndex, m_tree->findLineIndexForNode(branchNode5));
    QVERIFY(liveLineIndex > 0);

    // 分岐ツリーの構造を確認
    QVector<BranchLine> lines = m_tree->allLines();
    qDebug() << "=== Branch lines ===";
    qDebug() << "Number of lines:" << lines.size();

    // 2ライン（本譜 + 分岐）があるべき
    QCOMPARE(lines.size(), 2);

    // 分岐ラインのbranchPlyが5であることを確認
    const BranchLine& branchLine = lines.at(1);
    qDebug() << "Branch line branchPly:" << branchLine.branchPly;
    qDebug() << "Branch line branchPoint ply:" << (branchLine.branchPoint ? branchLine.branchPoint->ply() : -1);

    QCOMPARE(branchLine.branchPly, 5);  // Branch line should start at ply 5
    QVERIFY2(branchLine.branchPoint == node4, "Branch point should be node4 (ply=4)");
}

QTEST_GUILESS_MAIN(TestLiveGameSession)
#include "tst_livegamesession.moc"
