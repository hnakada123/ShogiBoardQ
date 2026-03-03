/// @file tst_tsumeshogi_generator.cpp
/// @brief 詰将棋局面生成テスト（TsumeshogiPositionGenerator + TsumePositionUtil）

#include <QtTest>

#include "tsumeshogipositiongenerator.h"
#include "tsumepositionutil.h"
#include "threadtypes.h"

class TestTsumeshogiGenerator : public QObject
{
    Q_OBJECT

private:
    /// SFEN文字列の基本バリデーション（スペース区切りで4トークン: 盤面 手番 持駒 手数）
    static bool isValidSfenFormat(const QString& sfen)
    {
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() != 4) return false;
        if (tokens[1] != QLatin1String("b") && tokens[1] != QLatin1String("w")) return false;
        return true;
    }

    /// SFEN盤面部分から指定文字の出現数をカウント
    static int countPieceInBoard(const QString& boardPart, QChar piece)
    {
        int count = 0;
        for (const QChar ch : boardPart) {
            if (ch == piece) ++count;
        }
        return count;
    }

    /// SFEN盤面+持駒から攻方（先手=大文字）の駒数をカウント（玉除く）
    static int countAttackPieces(const QString& sfen)
    {
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() < 3) return 0;

        int count = 0;

        // 盤面部分の大文字駒をカウント（K除く、成駒の+は無視）
        const QString& board = tokens[0];
        for (const QChar ch : board) {
            if (ch.isUpper() && ch != QLatin1Char('K')) {
                ++count;
            }
        }

        // 持駒部分の大文字駒をカウント
        const QString& hand = tokens[2];
        if (hand != QLatin1String("-")) {
            int num = 0;
            for (const QChar ch : hand) {
                if (ch.isDigit()) {
                    num = num * 10 + ch.digitValue();
                } else if (ch.isUpper()) {
                    count += (num > 0) ? num : 1;
                    num = 0;
                } else {
                    num = 0;
                }
            }
        }
        return count;
    }

    /// SFEN盤面+持駒から守方（後手=小文字）の駒数をカウント（玉除く）
    static int countDefendPieces(const QString& sfen)
    {
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() < 3) return 0;

        int count = 0;

        // 盤面部分の小文字駒をカウント（k除く、成駒の+は無視）
        const QString& board = tokens[0];
        for (const QChar ch : board) {
            if (ch.isLower() && ch != QLatin1Char('k')) {
                ++count;
            }
        }

        // 持駒部分の小文字駒をカウント（kは持駒にはない）
        const QString& hand = tokens[2];
        if (hand != QLatin1String("-")) {
            int num = 0;
            for (const QChar ch : hand) {
                if (ch.isDigit()) {
                    num = num * 10 + ch.digitValue();
                } else if (ch.isLower()) {
                    count += (num > 0) ? num : 1;
                    num = 0;
                } else {
                    num = 0;
                }
            }
        }
        return count;
    }

private slots:
    // --- TsumePositionUtil::buildPositionWithMoves ---
    void nullMoves_returnsStartpos();
    void emptyMoves_returnsStartpos();
    void zeroIndex_returnsStartpos();
    void withMoves_returnsCorrectPositionCmd();
    void sfenStart_returnsSfenPosition();
    void indexBeyondMoves_clampsToSize();

    // --- TsumePositionUtil::buildPositionForMate ---
    void sfenRecord_returnsPositionWithForcedTurn();
    void onlyDefenderKing_turnIsWhite();
    void onlyAttackerKing_turnIsBlack();
    void bothKings_preservesTurn();
    void allSourcesEmpty_returnsEmpty();
    void fallbackToStartSfen();

    // --- TsumeshogiPositionGenerator ---
    void generate_returnsValidSfen();
    void generate_noAttackerKing();
    void generate_hasDefenderKing();
    void generate_respectsMaxAttackPieces();
    void generate_respectsMaxDefendPieces();
    void generateBatch_returnsRequestedCount();
    void generateBatch_cancellation();
    void generate_multipleCalls_produceDifferentResults();
};

// ============================================================
// TsumePositionUtil::buildPositionWithMoves テスト
// ============================================================

void TestTsumeshogiGenerator::nullMoves_returnsStartpos()
{
    const QString result = TsumePositionUtil::buildPositionWithMoves(nullptr, QString(), 0);
    QCOMPARE(result, QStringLiteral("position startpos"));
}

void TestTsumeshogiGenerator::emptyMoves_returnsStartpos()
{
    QStringList empty;
    const QString result = TsumePositionUtil::buildPositionWithMoves(&empty, QStringLiteral("startpos"), 1);
    QCOMPARE(result, QStringLiteral("position startpos"));
}

void TestTsumeshogiGenerator::zeroIndex_returnsStartpos()
{
    QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    const QString result = TsumePositionUtil::buildPositionWithMoves(&moves, QStringLiteral("startpos"), 0);
    QCOMPARE(result, QStringLiteral("position startpos"));
}

void TestTsumeshogiGenerator::withMoves_returnsCorrectPositionCmd()
{
    QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")};
    const QString result = TsumePositionUtil::buildPositionWithMoves(&moves, QStringLiteral("startpos"), 2);
    QCOMPARE(result, QStringLiteral("position startpos moves 7g7f 3c3d"));
}

void TestTsumeshogiGenerator::sfenStart_returnsSfenPosition()
{
    const QString sfenStr = QStringLiteral("sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    QStringList moves = {QStringLiteral("7g7f")};
    const QString result = TsumePositionUtil::buildPositionWithMoves(&moves, sfenStr, 1);
    QCOMPARE(result, QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 7g7f"));
}

void TestTsumeshogiGenerator::indexBeyondMoves_clampsToSize()
{
    QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    const QString result = TsumePositionUtil::buildPositionWithMoves(&moves, QStringLiteral("startpos"), 100);
    // selectedIndex=100 but only 2 moves → clamped to 2
    QCOMPARE(result, QStringLiteral("position startpos moves 7g7f 3c3d"));
}

// ============================================================
// TsumePositionUtil::buildPositionForMate テスト
// ============================================================

void TestTsumeshogiGenerator::sfenRecord_returnsPositionWithForcedTurn()
{
    // sfenRecord に SFEN が入っている場合
    QStringList sfenRecord = {
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
    };
    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    // 両玉あり → 元の手番維持 (b)
    QVERIFY(result.contains(QLatin1String(" b ")));
}

void TestTsumeshogiGenerator::onlyDefenderKing_turnIsWhite()
{
    // 受方玉のみ (大文字 K) → 先手(b)が攻方 → 手番は w...
    // いや、ロジック確認: cntK>0 かつ cntk==0 → return 'w'
    // K = 受方玉（大文字=先手の駒）, k = 攻方玉（小文字=後手の駒）
    // 詰将棋の慣例: 受方玉(K)のみあり、攻方(先手)が詰ます → 手番 b が返るのでは？
    // ロジック: (cntK > 0) ^ (cntk > 0) = true で cntk == 0 → return 'w'
    // つまり先手玉(K)しかない場合、攻方は後手(w)と推定される
    QStringList sfenRecord = {
        QStringLiteral("9/9/9/9/4K4/9/9/9/9 b - 1")
    };
    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    QVERIFY(result.contains(QLatin1String(" w ")));
}

void TestTsumeshogiGenerator::onlyAttackerKing_turnIsBlack()
{
    // 攻方玉のみ (小文字 k) → 手番 b
    // ロジック: cntK == 0, cntk > 0 → return 'b'
    QStringList sfenRecord = {
        QStringLiteral("9/9/9/9/4k4/9/9/9/9 b - 1")
    };
    const QString result = TsumePositionUtil::buildPositionForMate(&sfenRecord, QString(), {}, 0);
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    QVERIFY(result.contains(QLatin1String(" b ")));
}

void TestTsumeshogiGenerator::bothKings_preservesTurn()
{
    // 両玉あり → 元の手番を維持
    QStringList sfenRecordB = {
        QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1")
    };
    const QString resultB = TsumePositionUtil::buildPositionForMate(&sfenRecordB, QString(), {}, 0);
    QVERIFY(resultB.contains(QLatin1String(" b ")));

    QStringList sfenRecordW = {
        QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1")
    };
    const QString resultW = TsumePositionUtil::buildPositionForMate(&sfenRecordW, QString(), {}, 0);
    QVERIFY(resultW.contains(QLatin1String(" w ")));
}

void TestTsumeshogiGenerator::allSourcesEmpty_returnsEmpty()
{
    QStringList emptyList;
    const QString result = TsumePositionUtil::buildPositionForMate(&emptyList, QString(), {}, 0);
    QVERIFY(result.isEmpty());
}

void TestTsumeshogiGenerator::fallbackToStartSfen()
{
    // sfenRecord が空 → startSfenStr にフォールバック
    QStringList emptyList;
    const QString startSfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1");
    const QString result = TsumePositionUtil::buildPositionForMate(&emptyList, startSfen, {}, 0);
    QVERIFY(result.startsWith(QStringLiteral("position sfen ")));
    QVERIFY(!result.isEmpty());
}

// ============================================================
// TsumeshogiPositionGenerator テスト
// ============================================================

void TestTsumeshogiGenerator::generate_returnsValidSfen()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;
    gen.setSettings(settings);

    const QString sfen = gen.generate();
    QVERIFY2(!sfen.isEmpty(), "generate() should return non-empty SFEN");
    QVERIFY2(isValidSfenFormat(sfen), qPrintable(QStringLiteral("Invalid SFEN format: %1").arg(sfen)));
}

void TestTsumeshogiGenerator::generate_noAttackerKing()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;
    gen.setSettings(settings);

    // 複数回テストして確認（ランダム生成のため）
    for (int i = 0; i < 20; ++i) {
        const QString sfen = gen.generate();
        QVERIFY(!sfen.isEmpty());
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QVERIFY(tokens.size() >= 1);
        // 盤面部分に大文字 K（先手の玉）がないことを確認
        const int kingCount = countPieceInBoard(tokens[0], QLatin1Char('K'));
        QCOMPARE(kingCount, 0);
    }
}

void TestTsumeshogiGenerator::generate_hasDefenderKing()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;
    gen.setSettings(settings);

    for (int i = 0; i < 20; ++i) {
        const QString sfen = gen.generate();
        QVERIFY(!sfen.isEmpty());
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QVERIFY(tokens.size() >= 1);
        // 盤面部分に小文字 k（後手の玉）が1つあることを確認
        const int kingCount = countPieceInBoard(tokens[0], QLatin1Char('k'));
        QCOMPARE(kingCount, 1);
    }
}

void TestTsumeshogiGenerator::generate_respectsMaxAttackPieces()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 3;
    settings.maxDefendPieces = 0;
    settings.addRemainingToDefenderHand = false;
    gen.setSettings(settings);

    for (int i = 0; i < 30; ++i) {
        const QString sfen = gen.generate();
        QVERIFY(!sfen.isEmpty());
        const int attackCount = countAttackPieces(sfen);
        QVERIFY2(attackCount <= settings.maxAttackPieces,
                 qPrintable(QStringLiteral("Attack pieces %1 > max %2 in: %3")
                            .arg(attackCount).arg(settings.maxAttackPieces).arg(sfen)));
    }
}

void TestTsumeshogiGenerator::generate_respectsMaxDefendPieces()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 2;
    settings.addRemainingToDefenderHand = false;
    gen.setSettings(settings);

    for (int i = 0; i < 30; ++i) {
        const QString sfen = gen.generate();
        QVERIFY(!sfen.isEmpty());
        const int defendCount = countDefendPieces(sfen);
        QVERIFY2(defendCount <= settings.maxDefendPieces,
                 qPrintable(QStringLiteral("Defend pieces %1 > max %2 in: %3")
                            .arg(defendCount).arg(settings.maxDefendPieces).arg(sfen)));
    }
}

void TestTsumeshogiGenerator::generateBatch_returnsRequestedCount()
{
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;

    auto cancelFlag = makeCancelFlag();
    const int count = 10;

    const QStringList results = TsumeshogiPositionGenerator::generateBatch(settings, count, cancelFlag);
    QCOMPARE(results.size(), count);

    // 全結果が有効なSFEN形式であることを確認
    for (const QString& sfen : std::as_const(results)) {
        QVERIFY2(isValidSfenFormat(sfen), qPrintable(QStringLiteral("Invalid SFEN in batch: %1").arg(sfen)));
    }
}

void TestTsumeshogiGenerator::generateBatch_cancellation()
{
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;

    auto cancelFlag = makeCancelFlag();
    // 即座にキャンセル
    cancelFlag->store(true);

    const QStringList results = TsumeshogiPositionGenerator::generateBatch(settings, 100, cancelFlag);
    // キャンセルにより全数未満になる（0も許容）
    QVERIFY2(results.size() < 100,
             qPrintable(QStringLiteral("Expected fewer than 100 results after cancel, got %1").arg(results.size())));
}

void TestTsumeshogiGenerator::generate_multipleCalls_produceDifferentResults()
{
    TsumeshogiPositionGenerator gen;
    TsumeshogiPositionGenerator::Settings settings;
    settings.maxAttackPieces = 4;
    settings.maxDefendPieces = 1;
    gen.setSettings(settings);

    QSet<QString> results;
    constexpr int kTrials = 20;
    for (int i = 0; i < kTrials; ++i) {
        results.insert(gen.generate());
    }
    // ランダム生成なので20回で少なくとも2種類の異なる結果が出るはず
    QVERIFY2(results.size() >= 2,
             qPrintable(QStringLiteral("Expected at least 2 distinct results from %1 trials, got %2")
                        .arg(kTrials).arg(results.size())));
}

QTEST_MAIN(TestTsumeshogiGenerator)
#include "tst_tsumeshogi_generator.moc"
