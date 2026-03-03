/// @file tst_tsume_search.cpp
/// @brief 詰み探索フロー統合テスト（TsumePositionUtil 局面構築→コマンド生成）

#include <QtTest>

#include "tsumepositionutil.h"

class TestTsumeSearch : public QObject
{
    Q_OBJECT

private:
    /// SFEN文字列からトークン分割して手番を取得（"b" or "w"）
    static QString extractTurn(const QString& positionCmd)
    {
        // "position sfen <board> <turn> <hand> <movenum>" の形式
        const QString sfenPart = positionCmd.mid(QStringLiteral("position sfen ").length());
        const QStringList tokens = sfenPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() >= 2) return tokens[1];
        return {};
    }

    /// SFEN文字列から手数トークンを取得
    static QString extractMoveNumber(const QString& positionCmd)
    {
        const QString sfenPart = positionCmd.mid(QStringLiteral("position sfen ").length());
        const QStringList tokens = sfenPart.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() >= 4) return tokens[3];
        return {};
    }

private slots:
    // --- 局面構築統合テスト ---
    void buildForMate_singleKing_forcesAttackerTurn();
    void buildForMate_tsumePosition_correctFormat();
    void buildForMate_midGamePosition_withMoves();
    void buildForMate_handicapPosition();

    // --- チェックメイトコマンド構築 ---
    void buildCheckmateSendCommand_fromStartpos();
    void buildCheckmateSendCommand_fromSfen();

    // --- 典型局面テスト ---
    void tsumePosition_oneMoveMate();
    void tsumePosition_threeMoveMate();
    void tsumePosition_noMatePosition();

    // --- エッジケース ---
    void buildForMate_emptyBoard_returnsEmpty();
    void buildForMate_positionStrList_fallback();
};

// ============================================================
// 局面構築統合テスト
// ============================================================

void TestTsumeSearch::buildForMate_singleKing_forcesAttackerTurn()
{
    // 後手玉(k)のみ → 先手(b)が攻方として手番を強制
    const QString sfenOnlyGoteKing = QStringLiteral("4k4/9/9/9/9/9/9/9/9 b - 1");
    QStringList sfenRecord = {sfenOnlyGoteKing};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    QCOMPARE(extractTurn(result), QStringLiteral("b"));
    // 手数はリセットされて1
    QCOMPARE(extractMoveNumber(result), QStringLiteral("1"));
}

void TestTsumeSearch::buildForMate_tsumePosition_correctFormat()
{
    // 実際の詰将棋局面: 後手玉1一、先手金2二、持駒に金
    // 1二金打で詰み（1手詰め）
    const QString tsumeSfen = QStringLiteral("k8/G8/9/9/9/9/9/9/9 b G2r2b2g4s4n4l18p 1");
    QStringList sfenRecord = {tsumeSfen};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    // "position sfen ..." 形式であること
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));

    // 後手玉(k)のみ → 先手(b)が攻方
    QCOMPARE(extractTurn(result), QStringLiteral("b"));

    // 盤面部分が含まれていること
    QVERIFY(result.contains(QLatin1String("k")));
    QVERIFY(result.contains(QLatin1String("G")));
}

void TestTsumeSearch::buildForMate_midGamePosition_withMoves()
{
    // 中盤局面: buildPositionWithMoves でmoves付きのpositionコマンドを生成
    QStringList usiMoves = {
        QStringLiteral("7g7f"), QStringLiteral("3c3d"),
        QStringLiteral("2g2f"), QStringLiteral("8c8d")
    };
    const QString startCmd = QStringLiteral("startpos");

    const QString result = TsumePositionUtil::buildPositionWithMoves(&usiMoves, startCmd, 4);

    QCOMPARE(result, QStringLiteral("position startpos moves 7g7f 3c3d 2g2f 8c8d"));
}

void TestTsumeSearch::buildForMate_handicapPosition()
{
    // 駒落ち局面（飛車落ち）のSFEN
    const QString handicapSfen = QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    QStringList sfenRecord = {handicapSfen};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    // 両玉(K, k)あり → 元の手番 w を維持
    QCOMPARE(extractTurn(result), QStringLiteral("w"));
}

// ============================================================
// チェックメイトコマンド構築
// ============================================================

void TestTsumeSearch::buildCheckmateSendCommand_fromStartpos()
{
    // startpos からの局面構築 → "go mate infinite" を付加できる形式の確認
    QStringList usiMoves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    const QString startCmd = QStringLiteral("startpos");

    const QString positionCmd = TsumePositionUtil::buildPositionWithMoves(&usiMoves, startCmd, 2);

    // USI checkmate コマンド形式: position コマンド + "\ngo mate infinite"
    const QString checkmateCmd = positionCmd + QStringLiteral("\ngo mate infinite");

    QVERIFY(checkmateCmd.startsWith(QStringLiteral("position startpos moves ")));
    QVERIFY(checkmateCmd.endsWith(QStringLiteral("go mate infinite")));
    QVERIFY(checkmateCmd.contains(QStringLiteral("7g7f 3c3d")));
}

void TestTsumeSearch::buildCheckmateSendCommand_fromSfen()
{
    // SFEN局面からの checkmate コマンド構築
    const QString tsumeSfen = QStringLiteral("3sks3/9/4G4/9/9/9/9/9/9 b G2r2b4g4s4n4l18p 1");
    QStringList sfenRecord = {tsumeSfen};

    const QString positionCmd = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    // USI checkmate コマンド形式
    const QString checkmateCmd = positionCmd + QStringLiteral("\ngo mate infinite");

    QVERIFY(checkmateCmd.startsWith(QStringLiteral("position sfen ")));
    QVERIFY(checkmateCmd.endsWith(QStringLiteral("go mate infinite")));
}

// ============================================================
// 典型局面テスト
// ============================================================

void TestTsumeSearch::tsumePosition_oneMoveMate()
{
    // 1手詰め局面: 後手玉が1一(k)、先手金が2一(G)、持駒に金
    // 1二金打(G*1b)で詰み
    const QString sfen = QStringLiteral("k8/G8/9/9/9/9/9/9/9 b G2r2b2g4s4n4l18p 1");
    QStringList sfenRecord = {sfen};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    // 正しい position コマンドが構築できる
    QVERIFY(!result.isEmpty());
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));

    // 後手玉(k)のみ → 先手(b)手番
    QCOMPARE(extractTurn(result), QStringLiteral("b"));

    // 盤面に金(G)と後手玉(k)が含まれる
    QVERIFY(result.contains(QLatin1String("k")));
    QVERIFY(result.contains(QLatin1String("G")));

    // 持駒部分が含まれる
    QVERIFY(result.contains(QLatin1String("G2r2b2g4s4n4l18p")));
}

void TestTsumeSearch::tsumePosition_threeMoveMate()
{
    // 3手詰め局面: 後手玉が3一(k)、先手飛車が3九(R)、持駒に金
    // 3二金打→同玉→3一飛成 の3手で詰み（例）
    const QString sfen = QStringLiteral("2k6/9/9/9/9/9/9/9/2R6 b G2r2b4g4s4n4l18p 1");
    QStringList sfenRecord = {sfen};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    QVERIFY(!result.isEmpty());
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));

    // 後手玉(k)のみ → 先手(b)手番
    QCOMPARE(extractTurn(result), QStringLiteral("b"));

    // 盤面に飛車(R)と後手玉(k)が含まれる
    QVERIFY(result.contains(QLatin1String("R")));
    QVERIFY(result.contains(QLatin1String("k")));
}

void TestTsumeSearch::tsumePosition_noMatePosition()
{
    // 詰みなし局面でも position コマンドは正常に構築される
    const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1");
    QStringList sfenRecord = {sfen};

    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);

    // 両玉あり → position コマンドは構築可能（詰みの有無はエンジンが判断）
    QVERIFY(!result.isEmpty());
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));

    // 両玉あり → 元の手番(b)を維持
    QCOMPARE(extractTurn(result), QStringLiteral("b"));
}

// ============================================================
// エッジケース
// ============================================================

void TestTsumeSearch::buildForMate_emptyBoard_returnsEmpty()
{
    // 盤面データなし（全ソースが空）→ 空文字列
    QStringList emptyRecord;
    const QString result = TsumePositionUtil::buildPositionForMate(
        &emptyRecord, QString(), QStringList(), 0);

    QVERIFY(result.isEmpty());
}

void TestTsumeSearch::buildForMate_positionStrList_fallback()
{
    // sfenRecord が空、startSfenStr も空 → positionStrList にフォールバック
    QStringList emptyRecord;
    QStringList positionStrList = {
        QStringLiteral("position sfen 4k4/9/4G4/9/9/9/9/9/9 b G2r2b4g4s4n4l18p 1")
    };

    const QString result = TsumePositionUtil::buildPositionForMate(
        &emptyRecord, QString(), positionStrList, 0);

    QVERIFY(!result.isEmpty());
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));

    // 後手玉(k)のみ → 先手(b)手番
    QCOMPARE(extractTurn(result), QStringLiteral("b"));

    // "position sfen" プレフィックスが剥がされて再構築されること
    QVERIFY(result.contains(QLatin1String("4k4")));
    QVERIFY(result.contains(QLatin1String("G")));
}

QTEST_MAIN(TestTsumeSearch)
#include "tst_tsume_search.moc"
