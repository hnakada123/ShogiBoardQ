/// @file tst_joseki_repository.cpp
/// @brief 定跡リポジトリ・プレゼンターテスト

#include <QtTest>
#include <QTemporaryDir>

#include "josekirepository.h"
#include "josekipresenter.h"
#include "josekiwindow.h"  // JosekiMove 構造体

class TestJosekiRepository : public QObject
{
    Q_OBJECT

private:
    static JosekiMove makeMove(const QString &usi, int value = 0, int depth = 0,
                               int freq = 1, const QString &comment = {})
    {
        JosekiMove m;
        m.move = usi;
        m.nextMove = QStringLiteral("none");
        m.value = value;
        m.depth = depth;
        m.frequency = freq;
        m.comment = comment;
        return m;
    }

    static const QString kHirateSfen;
    static const QString kHirateSfenWithPly;

private slots:
    // --- CRUD ---
    void initialState_isEmpty();
    void addMove_addsToPosition();
    void addMove_multipleMovesToSamePosition();
    void updateMove_changesValues();
    void deleteMove_removesAtIndex();
    void deleteMove_removesPositionWhenEmpty();
    void removeMoveByUsi_removesMatching();
    void clear_resetsAll();
    void containsPosition_false_forUnknown();
    void movesForPosition_returnsEmptyForUnknown();

    // --- File I/O ---
    void saveAndLoad_roundTrip();
    void parseFromFile_invalidPath_failsGracefully();
    void serializeToFile_writesExpectedFormat();

    // --- Merge ---
    void registerMergeMove_newEntry_addsWithFrequency1();
    void registerMergeMove_existing_incrementsFrequency();
    void ensureSfenWithPly_registersOnce();

    // --- JosekiPresenter static ---
    void normalizeSfen_removesPlyNumber();
    void normalizeSfen_alreadyNormalized();
    void pieceToKanji_pawn();
    void pieceToKanji_promotedPawn();
    void pieceToKanji_allPieces();
    void hasDuplicateMove_true();
    void hasDuplicateMove_false();
};

const QString TestJosekiRepository::kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");

const QString TestJosekiRepository::kHirateSfenWithPly =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

// =============================================================
// CRUD テスト
// =============================================================

void TestJosekiRepository::initialState_isEmpty()
{
    JosekiRepository repo;
    QVERIFY(repo.isEmpty());
    QCOMPARE(repo.positionCount(), 0);
}

void TestJosekiRepository::addMove_addsToPosition()
{
    JosekiRepository repo;
    JosekiMove m = makeMove(QStringLiteral("7g7f"), 30, 10, 5);

    repo.addMove(kHirateSfen, m);

    QVERIFY(!repo.isEmpty());
    QCOMPARE(repo.positionCount(), 1);
    QVERIFY(repo.containsPosition(kHirateSfen));

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].value, 30);
    QCOMPARE(moves[0].depth, 10);
    QCOMPARE(moves[0].frequency, 5);
}

void TestJosekiRepository::addMove_multipleMovesToSamePosition()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("5g5f")));

    QCOMPARE(repo.positionCount(), 1);

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 3);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[1].move, QStringLiteral("2g2f"));
    QCOMPARE(moves[2].move, QStringLiteral("5g5f"));
}

void TestJosekiRepository::updateMove_changesValues()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 10, 5, 1, QStringLiteral("old")));

    repo.updateMove(kHirateSfen, QStringLiteral("7g7f"), 100, 20, 50, QStringLiteral("new comment"));

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].value, 100);
    QCOMPARE(moves[0].depth, 20);
    QCOMPARE(moves[0].frequency, 50);
    QCOMPARE(moves[0].comment, QStringLiteral("new comment"));
}

void TestJosekiRepository::deleteMove_removesAtIndex()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("5g5f")));

    repo.deleteMove(kHirateSfen, 1);  // 2g2f を削除

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 2);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[1].move, QStringLiteral("5g5f"));
}

void TestJosekiRepository::deleteMove_removesPositionWhenEmpty()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.ensureSfenWithPly(kHirateSfen, kHirateSfenWithPly);

    repo.deleteMove(kHirateSfen, 0);

    QVERIFY(!repo.containsPosition(kHirateSfen));
    QVERIFY(repo.isEmpty());
    // sfenWithPlyMap もクリアされている
    QVERIFY(repo.sfenWithPly(kHirateSfen).isEmpty());
}

void TestJosekiRepository::removeMoveByUsi_removesMatching()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));

    repo.removeMoveByUsi(kHirateSfen, QStringLiteral("7g7f"));

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("2g2f"));
}

void TestJosekiRepository::clear_resetsAll()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.ensureSfenWithPly(kHirateSfen, kHirateSfenWithPly);
    repo.mergeRegisteredMoves().insert(QStringLiteral("test:key"));

    repo.clear();

    QVERIFY(repo.isEmpty());
    QCOMPARE(repo.positionCount(), 0);
    QVERIFY(repo.sfenWithPly(kHirateSfen).isEmpty());
    QVERIFY(repo.mergeRegisteredMoves().isEmpty());
}

void TestJosekiRepository::containsPosition_false_forUnknown()
{
    JosekiRepository repo;
    QVERIFY(!repo.containsPosition(QStringLiteral("unknown/sfen b -")));
}

void TestJosekiRepository::movesForPosition_returnsEmptyForUnknown()
{
    JosekiRepository repo;
    const QList<JosekiMove> &moves = repo.movesForPosition(QStringLiteral("unknown/sfen b -"));
    QVERIFY(moves.isEmpty());
}

// =============================================================
// ファイル I/O テスト
// =============================================================

void TestJosekiRepository::saveAndLoad_roundTrip()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString filePath = tmpDir.path() + QStringLiteral("/test_joseki.db");

    // 保存用データを準備
    JosekiRepository repoSave;
    repoSave.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30, 10, 100, QStringLiteral("角道")));
    repoSave.addMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), -10, 8, 50));
    repoSave.ensureSfenWithPly(kHirateSfen, kHirateSfenWithPly);

    // 別の局面も追加
    const QString secondSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w -");
    repoSave.addMove(secondSfen, makeMove(QStringLiteral("3c3d"), 0, 5, 20));

    // 保存
    QString errorMsg;
    QVERIFY(repoSave.saveToFile(filePath, &errorMsg));
    QVERIFY(errorMsg.isEmpty());

    // 読み込み
    JosekiRepository repoLoad;
    QVERIFY(repoLoad.loadFromFile(filePath, &errorMsg));
    QVERIFY(errorMsg.isEmpty());

    // 検証
    QCOMPARE(repoLoad.positionCount(), 2);
    QVERIFY(repoLoad.containsPosition(kHirateSfen));
    QVERIFY(repoLoad.containsPosition(secondSfen));

    const QList<JosekiMove> &hirateMoves = repoLoad.movesForPosition(kHirateSfen);
    QCOMPARE(hirateMoves.size(), 2);
    QCOMPARE(hirateMoves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(hirateMoves[0].value, 30);
    QCOMPARE(hirateMoves[0].depth, 10);
    QCOMPARE(hirateMoves[0].frequency, 100);
    QCOMPARE(hirateMoves[0].comment, QStringLiteral("角道"));
    QCOMPARE(hirateMoves[1].move, QStringLiteral("2g2f"));
    QCOMPARE(hirateMoves[1].value, -10);
    QCOMPARE(hirateMoves[1].frequency, 50);

    const QList<JosekiMove> &secondMoves = repoLoad.movesForPosition(secondSfen);
    QCOMPARE(secondMoves.size(), 1);
    QCOMPARE(secondMoves[0].move, QStringLiteral("3c3d"));
}

void TestJosekiRepository::parseFromFile_invalidPath_failsGracefully()
{
    JosekiLoadResult result = JosekiRepository::parseFromFile(QStringLiteral("/nonexistent/path/joseki.db"));
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
}

void TestJosekiRepository::serializeToFile_writesExpectedFormat()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString filePath = tmpDir.path() + QStringLiteral("/format_test.db");

    QMap<QString, QList<JosekiMove>> data;
    data[kHirateSfen].append(makeMove(QStringLiteral("7g7f"), 30, 10, 100));

    QMap<QString, QString> sfenMap;
    sfenMap[kHirateSfen] = kHirateSfenWithPly;

    JosekiSaveResult result = JosekiRepository::serializeToFile(filePath, data, sfenMap);
    QVERIFY(result.success);
    QCOMPARE(result.savedCount, 1);

    // ファイル内容を検証
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = file.readAll();
    file.close();

    // ヘッダー確認
    QVERIFY(content.startsWith(QStringLiteral("#YANEURAOU-DB2016")));
    // SFEN行確認
    QVERIFY(content.contains(QStringLiteral("sfen ") + kHirateSfenWithPly));
    // 指し手行確認
    QVERIFY(content.contains(QStringLiteral("7g7f none 30 10 100")));
}

// =============================================================
// マージ操作テスト
// =============================================================

void TestJosekiRepository::registerMergeMove_newEntry_addsWithFrequency1()
{
    JosekiRepository repo;

    repo.registerMergeMove(kHirateSfen, kHirateSfenWithPly, QStringLiteral("7g7f"));

    QVERIFY(repo.containsPosition(kHirateSfen));
    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].frequency, 1);
    QCOMPARE(moves[0].nextMove, QStringLiteral("none"));

    // マージ登録済みセットに記録されている
    QVERIFY(repo.mergeRegisteredMoves().contains(kHirateSfen + QStringLiteral(":7g7f")));

    // sfenWithPly が登録されている
    QCOMPARE(repo.sfenWithPly(kHirateSfen), kHirateSfenWithPly);
}

void TestJosekiRepository::registerMergeMove_existing_incrementsFrequency()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30, 10, 5));

    repo.registerMergeMove(kHirateSfen, kHirateSfenWithPly, QStringLiteral("7g7f"));

    const QList<JosekiMove> &moves = repo.movesForPosition(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].frequency, 6);  // 5 + 1
}

void TestJosekiRepository::ensureSfenWithPly_registersOnce()
{
    JosekiRepository repo;
    const QString ply1 = kHirateSfenWithPly;
    const QString ply2 = kHirateSfen + QStringLiteral(" 99");

    repo.ensureSfenWithPly(kHirateSfen, ply1);
    repo.ensureSfenWithPly(kHirateSfen, ply2);  // 二回目は無視される

    QCOMPARE(repo.sfenWithPly(kHirateSfen), ply1);
}

// =============================================================
// JosekiPresenter static メソッドテスト
// =============================================================

void TestJosekiRepository::normalizeSfen_removesPlyNumber()
{
    const QString input = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString expected = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    QCOMPARE(JosekiPresenter::normalizeSfen(input), expected);
}

void TestJosekiRepository::normalizeSfen_alreadyNormalized()
{
    const QString input = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    QCOMPARE(JosekiPresenter::normalizeSfen(input), input);
}

void TestJosekiRepository::pieceToKanji_pawn()
{
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('P')), QStringLiteral("歩"));
}

void TestJosekiRepository::pieceToKanji_promotedPawn()
{
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('P'), true), QStringLiteral("と"));
}

void TestJosekiRepository::pieceToKanji_allPieces()
{
    // 非成駒
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('P')), QStringLiteral("歩"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('L')), QStringLiteral("香"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('N')), QStringLiteral("桂"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('S')), QStringLiteral("銀"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('G')), QStringLiteral("金"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('B')), QStringLiteral("角"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('R')), QStringLiteral("飛"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('K')), QStringLiteral("玉"));

    // 成駒
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('P'), true), QStringLiteral("と"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('L'), true), QStringLiteral("杏"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('N'), true), QStringLiteral("圭"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('S'), true), QStringLiteral("全"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('G'), true), QStringLiteral("金"));  // 金は成れない
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('B'), true), QStringLiteral("馬"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('R'), true), QStringLiteral("龍"));

    // 小文字でも動作する
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('p')), QStringLiteral("歩"));
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('r'), true), QStringLiteral("龍"));

    // 未知の文字
    QCOMPARE(JosekiPresenter::pieceToKanji(QChar('X')), QStringLiteral("?"));
}

void TestJosekiRepository::hasDuplicateMove_true()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));

    JosekiPresenter presenter(&repo);
    QVERIFY(presenter.hasDuplicateMove(kHirateSfen, QStringLiteral("7g7f")));
}

void TestJosekiRepository::hasDuplicateMove_false()
{
    JosekiRepository repo;
    repo.addMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));

    JosekiPresenter presenter(&repo);
    QVERIFY(!presenter.hasDuplicateMove(kHirateSfen, QStringLiteral("2g2f")));
    // 未登録局面でも false
    QVERIFY(!presenter.hasDuplicateMove(QStringLiteral("unknown b -"), QStringLiteral("7g7f")));
}

QTEST_MAIN(TestJosekiRepository)
#include "tst_joseki_repository.moc"
