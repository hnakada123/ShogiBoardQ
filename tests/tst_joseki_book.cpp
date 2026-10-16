/// @file tst_joseki_book.cpp
/// @brief 定跡のデータ（JosekiBook）とファイルの読み書き、定跡ウィンドウの表記のテスト

#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include "sfenreplay.h"
#include "sfenutils.h"
#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
#endif

#include "josekibook.h"
#include "josekibooksession.h"
#include "josekitablemodel.h"
#include "josekitypes.h"

class TestJosekiBook : public QObject
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
    void removeMove_removesMatching();
    void removeMove_removesPositionWhenEmpty();
    void removeMove_byUsi();
    void clear_resetsAll();
    void find_nullForUnknown();
    void movesAt_returnsEmptyForUnknown();

    // --- File I/O ---
    void saveAndLoad_roundTrip();
    void load_invalidPath_failsGracefully();
    void save_writesExpectedFormat();
    void saveAndLoad_emptyBook();
    void failedSave_preservesOriginal();
    void saveFailureChild();
    void mergeFailure_canRetryWithoutDoubleCounting();
    void japaneseMove_usesPositionTurn();
    void japaneseMove_noneReply();

    // --- Merge ---
    void registerMergeMove_newEntry_addsWithFrequency1();
    void registerMergeMove_existing_incrementsFrequency();
    void putMove_keepsFirstRegisteredSfen();

    // --- 局面のキー・表記・重複（JosekiBookSession・JosekiTableModel） ---
    void normalizeSfen_removesPlyNumber();
    void normalizeSfen_alreadyNormalized();
    void usiMoveToJapanese_promotedPiecesUseRecordNames();
    void hasDuplicateMove_true();
    void hasDuplicateMove_false();
};

const QString TestJosekiBook::kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");

const QString TestJosekiBook::kHirateSfenWithPly =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

// =============================================================
// CRUD テスト
// =============================================================

void TestJosekiBook::initialState_isEmpty()
{
    JosekiBook repo;
    QVERIFY(repo.isEmpty());
    QCOMPARE(repo.positionCount(), 0);
}

void TestJosekiBook::addMove_addsToPosition()
{
    JosekiBook repo;
    JosekiMove m = makeMove(QStringLiteral("7g7f"), 30, 10, 5);

    repo.putMove(kHirateSfen, m);

    QVERIFY(!repo.isEmpty());
    QCOMPARE(repo.positionCount(), 1);
    QVERIFY(repo.find(kHirateSfen));

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].value, 30);
    QCOMPARE(moves[0].depth, 10);
    QCOMPARE(moves[0].frequency, 5);
}

void TestJosekiBook::addMove_multipleMovesToSamePosition()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("5g5f")));

    QCOMPARE(repo.positionCount(), 1);

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 3);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[1].move, QStringLiteral("2g2f"));
    QCOMPARE(moves[2].move, QStringLiteral("5g5f"));
}

void TestJosekiBook::updateMove_changesValues()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 10, 5, 1, QStringLiteral("old")));

    QVERIFY(repo.updateMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 100, 20, 50, QStringLiteral("new comment"))));

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].value, 100);
    QCOMPARE(moves[0].depth, 20);
    QCOMPARE(moves[0].frequency, 50);
    QCOMPARE(moves[0].comment, QStringLiteral("new comment"));
}

void TestJosekiBook::removeMove_removesMatching()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("5g5f")));

    QVERIFY(repo.removeMove(kHirateSfen, QStringLiteral("2g2f")));

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 2);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[1].move, QStringLiteral("5g5f"));
}

void TestJosekiBook::removeMove_removesPositionWhenEmpty()
{
    JosekiBook repo;
    repo.putMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f")));

    QVERIFY(repo.removeMove(kHirateSfen, QStringLiteral("7g7f")));

    // 最後の定跡手を消すと局面（登録したときの SFEN も）も消える
    QVERIFY(!repo.find(kHirateSfen));
    QVERIFY(repo.isEmpty());
    QVERIFY(!repo.removeMove(kHirateSfen, QStringLiteral("7g7f")));
}

void TestJosekiBook::removeMove_byUsi()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f")));

    QVERIFY(repo.removeMove(kHirateSfen, QStringLiteral("7g7f")));

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("2g2f"));
}

void TestJosekiBook::clear_resetsAll()
{
    JosekiBook repo;
    repo.putMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f")));
    repo.registerMergeMove(kHirateSfenWithPly, QStringLiteral("2g2f"));

    repo.clear();

    QVERIFY(repo.isEmpty());
    QCOMPARE(repo.positionCount(), 0);
    QVERIFY(!repo.find(kHirateSfen));
    QVERIFY(repo.mergeRegistered().isEmpty());
}

void TestJosekiBook::find_nullForUnknown()
{
    JosekiBook repo;
    QVERIFY(!repo.find(QStringLiteral("unknown/sfen b -")));
}

void TestJosekiBook::movesAt_returnsEmptyForUnknown()
{
    JosekiBook repo;
    const QList<JosekiMove> &moves = repo.movesAt(QStringLiteral("unknown/sfen b -"));
    QVERIFY(moves.isEmpty());
}

// =============================================================
// ファイル I/O テスト
// =============================================================

void TestJosekiBook::saveAndLoad_roundTrip()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString filePath = tmpDir.path() + QStringLiteral("/test_joseki.db");

    // 保存用データを準備
    JosekiBook repoSave;
    repoSave.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30, 10, 100, QStringLiteral("角道")));
    repoSave.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), -10, 8, 50));

    // 別の局面も追加
    const QString secondSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w -");
    repoSave.putMove(secondSfen, makeMove(QStringLiteral("3c3d"), 0, 5, 20));

    // 保存
    const JosekiSaveResult saved = JosekiBookFile::save(filePath, repoSave);
    QVERIFY(saved.success);
    QVERIFY(saved.errorMessage.isEmpty());

    // 読み込み
    const JosekiLoadResult loaded = JosekiBookFile::load(filePath);
    QVERIFY(loaded.success);
    const JosekiBook &repoLoad = loaded.book;
    QCOMPARE(repoLoad.find(kHirateSfen)->sfen, kHirateSfen + QStringLiteral(" 1"));

    // 検証
    QCOMPARE(repoLoad.positionCount(), 2);
    QVERIFY(repoLoad.find(kHirateSfen));
    QVERIFY(repoLoad.find(secondSfen));

    const QList<JosekiMove> &hirateMoves = repoLoad.movesAt(kHirateSfen);
    QCOMPARE(hirateMoves.size(), 2);
    QCOMPARE(hirateMoves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(hirateMoves[0].value, 30);
    QCOMPARE(hirateMoves[0].depth, 10);
    QCOMPARE(hirateMoves[0].frequency, 100);
    QCOMPARE(hirateMoves[0].comment, QStringLiteral("角道"));
    QCOMPARE(hirateMoves[1].move, QStringLiteral("2g2f"));
    QCOMPARE(hirateMoves[1].value, -10);
    QCOMPARE(hirateMoves[1].frequency, 50);

    const QList<JosekiMove> &secondMoves = repoLoad.movesAt(secondSfen);
    QCOMPARE(secondMoves.size(), 1);
    QCOMPARE(secondMoves[0].move, QStringLiteral("3c3d"));
}

void TestJosekiBook::load_invalidPath_failsGracefully()
{
    JosekiLoadResult result = JosekiBookFile::load(QStringLiteral("/nonexistent/path/joseki.db"));
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
}

void TestJosekiBook::save_writesExpectedFormat()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString filePath = tmpDir.path() + QStringLiteral("/format_test.db");

    JosekiBook book;
    book.putMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f"), 30, 10, 100));

    JosekiSaveResult result = JosekiBookFile::save(filePath, book);
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

void TestJosekiBook::registerMergeMove_newEntry_addsWithFrequency1()
{
    JosekiBook repo;

    repo.registerMergeMove(kHirateSfenWithPly, QStringLiteral("7g7f"));

    QVERIFY(repo.find(kHirateSfen));
    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].frequency, 1);
    QCOMPARE(moves[0].nextMove, QStringLiteral("none"));

    // マージ登録済みセットに記録されている
    QVERIFY(repo.isMergeRegistered(kHirateSfenWithPly, QStringLiteral("7g7f")));
    QVERIFY(repo.mergeRegistered().contains({kHirateSfen, QStringLiteral("7g7f")}));

    // 局面は登録したときの手数付きの SFEN で覚える
    QCOMPARE(repo.find(kHirateSfen)->sfen, kHirateSfenWithPly);
}

void TestJosekiBook::registerMergeMove_existing_incrementsFrequency()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30, 10, 5));

    repo.registerMergeMove(kHirateSfenWithPly, QStringLiteral("7g7f"));

    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].frequency, 6);  // 5 + 1
}

void TestJosekiBook::putMove_keepsFirstRegisteredSfen()
{
    JosekiBook repo;
    const QString ply1 = kHirateSfenWithPly;
    const QString ply2 = kHirateSfen + QStringLiteral(" 99");

    repo.putMove(ply1, makeMove(QStringLiteral("7g7f")));
    repo.putMove(ply2, makeMove(QStringLiteral("2g2f")));  // 局面の SFEN は最初に登録したもの

    QCOMPARE(repo.find(kHirateSfen)->sfen, ply1);
    QCOMPARE(repo.movesAt(ply2).size(), 2);
}

// =============================================================
// 局面のキー・表記・重複のテスト
// =============================================================

void TestJosekiBook::normalizeSfen_removesPlyNumber()
{
    const QString input = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString expected = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    QCOMPARE(JosekiBookSession::positionKey(input), expected);
}

void TestJosekiBook::normalizeSfen_alreadyNormalized()
{
    const QString input = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    QCOMPARE(JosekiBookSession::positionKey(input), input);
}

void TestJosekiBook::usiMoveToJapanese_promotedPiecesUseRecordNames()
{
    // 成香・成桂・成銀は棋譜欄と同じく2文字で書く
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("4f5e"), QStringLiteral("4k4/9/9/9/9/5+L3/9/9/4K4 b - 1")),
             QStringLiteral("▲５五成香(46)"));
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("S*5e"), QStringLiteral("4k4/9/9/9/9/9/9/4S4/4K4 w s 1")),
             QStringLiteral("△５五銀打"));
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("7c6b+"), QStringLiteral("4k4/9/2S6/9/9/9/9/9/4K4 b - 1")),
             QStringLiteral("▲６二銀成(73)"));
}

void TestJosekiBook::hasDuplicateMove_true()
{
    JosekiBookSession session;
    session.addMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f")));
    QVERIFY(session.hasMove(kHirateSfen, QStringLiteral("7g7f")));
    QVERIFY(session.isModified());
}

void TestJosekiBook::hasDuplicateMove_false()
{
    JosekiBookSession session;
    session.addMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f")));
    QVERIFY(!session.hasMove(kHirateSfen, QStringLiteral("2g2f")));
    // 未登録局面でも false
    QVERIFY(!session.hasMove(QStringLiteral("unknown b -"), QStringLiteral("7g7f")));
}

void TestJosekiBook::saveAndLoad_emptyBook()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    JosekiBook repo;
    const QString path = dir.filePath(QStringLiteral("empty.db"));
    QVERIFY(JosekiBookFile::save(path, repo).success);
    QVERIFY(JosekiBookFile::load(path).success);
    QVERIFY(JosekiBookFile::load(path).book.isEmpty());
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f")));
    QVERIFY(repo.removeMove(kHirateSfen, QStringLiteral("7g7f")));
    QVERIFY(JosekiBookFile::save(path, repo).success);
    QVERIFY(JosekiBookFile::load(path).book.isEmpty());

    QFile invalid(dir.filePath(QStringLiteral("invalid.db")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("#YANEURAOU-DB2016 1.00\ninvalid data\n");
    invalid.close();
    QVERIFY(!JosekiBookFile::load(invalid.fileName()).success);
}

void TestJosekiBook::failedSave_preservesOriginal()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("book.db"));
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30, 10, 5, QString(8192, QLatin1Char('a'))));
    QVERIFY(JosekiBookFile::save(path, repo).success);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray original = file.readAll();
    file.close();

    QProcess child;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SHOGIBOARDQ_SAVE_FAILURE_PATH"), path);
    child.setProcessEnvironment(environment);
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("saveFailureChild")});
    QVERIFY(child.waitForFinished(10000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QVERIFY2(child.exitCode() == 0, child.readAllStandardOutput().constData());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
#else
    QSKIP("RLIMIT_FSIZE is only available on Unix");
#endif
}

void TestJosekiBook::saveFailureChild()
{
#ifdef Q_OS_UNIX
    const QString path = qEnvironmentVariable("SHOGIBOARDQ_SAVE_FAILURE_PATH");
    if (path.isEmpty()) QSKIP("Only run in the write-failure subprocess");
    struct rlimit originalLimit;
    QVERIFY(getrlimit(RLIMIT_FSIZE, &originalLimit) == 0);
    struct rlimit limit = originalLimit;
    limit.rlim_cur = 4096;
    const auto originalHandler = std::signal(SIGXFSZ, SIG_IGN);
    QVERIFY(setrlimit(RLIMIT_FSIZE, &limit) == 0);
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 99, 32, 1, QString(8192, QLatin1Char('b'))));
    const JosekiSaveResult result = JosekiBookFile::save(path, repo);
    const bool saved = result.success;
    const QString error = result.errorMessage;
    const int restored = setrlimit(RLIMIT_FSIZE, &originalLimit);
    std::signal(SIGXFSZ, originalHandler);
    QCOMPARE(restored, 0);
    QVERIFY(!saved);
    QVERIFY(!error.isEmpty());
#else
    QSKIP("RLIMIT_FSIZE is only available on Unix");
#endif
}

void TestJosekiBook::mergeFailure_canRetryWithoutDoubleCounting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    JosekiBookSession session;
    session.addMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f"), 30, 10, 5));
    const QString directory = dir.filePath(QStringLiteral("book-dir"));
    QVERIFY(QDir().mkpath(directory));
    const QString path = directory + QStringLiteral("/book.db");
    QVERIFY(session.save(path));
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().rmdir(directory));

    // 保存できなければ何も変えない（登録済みの印も付けない）
    const KifuMergeEntry entry{1, kHirateSfenWithPly, QStringLiteral("7g7f"), {}, false};
    QString error;
    QVERIFY(!session.registerMergeMoves({entry}, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(session.movesAt(kHirateSfen).first().frequency, 5);
    QVERIFY(session.mergeRegisteredMoves().isEmpty());

    // 保存できるようになれば登録でき、同じ手をもう一度登録しても数えない
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(session.registerMergeMoves({entry}));
    QVERIFY(session.registerMergeMoves({entry}));
    QCOMPARE(session.movesAt(kHirateSfen).first().frequency, 6);
    QVERIFY(!session.isModified());
    const JosekiLoadResult loaded = JosekiBookFile::load(path);
    QVERIFY(loaded.success);
    QCOMPARE(loaded.book.movesAt(kHirateSfen).first().frequency, 6);
}

void TestJosekiBook::japaneseMove_usesPositionTurn()
{
    const QString start = QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("3c3d"), start), QStringLiteral("△３四歩(33)"));
    const QString next = SfenReplay::positionAfter(start, QStringLiteral("3c3d"));
    QVERIFY(!next.isEmpty());
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("7g7f"), next), QStringLiteral("▲７六歩(77)"));
}

void TestJosekiBook::japaneseMove_noneReply()
{
    const QString hirate = SfenUtils::hirateSfen();
    QCOMPARE(JosekiTableModel::moveText(QStringLiteral("none"), hirate), QStringLiteral("なし"));
    QVERIFY(JosekiTableModel::moveText(QString(), hirate).isEmpty());
}

QTEST_MAIN(TestJosekiBook)
#include "tst_joseki_book.moc"
