/// @file tst_jishogi_calculator.cpp
/// @brief 持将棋・入玉宣言点数計算テスト

#include <QtTest>

#include "jishogicalculator.h"
#include "shogitypes.h"

class TestJishogiCalculator : public QObject
{
    Q_OBJECT

private:
    /// 空の盤面（81マス全て None）を返すヘルパー
    QList<Piece> emptyBoard() { return QList<Piece>(81, Piece::None); }

private slots:
    // === 基本点数計算テスト ===

    void calculate_emptyBoard_zeroPoints();
    void calculate_kingsOnly_zeroPoints();
    void calculate_bigPieces_fivePointsEach();
    void calculate_smallPieces_onePointEach();
    void calculate_promotedPieces_sameAsBase();
    void calculate_handPieces_counted();
    void calculate_kingInEnemyTerritory_sente();
    void calculate_kingInEnemyTerritory_gote();
    void calculate_kingNotInEnemyTerritory();
    void calculate_piecesInEnemyTerritory_counted();
    void calculate_piecesOutsideEnemyTerritory_notInDeclaration();

    // === 宣言条件テスト ===

    void meetsConditions_kingInCamp_tenPieces_noCheck_true();
    void meetsConditions_kingNotInCamp_false();
    void meetsConditions_lessThanTenPieces_false();
    void meetsConditions_kingInCheck_false();

    // === 24点法判定テスト ===

    void getResult24_thirtyOneOrMore_win();
    void getResult24_twentyFourToThirty_draw();
    void getResult24_lessThanTwentyFour_lose();
    void getResult24_conditionsNotMet_lose();

    // === 27点法判定テスト ===

    void getResult27_sente_twentyEightOrMore_win();
    void getResult27_sente_twentySeven_lose();
    void getResult27_gote_twentySevenOrMore_win();
    void getResult27_gote_twentySix_lose();
    void getResult27_conditionsNotMet_lose();

    // === エッジケーステスト ===

    void calculate_allPiecesOneSide_maxPoints();
    void calculate_exactThreshold_24point_boundary();
    void calculate_exactThreshold_27point_sente_boundary();
    void calculate_exactThreshold_27point_gote_boundary();
};

// ============================================================================
// 基本点数計算テスト
// ============================================================================

void TestJishogiCalculator::calculate_emptyBoard_zeroPoints()
{
    auto board = emptyBoard();
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 0);
    QCOMPARE(result.sente.declarationPoints, 0);
    QCOMPARE(result.gote.totalPoints, 0);
    QCOMPARE(result.gote.declarationPoints, 0);
}

void TestJishogiCalculator::calculate_kingsOnly_zeroPoints()
{
    // 玉は0点
    auto board = emptyBoard();
    // index = (rank-1)*9 + (file-1)
    // 先手玉を1一（rank=1, file=1）に配置
    board[0] = Piece::BlackKing;
    // 後手玉を9九（rank=9, file=9）に配置
    board[80] = Piece::WhiteKing;
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 0);
    QCOMPARE(result.gote.totalPoints, 0);
}

void TestJishogiCalculator::calculate_bigPieces_fivePointsEach()
{
    // 大駒（飛車・角・龍・馬）は各5点
    auto board = emptyBoard();
    // rank=5（中段）に先手の大駒4枚を配置 → 敵陣外なので totalPoints のみ加算
    int rank5file1 = (5 - 1) * 9 + (1 - 1); // index = 36
    board[rank5file1]     = Piece::BlackRook;
    board[rank5file1 + 1] = Piece::BlackBishop;
    board[rank5file1 + 2] = Piece::BlackDragon;
    board[rank5file1 + 3] = Piece::BlackHorse;
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 20); // 5*4
}

void TestJishogiCalculator::calculate_smallPieces_onePointEach()
{
    // 小駒（金・銀・桂・香・歩）は各1点
    auto board = emptyBoard();
    int rank5file1 = (5 - 1) * 9 + (1 - 1);
    board[rank5file1]     = Piece::BlackGold;
    board[rank5file1 + 1] = Piece::BlackSilver;
    board[rank5file1 + 2] = Piece::BlackKnight;
    board[rank5file1 + 3] = Piece::BlackLance;
    board[rank5file1 + 4] = Piece::BlackPawn;
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 5); // 1*5
}

void TestJishogiCalculator::calculate_promotedPieces_sameAsBase()
{
    // 成駒は成る前の駒と同じ点数（成銀=1点、龍=5点）
    auto board = emptyBoard();
    int rank5file1 = (5 - 1) * 9 + (1 - 1);
    board[rank5file1]     = Piece::BlackPromotedSilver; // 1点
    board[rank5file1 + 1] = Piece::BlackPromotedPawn;   // 1点
    board[rank5file1 + 2] = Piece::BlackPromotedLance;  // 1点
    board[rank5file1 + 3] = Piece::BlackPromotedKnight; // 1点
    board[rank5file1 + 4] = Piece::BlackDragon;         // 5点（龍=飛車の成駒）
    board[rank5file1 + 5] = Piece::BlackHorse;          // 5点（馬=角の成駒）
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 14); // 1*4 + 5*2
}

void TestJishogiCalculator::calculate_handPieces_counted()
{
    // 持ち駒も点数に含まれる（totalPoints と declarationPoints の両方に加算）
    auto board = emptyBoard();
    QMap<Piece, int> stand;
    stand[Piece::BlackRook] = 1;   // 5点
    stand[Piece::BlackPawn] = 3;   // 3点
    stand[Piece::WhiteBishop] = 1; // 5点
    stand[Piece::WhiteGold] = 2;   // 2点

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 8);        // 5 + 3
    QCOMPARE(result.sente.declarationPoints, 8);   // 持ち駒は宣言点にも加算
    QCOMPARE(result.gote.totalPoints, 7);          // 5 + 2
    QCOMPARE(result.gote.declarationPoints, 7);
}

void TestJishogiCalculator::calculate_kingInEnemyTerritory_sente()
{
    // 先手の玉が敵陣（1-3段目）にいる → kingInEnemyTerritory = true
    auto board = emptyBoard();
    board[(2 - 1) * 9 + (5 - 1)] = Piece::BlackKing; // rank=2, file=5
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QVERIFY(result.sente.kingInEnemyTerritory);
}

void TestJishogiCalculator::calculate_kingInEnemyTerritory_gote()
{
    // 後手の玉が敵陣（7-9段目）にいる → kingInEnemyTerritory = true
    auto board = emptyBoard();
    board[(8 - 1) * 9 + (5 - 1)] = Piece::WhiteKing; // rank=8, file=5
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QVERIFY(result.gote.kingInEnemyTerritory);
}

void TestJishogiCalculator::calculate_kingNotInEnemyTerritory()
{
    // 先手玉が4段目（自陣）→ kingInEnemyTerritory = false
    // 後手玉が6段目（自陣）→ kingInEnemyTerritory = false
    auto board = emptyBoard();
    board[(4 - 1) * 9 + (5 - 1)] = Piece::BlackKing; // rank=4
    board[(6 - 1) * 9 + (5 - 1)] = Piece::WhiteKing; // rank=6
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QVERIFY(!result.sente.kingInEnemyTerritory);
    QVERIFY(!result.gote.kingInEnemyTerritory);
}

void TestJishogiCalculator::calculate_piecesInEnemyTerritory_counted()
{
    // 先手の駒が敵陣（1-3段目）にある → declarationPoints と piecesInEnemyTerritory に加算
    auto board = emptyBoard();
    board[(1 - 1) * 9 + (1 - 1)] = Piece::BlackKing;   // rank=1, 敵陣内（玉はカウント対象外）
    board[(1 - 1) * 9 + (2 - 1)] = Piece::BlackRook;    // rank=1, 敵陣内 → 5点
    board[(2 - 1) * 9 + (1 - 1)] = Piece::BlackSilver;  // rank=2, 敵陣内 → 1点
    board[(3 - 1) * 9 + (1 - 1)] = Piece::BlackPawn;    // rank=3, 敵陣内 → 1点
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 7);           // 5+1+1
    QCOMPARE(result.sente.declarationPoints, 7);     // 敵陣内の駒のみ
    QCOMPARE(result.sente.piecesInEnemyTerritory, 3); // 玉を除く3枚
    QVERIFY(result.sente.kingInEnemyTerritory);
}

void TestJishogiCalculator::calculate_piecesOutsideEnemyTerritory_notInDeclaration()
{
    // 先手の駒が敵陣外 → totalPoints には加算されるが declarationPoints には加算されない
    auto board = emptyBoard();
    board[(5 - 1) * 9 + (1 - 1)] = Piece::BlackRook;   // rank=5（中段）→ 5点
    board[(7 - 1) * 9 + (1 - 1)] = Piece::BlackBishop;  // rank=7（自陣）→ 5点
    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    QCOMPARE(result.sente.totalPoints, 10);
    QCOMPARE(result.sente.declarationPoints, 0); // 敵陣外なので宣言点は0
    QCOMPARE(result.sente.piecesInEnemyTerritory, 0);
}

// ============================================================================
// 宣言条件テスト
// ============================================================================

void TestJishogiCalculator::meetsConditions_kingInCamp_tenPieces_noCheck_true()
{
    JishogiCalculator::PlayerScore score{};
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QVERIFY(JishogiCalculator::meetsDeclarationConditions(score, false));
}

void TestJishogiCalculator::meetsConditions_kingNotInCamp_false()
{
    JishogiCalculator::PlayerScore score{};
    score.kingInEnemyTerritory = false;
    score.piecesInEnemyTerritory = 10;

    QVERIFY(!JishogiCalculator::meetsDeclarationConditions(score, false));
}

void TestJishogiCalculator::meetsConditions_lessThanTenPieces_false()
{
    JishogiCalculator::PlayerScore score{};
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 9;

    QVERIFY(!JishogiCalculator::meetsDeclarationConditions(score, false));
}

void TestJishogiCalculator::meetsConditions_kingInCheck_false()
{
    JishogiCalculator::PlayerScore score{};
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QVERIFY(!JishogiCalculator::meetsDeclarationConditions(score, true));
}

// ============================================================================
// 24点法判定テスト
// ============================================================================

void TestJishogiCalculator::getResult24_thirtyOneOrMore_win()
{
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 31;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QString result = JishogiCalculator::getResult24(score, false);

    QCOMPARE(result, QObject::tr("勝ち"));
}

void TestJishogiCalculator::getResult24_twentyFourToThirty_draw()
{
    JishogiCalculator::PlayerScore score{};
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    // 24点 → 引き分け
    score.declarationPoints = 24;
    QCOMPARE(JishogiCalculator::getResult24(score, false), QObject::tr("引き分け"));

    // 30点 → 引き分け
    score.declarationPoints = 30;
    QCOMPARE(JishogiCalculator::getResult24(score, false), QObject::tr("引き分け"));
}

void TestJishogiCalculator::getResult24_lessThanTwentyFour_lose()
{
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 23;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult24(score, false), QObject::tr("負け"));
}

void TestJishogiCalculator::getResult24_conditionsNotMet_lose()
{
    // 宣言条件を満たしていない場合は点数に関わらず負け
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 40;
    score.kingInEnemyTerritory = false; // 条件不成立
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult24(score, false), QObject::tr("負け"));
}

// ============================================================================
// 27点法判定テスト
// ============================================================================

void TestJishogiCalculator::getResult27_sente_twentyEightOrMore_win()
{
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 28;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, true, false), QObject::tr("勝ち"));
}

void TestJishogiCalculator::getResult27_sente_twentySeven_lose()
{
    // 先手は28点必要、27点では負け
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 27;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, true, false), QObject::tr("負け"));
}

void TestJishogiCalculator::getResult27_gote_twentySevenOrMore_win()
{
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 27;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, false, false), QObject::tr("勝ち"));
}

void TestJishogiCalculator::getResult27_gote_twentySix_lose()
{
    // 後手は27点必要、26点では負け
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 26;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, false, false), QObject::tr("負け"));
}

void TestJishogiCalculator::getResult27_conditionsNotMet_lose()
{
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 40;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    // 王手がかかっている → 条件不成立
    QCOMPARE(JishogiCalculator::getResult27(score, true, true), QObject::tr("負け"));
}

// ============================================================================
// エッジケーステスト
// ============================================================================

void TestJishogiCalculator::calculate_allPiecesOneSide_maxPoints()
{
    // 全駒が先手にある場合の最大点数を確認
    // 盤上: 玉以外の全駒を敵陣に配置
    auto board = emptyBoard();
    QMap<Piece, int> stand;

    // 先手玉を敵陣に配置
    board[0] = Piece::BlackKing; // rank=1

    // 先手の大駒4枚を敵陣に配置（飛車2、角2 → 20点）
    // ただし将棋では飛車・角は各1枚ずつなので、飛車1+角1=10点を盤上に
    board[1] = Piece::BlackRook;   // 5点
    board[2] = Piece::BlackBishop; // 5点

    // 先手の小駒を敵陣に配置
    // 金2枚 + 銀2枚 + 桂2枚 + 香2枚 + 歩9枚 = 17枚17点
    board[3] = Piece::BlackGold;
    board[4] = Piece::BlackGold;
    board[5] = Piece::BlackSilver;
    board[6] = Piece::BlackSilver;
    board[7] = Piece::BlackKnight;
    board[8] = Piece::BlackKnight;
    // rank=2
    board[9]  = Piece::BlackLance;
    board[10] = Piece::BlackLance;
    board[11] = Piece::BlackPawn;
    board[12] = Piece::BlackPawn;
    board[13] = Piece::BlackPawn;
    board[14] = Piece::BlackPawn;
    board[15] = Piece::BlackPawn;
    board[16] = Piece::BlackPawn;
    board[17] = Piece::BlackPawn;
    // rank=3
    board[18] = Piece::BlackPawn;
    board[19] = Piece::BlackPawn;

    auto result = JishogiCalculator::calculate(board, stand);

    // 飛(5)+角(5)+金2(2)+銀2(2)+桂2(2)+香2(2)+歩9(9) = 27点
    QCOMPARE(result.sente.totalPoints, 27);
    QCOMPARE(result.sente.declarationPoints, 27); // 全て敵陣内
    QCOMPARE(result.sente.piecesInEnemyTerritory, 19); // 玉除く19枚
    QVERIFY(result.sente.kingInEnemyTerritory);
    // 後手は0点
    QCOMPARE(result.gote.totalPoints, 0);
}

void TestJishogiCalculator::calculate_exactThreshold_24point_boundary()
{
    // ちょうど24点の宣言点を構築（24点法の引き分け境界）
    // 大駒4枚(20点) + 小駒4枚(4点) = 24点を敵陣に配置
    auto board = emptyBoard();

    // 先手玉を敵陣に配置
    board[0] = Piece::BlackKing;

    // 大駒4枚を敵陣に配置
    board[1] = Piece::BlackRook;    // 5点
    board[2] = Piece::BlackBishop;  // 5点
    board[3] = Piece::BlackDragon;  // 5点（龍）
    board[4] = Piece::BlackHorse;   // 5点（馬）
    // ↑ 実際には先手の飛車・角は各1枚ずつだが、テストでは配置可能

    // 小駒4枚を敵陣に配置
    board[5] = Piece::BlackGold;    // 1点
    board[6] = Piece::BlackSilver;  // 1点
    board[7] = Piece::BlackKnight;  // 1点
    board[8] = Piece::BlackLance;   // 1点

    // さらに2枚で敵陣内10枚の条件を満たす
    board[(2 - 1) * 9 + 0] = Piece::BlackPawn;  // 1点
    board[(2 - 1) * 9 + 1] = Piece::BlackPawn;  // 1点

    QMap<Piece, int> stand;

    auto result = JishogiCalculator::calculate(board, stand);

    // 5*4 + 1*6 = 26点
    QCOMPARE(result.sente.declarationPoints, 26);
    QCOMPARE(result.sente.piecesInEnemyTerritory, 10);
    QVERIFY(result.sente.kingInEnemyTerritory);

    // 条件を満たしている
    QVERIFY(JishogiCalculator::meetsDeclarationConditions(result.sente, false));

    // 24点法: 26点 → 引き分け
    QCOMPARE(JishogiCalculator::getResult24(result.sente, false), QObject::tr("引き分け"));
}

void TestJishogiCalculator::calculate_exactThreshold_27point_sente_boundary()
{
    // 27点法で先手ちょうど28点（勝ち境界）
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 28;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, true, false), QObject::tr("勝ち"));

    // 1点少ない27点は負け
    score.declarationPoints = 27;
    QCOMPARE(JishogiCalculator::getResult27(score, true, false), QObject::tr("負け"));
}

void TestJishogiCalculator::calculate_exactThreshold_27point_gote_boundary()
{
    // 27点法で後手ちょうど27点（勝ち境界）
    JishogiCalculator::PlayerScore score{};
    score.declarationPoints = 27;
    score.kingInEnemyTerritory = true;
    score.piecesInEnemyTerritory = 10;

    QCOMPARE(JishogiCalculator::getResult27(score, false, false), QObject::tr("勝ち"));

    // 1点少ない26点は負け
    score.declarationPoints = 26;
    QCOMPARE(JishogiCalculator::getResult27(score, false, false), QObject::tr("負け"));
}

QTEST_MAIN(TestJishogiCalculator)
#include "tst_jishogi_calculator.moc"
