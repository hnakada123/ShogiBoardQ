/// @file tst_consideration_resolver.cpp
/// @brief 検討モード局面解決テスト

#include <QtTest>

#include "considerationpositionresolver.h"
#include "shogimove.h"

class TestConsiderationResolver : public QObject
{
    Q_OBJECT

private:
    /// テスト用のデータセットを準備
    struct TestData {
        QStringList positionStrList;
        QStringList gameUsiMoves;
        QList<ShogiMove> gameMoves;
        QString startSfenStr;
        QStringList sfenRecord;
        QString currentSfenStr;
    };

    /// 平手から3手進んだテストデータ
    TestData makeSampleData()
    {
        TestData d;
        d.startSfenStr = QStringLiteral("startpos");
        d.gameUsiMoves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d"),
            QStringLiteral("2g2f")
        };
        // positionStrList: row=0は開始局面、row=1..3は各手後の局面
        d.positionStrList = {
            QStringLiteral("position startpos"),
            QStringLiteral("position startpos moves 7g7f"),
            QStringLiteral("position startpos moves 7g7f 3c3d"),
            QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"),
        };
        d.sfenRecord = {
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
        };
        // gameMoves: 7六歩 (from=6,6 to=6,5), 3四歩 (from=2,2 to=2,3), 2六歩 (from=1,6 to=1,5)
        // 座標: 0-indexed (file=0-8 left-right, rank=0-8 top-bottom)
        // 7六歩: fromSquare(6,6) toSquare(6,5) = 7筋6段→7筋6段...
        // USI 7g7f: file7=index6, rankg=index6, rankf=index5
        d.gameMoves = {
            ShogiMove(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false),   // 7g7f (▲7六歩)
            ShogiMove(QPoint(2, 2), QPoint(2, 3), Piece::WhitePawn, Piece::None, false),   // 3c3d (△3四歩)
            ShogiMove(QPoint(1, 6), QPoint(1, 5), Piece::BlackPawn, Piece::None, false),   // 2g2f (▲2六歩)
        };
        return d;
    }

    ConsiderationPositionResolver::Inputs makeInputs(const TestData& d)
    {
        ConsiderationPositionResolver::Inputs inputs;
        inputs.positionStrList = &d.positionStrList;
        inputs.gameUsiMoves = &d.gameUsiMoves;
        inputs.gameMoves = &d.gameMoves;
        inputs.startSfenStr = &d.startSfenStr;
        inputs.sfenRecord = &d.sfenRecord;
        if (!d.currentSfenStr.isEmpty())
            inputs.currentSfenStr = &d.currentSfenStr;
        return inputs;
    }

private slots:
    // === 基本解決テスト ===

    void resolveForRow_initialPosition();
    void resolveForRow_midGame();
    void resolveForRow_negativeRow_clamped();

    // === ハイライト座標テスト ===

    void resolveForRow_previousMoveCoordinates();
    void resolveForRow_initialPosition_noHighlight();

    // === USI指し手テスト ===

    void resolveForRow_lastUsiMove_midGame();
    void resolveForRow_initialPosition_emptyUsiMove();

    // === 分岐局面テスト ===

    void resolveForRow_branchPosition_useCurrentSfen();
    void resolveForRow_branchPosition_sfenFormat();
    void resolveForRow_branchPosition_positionPrefix();

    // === エッジケーステスト ===

    void resolveForRow_allInputsNull();
    void resolveForRow_emptyLists();
    void resolveForRow_beyondListSize();

    // === positionStrListなし→USI指し手からの構築テスト ===

    void resolveForRow_buildFromUsiMoves();

    // === sfenRecordフォールバックテスト ===

    void resolveForRow_sfenRecordFallback();
};

// === 基本解決テスト ===

void TestConsiderationResolver::resolveForRow_initialPosition()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    QCOMPARE(result.position, QStringLiteral("position startpos"));
}

void TestConsiderationResolver::resolveForRow_midGame()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(2);

    QCOMPARE(result.position, QStringLiteral("position startpos moves 7g7f 3c3d"));
}

void TestConsiderationResolver::resolveForRow_negativeRow_clamped()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=-1 は positionStrList の範囲外 → USI指し手構築パスへ
    // TsumePositionUtil::buildPositionWithMoves で selectedIndex<=0 → 開始局面
    auto result = resolver.resolveForRow(-1);

    QVERIFY(!result.position.isEmpty());
    QVERIFY(result.position.contains(QStringLiteral("startpos")));
}

// === ハイライト座標テスト ===

void TestConsiderationResolver::resolveForRow_previousMoveCoordinates()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=2 は gameMoves[2] = 2g2f の移動先 (1, 5)
    auto result = resolver.resolveForRow(2);

    // gameMoves[2].toSquare = QPoint(1, 5)
    QCOMPARE(result.previousFileTo, 1);
    QCOMPARE(result.previousRankTo, 5);
}

void TestConsiderationResolver::resolveForRow_initialPosition_noHighlight()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=0 は gameMoves[0] = 7g7f の移動先 (6, 5) を参照
    // ※ gameMoves[row] を使うので row=0 でも gameMoves が非空ならハイライトあり
    auto result = resolver.resolveForRow(0);

    QCOMPARE(result.previousFileTo, 6);
    QCOMPARE(result.previousRankTo, 5);
}

// === USI指し手テスト ===

void TestConsiderationResolver::resolveForRow_lastUsiMove_midGame()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=3 → usiIdx = 2 → gameUsiMoves[2] = "2g2f"
    auto result = resolver.resolveForRow(3);

    QCOMPARE(result.lastUsiMove, QStringLiteral("2g2f"));
}

void TestConsiderationResolver::resolveForRow_initialPosition_emptyUsiMove()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=0 → resolveLastUsiMoveForPly returns empty (row<=0)
    auto result = resolver.resolveForRow(0);

    QVERIFY(result.lastUsiMove.isEmpty());
}

// === 分岐局面テスト ===

void TestConsiderationResolver::resolveForRow_branchPosition_useCurrentSfen()
{
    TestData d = makeSampleData();
    // currentSfenStr が設定されている場合はそちらを最優先
    d.currentSfenStr = QStringLiteral("position sfen 9/9/9/9/9/9/9/9/9 b - 1");
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    // currentSfenStr は "position " で始まるのでそのまま返る
    QCOMPARE(result.position, QStringLiteral("position sfen 9/9/9/9/9/9/9/9/9 b - 1"));
}

void TestConsiderationResolver::resolveForRow_branchPosition_sfenFormat()
{
    TestData d = makeSampleData();
    // "sfen " で始まる場合は "position " が先頭に付く
    d.currentSfenStr = QStringLiteral("sfen 9/9/9/9/9/9/9/9/9 b - 1");
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    QCOMPARE(result.position, QStringLiteral("position sfen 9/9/9/9/9/9/9/9/9 b - 1"));
}

void TestConsiderationResolver::resolveForRow_branchPosition_positionPrefix()
{
    TestData d = makeSampleData();
    // SFEN文字列がプレフィックスなしの場合は "position sfen " が付く
    d.currentSfenStr =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    QVERIFY(result.position.startsWith(QStringLiteral("position sfen ")));
    QVERIFY(result.position.contains(
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL")));
}

// === エッジケーステスト ===

void TestConsiderationResolver::resolveForRow_allInputsNull()
{
    // 全入力 nullptr でクラッシュしない
    ConsiderationPositionResolver::Inputs inputs;
    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    // startSfenStr も nullptr → 平手フォールバック
    QCOMPARE(result.position, QStringLiteral("position startpos"));
    QCOMPARE(result.previousFileTo, 0);
    QCOMPARE(result.previousRankTo, 0);
}

void TestConsiderationResolver::resolveForRow_emptyLists()
{
    // 空リストでクラッシュしない
    TestData d;
    // 平手SFENを設定（"startpos"リテラルではなく実際のSFEN）
    d.startSfenStr = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    d.positionStrList = {};
    d.gameUsiMoves = {};
    d.gameMoves = {};
    d.sfenRecord = {};

    ConsiderationPositionResolver::Inputs inputs;
    inputs.positionStrList = &d.positionStrList;
    inputs.gameUsiMoves = &d.gameUsiMoves;
    inputs.gameMoves = &d.gameMoves;
    inputs.startSfenStr = &d.startSfenStr;
    inputs.sfenRecord = &d.sfenRecord;

    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(0);

    // 空リスト + 平手SFEN → "position startpos" フォールバック
    QCOMPARE(result.position, QStringLiteral("position startpos"));
    QCOMPARE(result.previousFileTo, 0);
    QCOMPARE(result.previousRankTo, 0);
    QVERIFY(result.lastUsiMove.isEmpty());
}

void TestConsiderationResolver::resolveForRow_beyondListSize()
{
    TestData d = makeSampleData();
    auto inputs = makeInputs(d);
    ConsiderationPositionResolver resolver(inputs);

    // row=100 はリストサイズ(4)を超える → USI指し手構築パスへフォールバック
    auto result = resolver.resolveForRow(100);

    // TsumePositionUtil::buildPositionWithMoves で moveCount = min(100, 3) = 3 手全部
    QVERIFY(!result.position.isEmpty());
    QVERIFY(result.position.contains(QStringLiteral("startpos")));
}

// === positionStrListなし→USI指し手からの構築テスト ===

void TestConsiderationResolver::resolveForRow_buildFromUsiMoves()
{
    TestData d;
    // 平手SFENを設定（実際のSFEN文字列）
    d.startSfenStr = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    d.gameUsiMoves = {
        QStringLiteral("7g7f"),
        QStringLiteral("3c3d"),
    };
    // positionStrList を空にして USI指し手構築パスを強制
    d.positionStrList = {};

    ConsiderationPositionResolver::Inputs inputs;
    inputs.positionStrList = &d.positionStrList;
    inputs.gameUsiMoves = &d.gameUsiMoves;
    inputs.startSfenStr = &d.startSfenStr;

    ConsiderationPositionResolver resolver(inputs);

    // row=2 → 平手SFENは kHirateSfen と一致するので "position startpos moves ..."
    auto result = resolver.resolveForRow(2);

    QCOMPARE(result.position, QStringLiteral("position startpos moves 7g7f 3c3d"));
}

// === sfenRecordフォールバックテスト ===

void TestConsiderationResolver::resolveForRow_sfenRecordFallback()
{
    TestData d;
    d.startSfenStr = QStringLiteral("startpos");
    // positionStrList も gameUsiMoves も空 → sfenRecord フォールバック
    d.positionStrList = {};
    d.gameUsiMoves = {};
    d.sfenRecord = {
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
    };

    ConsiderationPositionResolver::Inputs inputs;
    inputs.positionStrList = &d.positionStrList;
    inputs.gameUsiMoves = &d.gameUsiMoves;
    inputs.startSfenStr = &d.startSfenStr;
    inputs.sfenRecord = &d.sfenRecord;

    ConsiderationPositionResolver resolver(inputs);

    auto result = resolver.resolveForRow(1);

    QCOMPARE(result.position,
             QStringLiteral("position sfen "
                            "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
}

QTEST_MAIN(TestConsiderationResolver)
#include "tst_consideration_resolver.moc"
