/// @file tst_pvtext.cpp
/// @brief 読み筋の値（PvText）と、その表記（KifuPresentation::pv・pvMove）のユニットテスト

#include <QtTest>

#include "kifupresentation.h"
#include "pvtext.h"
#include "sfenutils.h"

using namespace KifuPresentation;

class TestPvText : public QObject
{
    Q_OBJECT

private slots:
    void cleanup() { configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false); }

    /// 詰んだ局面ではエンジンが指し手の代わりに resign・win を返す。生の USI の語を出さず終局語にする
    void declarationsBecomeTerminalWords()
    {
        const PvText resign = PvText::play(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"), QStringList{"resign"});
        QCOMPARE(resign.declaration(), TerminalType::Resign);
        QVERIFY(resign.isDisplayable());
        QCOMPARE(resign.usiText(), QString());
        QCOMPARE(pv(resign), QStringLiteral("▲投了"));

        const PvText win = PvText::play(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1"), QStringList{"win"});
        QCOMPARE(pv(win), QStringLiteral("△入玉勝ち"));
    }

    /// 読み筋は開始局面の上で手を進めて書く（成り・同・取った駒を打つ・成駒の名前）
    void followsPositionThroughPv()
    {
        const PvText line = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(line.moveCount(), 5);
        QCOMPARE(line.usiText(), QStringLiteral("7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(line.firstMove()->toString(), QStringLiteral("7g7f"));
        QCOMPARE(pv(line), QStringLiteral("▲７六歩(77)△３四歩(33)▲２二角成(88)△同銀(31)▲４五角打"));
        QCOMPARE(pvMove(line, 4), QStringLiteral("△同銀(31)"));

        const PvText promoted = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("7g7f 3c3d 8h3c+ 4a3b 3c2b"));
        QCOMPARE(pv(promoted), QStringLiteral("▲７六歩(77)△３四歩(33)▲３三角成(88)△３二金(41)▲２二馬(33)"));
    }

    /// 初手の印は局面の手番で決まり、開始局面に至った手と同じマスへの初手は「同」と書く
    void usesSideToMoveAndPreviousMove()
    {
        // ▲７六歩 △３四歩 ▲２二角成 の後（後手番）
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4");
        const PvText line = PvText::play(sfen, QStringLiteral("3a2b B*3c"), BoardSquare{2, 2});
        QCOMPARE(pv(line), QStringLiteral("△同銀(31)▲３三角打"));
        QCOMPARE(pvMove(line, 1), QStringLiteral("△同銀(31)"));
        QCOMPARE(pv(PvText::play(sfen, QStringLiteral("3a2b"))), QStringLiteral("△２二銀(31)"));
    }

    /// 局面と合わない手（持っていない駒を打つ・相手の駒を動かす・駒のないマスから動かす）の手前で止め、
    /// そこから後の語は rest に残す。指せた手は表記にも USI の読み筋にも残す
    void stopsAtMoveNotFittingPosition()
    {
        const QString hirate = SfenUtils::hirateSfen();
        const PvText drop = PvText::play(hirate, QStringLiteral("7g7f G*5e"));
        QCOMPARE(pv(drop), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(drop.usiText(), QStringLiteral("7g7f"));
        QCOMPARE(drop.rest(), QStringList{QStringLiteral("G*5e")});

        QCOMPARE(PvText::play(hirate, QStringLiteral("7g7f 7f7e")).usiText(), QStringLiteral("7g7f"));

        // 1手も指せなければ、表に出す読み筋は無い（表記はエンジンが送った語のまま）
        const PvText none = PvText::play(hirate, QStringLiteral("5e5d"));
        QVERIFY(!none.isDisplayable());
        QVERIFY(!none.firstMove());
        QCOMPARE(pv(none), QStringLiteral("5e5d"));
    }

    /// 末尾の勝率などの指し手でない語はそのまま付け、途中に崩れた語があればそこまでにする
    void handlesNonMoveTokens()
    {
        const QString hirate = SfenUtils::hirateSfen();
        const PvText rate = PvText::play(hirate, QStringLiteral("7g7f 3c3d (57.54%)"));
        QCOMPARE(rate.annotation(), QStringLiteral("(57.54%)"));
        QCOMPARE(pv(rate), QStringLiteral("▲７六歩(77)△３四歩(33) (57.54%)"));
        QCOMPARE(rate.usiText(), QStringLiteral("7g7f 3c3d"));

        QCOMPARE(pv(PvText::play(hirate, QStringLiteral("7g7f 7z7f 3c3d"))), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(pv(PvText::play(hirate, QStringLiteral("7g7f (57%) 3c3d"))), QStringLiteral("▲７六歩(77)"));
    }

    /// 開始局面が無い・読めないときは1手も指さない
    void unreadablePositionKeepsEngineWords()
    {
        const PvText missing = PvText::play(QString(), QStringLiteral("7g7f"));
        QCOMPARE(missing.moveCount(), 0);
        QCOMPARE(missing.rest(), QStringList{QStringLiteral("7g7f")});
        QCOMPARE(PvText::play(QStringLiteral("not a sfen"), QStringLiteral("7g7f")).moveCount(), 0);
    }

    /// 英語表記は1手ずつ空白で区切り、移動元の要否は局面で決める。終局の宣言も同じ行に付ける
    void westernNotation()
    {
        configure(QStringLiteral("en"), QStringLiteral("western"), false);
        const PvText line = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("7g7f 3c3d 8h2b+ resign"));
        QCOMPARE(pv(line), QStringLiteral("▲P-7f △P-3d ▲Bx2b+ △投了"));
        QCOMPARE(pv(line, true), QStringLiteral("▲P7g-7f △P3c-3d ▲B8hx2b+ △投了"));
        QCOMPARE(pvMove(line, 3), QStringLiteral("▲Bx2b+"));
        QCOMPARE(pvMove(line, 4), QString());
    }
};

QTEST_GUILESS_MAIN(TestPvText)
#include "tst_pvtext.moc"
