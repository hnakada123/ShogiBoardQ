/// @file tst_consideration_resolver.cpp
/// @brief 検討する局面と直前の手を、分岐ツリーの表示中の手順から決めるテスト

#include <QtTest>

#include "considerationpositionresolver.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "recordline.h"
#include "shogimove.h"

namespace {

const QString kHirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
const QString kAfter76 = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
const QString kAfter34 = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
const QString kAfter26 = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4");
const QString kAfter84 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppppp1/1p7/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");

ShogiMove move(const char* usi, Turn mover, Piece moving)
{
    Q_UNUSED(mover)
    return ShogiMove(UsiMove::fromString(QString::fromLatin1(usi)), moving);
}

/// 平手から ▲７六歩 △３四歩 ▲２六歩 の本譜と、2手目の分岐 △８四歩
struct SampleTree {
    KifuBranchTree tree;
    KifuNavigationState nav{&tree};
    KifuBranchNode* ply1 = nullptr;
    KifuBranchNode* ply2 = nullptr;
    KifuBranchNode* ply3 = nullptr;
    KifuBranchNode* branch2 = nullptr;

    SampleTree()
    {
        tree.setRootSfen(kHirate);
        ply1 = tree.addMove(tree.root(), move("7g7f", Turn::Black, Piece::BlackPawn),
                            QStringLiteral("▲７六歩(77)"), kAfter76);
        ply2 = tree.addMove(ply1, move("3c3d", Turn::White, Piece::WhitePawn),
                            QStringLiteral("△３四歩(33)"), kAfter34);
        ply3 = tree.addMove(ply2, move("2g2f", Turn::Black, Piece::BlackPawn),
                            QStringLiteral("▲２六歩(27)"), kAfter26);
        branch2 = tree.addMove(ply1, move("8c8d", Turn::White, Piece::WhitePawn),
                               QStringLiteral("△８四歩(83)"), kAfter84);
    }

    RecordLine lineAt(KifuBranchNode* node)
    {
        nav.setCurrentNode(node);
        return nav.currentRecordLine();
    }
};

/// 局面に至った直前の手（USI。無ければ空）
QString lastUsi(const UsiPosition& position)
{
    return position.lastMove() ? position.lastMove()->toString() : QString();
}

/// 直前の手の移動先（読み筋の「同」の基準。無ければ無効なマス）
BoardSquare previousTo(const UsiPosition& position)
{
    return position.lastMove() ? position.lastMove()->to : BoardSquare{};
}

} // namespace

class TestConsiderationResolver : public QObject
{
    Q_OBJECT

private slots:
    void startPosition_hasNoPreviousMove();
    void midGame_usesMovesUpToDisplayedPly();
    void previousMove_isTheMoveJustPlayed();
    void rowOutOfRange_isClamped();
    void branch_usesDisplayedBranch();
    void terminalMove_isNotPartOfLine();
    void nonHirateStart_usesSfenCommand();
    void dropMove_destinationIsDropSquare();
    void emptyLine_usesFallbackSfen();
    void emptyLine_withoutFallback_isStartpos();
};

void TestConsiderationResolver::startPosition_hasNoPreviousMove()
{
    SampleTree t;
    const RecordLine line = t.lineAt(t.tree.root());
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);

    QCOMPARE(line.currentPly, 0);
    QCOMPARE(r.command(), QStringLiteral("position startpos"));
    QCOMPARE(previousTo(r).file, 0);
    QCOMPARE(previousTo(r).rank, 0);
    QVERIFY(lastUsi(r).isEmpty());
}

void TestConsiderationResolver::midGame_usesMovesUpToDisplayedPly()
{
    SampleTree t;
    const RecordLine line = t.lineAt(t.ply2);
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);

    QCOMPARE(line.currentPly, 2);
    QCOMPARE(r.command(), QStringLiteral("position startpos moves 7g7f 3c3d"));
}

void TestConsiderationResolver::previousMove_isTheMoveJustPlayed()
{
    // 2手目（△３四歩）の後の局面では、直前の手は △３四歩。3手目の ▲２六歩 ではない
    SampleTree t;
    const RecordLine line = t.lineAt(t.ply2);
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);

    QCOMPARE(lastUsi(r), QStringLiteral("3c3d"));
    QCOMPARE(previousTo(r).file, 3);
    QCOMPARE(previousTo(r).rank, 4);
}

void TestConsiderationResolver::rowOutOfRange_isClamped()
{
    SampleTree t;
    const RecordLine line = t.lineAt(t.ply3);

    const auto before = ConsiderationPositionResolver::resolve(line, -5);
    QCOMPARE(before.command(), QStringLiteral("position startpos"));
    QVERIFY(lastUsi(before).isEmpty());

    const auto after = ConsiderationPositionResolver::resolve(line, 99);
    QCOMPARE(after.command(), QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"));
    QCOMPARE(lastUsi(after), QStringLiteral("2g2f"));
}

void TestConsiderationResolver::branch_usesDisplayedBranch()
{
    SampleTree t;
    const RecordLine line = t.lineAt(t.branch2);
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);

    QCOMPARE(line.currentPly, 2);
    QCOMPARE(line.sfenAt(2), kAfter84);
    QCOMPARE(r.command(), QStringLiteral("position startpos moves 7g7f 8c8d"));
    QCOMPARE(lastUsi(r), QStringLiteral("8c8d"));
    QCOMPARE(previousTo(r).file, 8);
    QCOMPARE(previousTo(r).rank, 4);
}

void TestConsiderationResolver::terminalMove_isNotPartOfLine()
{
    SampleTree t;
    KifuBranchNode* resign = t.tree.addTerminalMove(t.ply3, TerminalType::Resign, Turn::White);
    const RecordLine line = t.lineAt(resign);

    QCOMPARE(line.moveCount(), 3);
    QCOMPARE(line.displayTexts().size(), 3);
    QCOMPARE(line.currentPly, 3);
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);
    QCOMPARE(r.command(), QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"));
    QCOMPARE(lastUsi(r), QStringLiteral("2g2f"));
}

void TestConsiderationResolver::nonHirateStart_usesSfenCommand()
{
    const QString start = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
    const QString after = QStringLiteral("4k4/9/4G4/9/9/9/9/9/4K4 w - 2");
    KifuBranchTree tree;
    tree.setRootSfen(start);
    KifuBranchNode* drop = tree.addMove(tree.root(), move("G*5c", Turn::Black, Piece::BlackGold),
                                        QStringLiteral("▲５三金打"), after);
    KifuNavigationState nav(&tree);
    nav.setCurrentNode(drop);
    const RecordLine line = nav.currentRecordLine();
    const auto r = ConsiderationPositionResolver::resolve(line, line.currentPly);

    QCOMPARE(r.command(), QStringLiteral("position sfen ") + start + QStringLiteral(" moves G*5c"));
}

void TestConsiderationResolver::dropMove_destinationIsDropSquare()
{
    const QString start = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
    const QString after = QStringLiteral("4k4/9/4G4/9/9/9/9/9/4K4 w - 2");
    KifuBranchTree tree;
    tree.setRootSfen(start);
    KifuBranchNode* drop = tree.addMove(tree.root(), move("G*5c", Turn::Black, Piece::BlackGold),
                                        QStringLiteral("▲５三金打"), after);
    KifuNavigationState nav(&tree);
    nav.setCurrentNode(drop);
    const auto r = ConsiderationPositionResolver::resolve(nav.currentRecordLine(), 1);

    QCOMPARE(lastUsi(r), QStringLiteral("G*5c"));
    QCOMPARE(previousTo(r).file, 5);
    QCOMPARE(previousTo(r).rank, 3);
}

void TestConsiderationResolver::emptyLine_usesFallbackSfen()
{
    const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
    QCOMPARE(ConsiderationPositionResolver::resolve(RecordLine{}, 0, sfen).command(),
             QStringLiteral("position sfen ") + sfen);
    QCOMPARE(ConsiderationPositionResolver::resolve(RecordLine{}, 0, QStringLiteral("position sfen ") + sfen).command(),
             QStringLiteral("position sfen ") + sfen);
    QCOMPARE(ConsiderationPositionResolver::resolve(RecordLine{}, 0, QStringLiteral("startpos")).command(),
             QStringLiteral("position startpos"));
    QCOMPARE(ConsiderationPositionResolver::resolve(RecordLine{}, 0, kHirate).command(),
             QStringLiteral("position startpos"));
}

void TestConsiderationResolver::emptyLine_withoutFallback_isStartpos()
{
    const auto r = ConsiderationPositionResolver::resolve(RecordLine{}, 3);
    QCOMPARE(r.command(), QStringLiteral("position startpos"));
    QVERIFY(lastUsi(r).isEmpty());
    QCOMPARE(previousTo(r).file, 0);
}

QTEST_MAIN(TestConsiderationResolver)
#include "tst_consideration_resolver.moc"
