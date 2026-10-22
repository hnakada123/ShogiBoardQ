/// @file tst_joseki_book.cpp
/// @brief 定跡のデータ（JosekiBook）とファイルの読み書き、定跡ウィンドウの表記のテスト

#include <QtTest>
#include <QSignalSpy>
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
#include "josekimergeentries.h"
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
    void leadingMove_ordersLikeYaneuraOu();
    void reorder_moveToAndSort();
    void session_registerEnginePv();
    void session_valueOfScore();

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
    void tableModel_showsFrequencyRatio();
    void tableModel_showsDerivedReplyInGray();
    void mergeEntries_readOtherKifuFormats();

    // --- Merge ---
    void registerMergeMove_newEntry_addsWithFrequency1();
    void registerMergeMove_existing_incrementsFrequency();
    void putMove_keepsFirstRegisteredSfen();
    void putMove_replacesInPlace();
    void load_keepsRegistrationsPerPly();
    void mergeFrom_combinesBooks();
    void session_startMergeBook();
    void load_duplicateMoveAndLegacyHands();
    void packedMoves_keepEveryNotation();
    void comments_updateAndRemove();

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
    QVERIFY(repo.contains(kHirateSfen));

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
    QVERIFY(!repo.contains(kHirateSfen));
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
    QVERIFY(!repo.contains(kHirateSfen));
    QVERIFY(repo.mergeRegistered().isEmpty());
}

void TestJosekiBook::find_nullForUnknown()
{
    JosekiBook repo;
    QVERIFY(!repo.contains(QStringLiteral("unknown/sfen b -")));
}

void TestJosekiBook::movesAt_returnsEmptyForUnknown()
{
    JosekiBook repo;
    const QList<JosekiMove> &moves = repo.movesAt(QStringLiteral("unknown/sfen b -"));
    QVERIFY(moves.isEmpty());
}

/// 定跡手の並べ替え: 位置を移す・出現頻度や評価値の順に並べる（同じなら元の順）。並びが変わらなければ false
void TestJosekiBook::reorder_moveToAndSort()
{
    JosekiBook book;
    const auto order = [&book](const QString &sfen) {
        QStringList moves;
        const QList<JosekiMove> found = book.movesAt(sfen);
        for (const JosekiMove &move : found) moves << move.move;
        return moves;
    };
    book.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 10, 0, 5));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 30, 0, 9));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("5g5f"), 30, 0, 1));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("6g6f"), -5, 0, 9));

    QVERIFY(book.moveTo(kHirateSfen, QStringLiteral("5g5f"), 0));
    QCOMPARE(order(kHirateSfen), (QStringList{"5g5f", "7g7f", "2g2f", "6g6f"}));
    QVERIFY(!book.moveTo(kHirateSfen, QStringLiteral("5g5f"), -3));  // 先頭より前は先頭
    QVERIFY(book.moveTo(kHirateSfen, QStringLiteral("7g7f"), 99));   // 末尾より後は末尾
    QCOMPARE(order(kHirateSfen), (QStringList{"5g5f", "2g2f", "6g6f", "7g7f"}));
    QVERIFY(!book.moveTo(kHirateSfen, QStringLiteral("8h2b+"), 0));

    // 出現頻度の順（同じなら評価値の高い順）
    QVERIFY(book.sortMoves(kHirateSfen, JosekiBook::SortKey::Frequency));
    QCOMPARE(order(kHirateSfen), (QStringList{"2g2f", "6g6f", "7g7f", "5g5f"}));
    QVERIFY(!book.sortMoves(kHirateSfen, JosekiBook::SortKey::Frequency));
    // 評価値の順（同じなら出現頻度の高い順）
    QVERIFY(book.sortMoves(kHirateSfen, JosekiBook::SortKey::Value));
    QCOMPARE(order(kHirateSfen), (QStringList{"2g2f", "5g5f", "7g7f", "6g6f"}));
    QVERIFY(!book.sortMoves(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b -"), JosekiBook::SortKey::Value));

    // 手数違いの登録は、引いた登録だけを並べ替える
    const QString at21 = kHirateSfen + QStringLiteral(" 21");
    const QString after = SfenReplay::positionAfter(kHirateSfenWithPly, QStringLiteral("7g7f"));
    book.putMove(after, makeMove(QStringLiteral("3c3d"), 0, 0, 1));
    book.putMove(after, makeMove(QStringLiteral("8c8d"), 0, 0, 2));
    QVERIFY(book.sortMoves(after, JosekiBook::SortKey::Frequency));
    QCOMPARE(order(after), (QStringList{"8c8d", "3c3d"}));
    QCOMPARE(order(at21), (QStringList{"2g2f", "5g5f", "7g7f", "6g6f"}));
}

/// エンジンの読み筋の登録: 新しい手は出現頻度1、登録済みの手は評価値・深さ・予想応手だけを書き換える
void TestJosekiBook::session_registerEnginePv()
{
    using Result = JosekiBookSession::EngineRegistration;
    JosekiBookSession session;
    const QString sfen = kHirateSfen + QStringLiteral(" 1");
    QCOMPARE(session.registerEnginePv(sfen, {}, UsiScore::cp(10), 5), Result::Invalid);
    QVERIFY(!session.isModified());

    QCOMPARE(session.registerEnginePv(sfen, {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")},
                                      UsiScore::cp(45), 20),
             Result::Added);
    JosekiMove added = session.movesAt(sfen).constFirst();
    QCOMPARE(added.move, QStringLiteral("7g7f"));
    QCOMPARE(added.nextMove, QStringLiteral("3c3d"));
    QCOMPARE(added.value, 45);
    QCOMPARE(added.depth, 20);
    QCOMPARE(added.frequency, 1);
    QVERIFY(session.isModified());

    // 出現頻度とコメントは残し、評価値・深さ・予想応手を書き換える
    added.frequency = 7;
    added.comment = QStringLiteral("角道を開ける");
    QVERIFY(session.updateMove(sfen, added));
    QCOMPARE(session.registerEnginePv(sfen, {QStringLiteral("7g7f"), QStringLiteral("8c8d")}, UsiScore::cp(-12), -1),
             Result::Updated);
    JosekiMove updated = session.movesAt(sfen).constFirst();
    QCOMPARE(updated.nextMove, QStringLiteral("8c8d"));
    QCOMPARE(updated.value, -12);
    QCOMPARE(updated.depth, 0);
    QCOMPARE(updated.frequency, 7);
    QCOMPARE(updated.comment, QStringLiteral("角道を開ける"));

    // 読み筋が1手だけなら、登録済みの予想応手は残す。新しい手は「なし」
    QCOMPARE(session.registerEnginePv(sfen, {QStringLiteral("7g7f")}, UsiScore::cp(3), 9), Result::Updated);
    QCOMPARE(session.movesAt(sfen).constFirst().nextMove, QStringLiteral("8c8d"));
    QCOMPARE(session.registerEnginePv(sfen, {QStringLiteral("2g2f")}, UsiScore::cp(3), 9), Result::Added);
    QCOMPARE(session.movesAt(sfen).constLast().nextMove, QStringLiteral("none"));
}

/// 定跡の評価値: 詰みはやねうら王と同じく ±(32000 − 手数)
void TestJosekiBook::session_valueOfScore()
{
    QCOMPARE(JosekiBookSession::valueOf(UsiScore::cp(-345)), -345);
    QCOMPARE(JosekiBookSession::valueOf(UsiScore{}), 0);
    QCOMPARE(JosekiBookSession::valueOf(UsiScore::mate(5)), 31995);
    QCOMPARE(JosekiBookSession::valueOf(UsiScore::mate(-4)), -31996);
    QCOMPARE(JosekiBookSession::valueOf(UsiScore::mate(0)), -32000);
    UsiScore winning;
    winning.kind = UsiScore::Kind::Mate;
    winning.mateWin = true;
    QCOMPARE(JosekiBookSession::valueOf(winning), 32000);
}

/// 予想応手が "none" のときにやねうら王が先読みに使う手は、出現頻度、次に評価値の高い手。
/// それも同じならファイルで先の手で、深さは見ない（やねうら王 9.10 で確かめた並び）
void TestJosekiBook::leadingMove_ordersLikeYaneuraOu()
{
    JosekiBook book;
    QVERIFY(book.leadingMove(kHirateSfen).isEmpty());

    // 出現頻度は評価値・ファイルの順より先に比べる
    book.putMove(kHirateSfen, makeMove(QStringLiteral("8h2b+"), 100, 32, 1));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 0, 32, 5));
    QCOMPARE(book.leadingMove(kHirateSfen), QStringLiteral("7g7f"));

    // 出現頻度が同じなら評価値の高い手
    book.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 50, 32, 5));
    QCOMPARE(book.leadingMove(kHirateSfen), QStringLiteral("2g2f"));

    // 出現頻度・評価値が同じならファイルで先の手（深さは見ない）
    book.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 0, 0, 5));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("5g5f"), 0, 99, 5));
    QCOMPARE(book.leadingMove(kHirateSfen), QStringLiteral("7g7f"));

    // 手数は区別しない
    QCOMPARE(book.leadingMove(kHirateSfen + QStringLiteral(" 9")), QStringLiteral("7g7f"));
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
    QCOMPARE(repoLoad.registeredSfen(kHirateSfen), kHirateSfen + QStringLiteral(" 1"));

    // 検証
    QCOMPARE(repoLoad.positionCount(), 2);
    QVERIFY(repoLoad.contains(kHirateSfen));
    QVERIFY(repoLoad.contains(secondSfen));

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

    QVERIFY(repo.contains(kHirateSfen));
    const QList<JosekiMove> &moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].frequency, 1);
    QCOMPARE(moves[0].nextMove, QStringLiteral("none"));

    // マージ登録済みセットに記録されている
    QVERIFY(repo.isMergeRegistered(kHirateSfenWithPly, QStringLiteral("7g7f")));
    QVERIFY(repo.mergeRegistered().contains({kHirateSfen, QStringLiteral("7g7f")}));

    // 局面は登録したときの手数付きの SFEN で覚える
    QCOMPARE(repo.registeredSfen(kHirateSfen), kHirateSfenWithPly);

    // 定跡手を消すと登録済みの印も消える（マージでもう一度登録できる）
    QVERIFY(repo.removeMove(kHirateSfenWithPly, QStringLiteral("7g7f")));
    QVERIFY(!repo.isMergeRegistered(kHirateSfenWithPly, QStringLiteral("7g7f")));
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
    repo.putMove(ply2, makeMove(QStringLiteral("2g2f")));  // 手数は区別せず、最初の登録に足す

    QCOMPARE(repo.registeredSfen(ply2), ply1);
    QCOMPARE(repo.registeredPlies(kHirateSfen), QList<int>{1});
    QCOMPARE(repo.movesAt(ply2).size(), 2);
}

/// 既にある指し手を足し直すと、その場で置き換える（表の順を変えない）
void TestJosekiBook::putMove_replacesInPlace()
{
    JosekiBook repo;
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 10));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 20));
    repo.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 30));

    const QList<JosekiMove> moves = repo.movesAt(kHirateSfen);
    QCOMPARE(moves.size(), 2);
    QCOMPARE(moves[0].move, QStringLiteral("7g7f"));
    QCOMPARE(moves[0].value, 30);
    QCOMPARE(moves[1].move, QStringLiteral("2g2f"));
}

/// 同じ局面を手数違いで別々に登録したファイルは、手数ごとの登録のまま読み、保存しても残す
/// （やねうら王は既定では手数まで一致する登録だけを使うので、まとめると使えない局面が出る）
namespace {
/// 足す定跡: ▲７六歩の後の局面（△３四歩・読めない予想応手の△８四歩）、初期局面（▲７六歩・▲２六歩）、
/// 今の定跡に無い局面（▲２六歩の後。21手目と23手目に別々の登録）
QByteArray incomingBookText(const QString &hirate)
{
    const QString after76 = SfenReplay::positionAfter(hirate + QStringLiteral(" 1"), QStringLiteral("7g7f"));
    const QString after26 = SfenReplay::positionAfter(hirate + QStringLiteral(" 1"), QStringLiteral("2g2f"));
    const QString board26 = after26.section(QLatin1Char(' '), 0, 2);
    return "#YANEURAOU-DB2016 1.00\n"
           "sfen " + after76.toUtf8() + "\n"
           "3c3d none 10 20 5\n"
           "#受け\n"
           "8c8d abc 0 0 1\n"
           "sfen " + hirate.toUtf8() + " 1\n"
           "7g7f 8c8d 99 40 4\n"
           "2g2f none 5 5 2\n"
           "sfen " + board26.toUtf8() + " 21\n"
           "8c8d none 0 0 1\n"
           "sfen " + board26.toUtf8() + " 23\n"
           "3c3d none 0 0 1\n";
}

JosekiBook currentBook(const QString &hirateWithPly)
{
    JosekiBook book;
    JosekiMove first;
    first.move = QStringLiteral("7g7f");
    first.nextMove = QStringLiteral("3c3d");
    first.value = 30;
    first.depth = 20;
    first.frequency = 10;
    first.comment = QStringLiteral("今の定跡");
    book.putMove(hirateWithPly, first);
    JosekiMove reply;
    reply.move = QStringLiteral("3c3d");
    reply.nextMove = QStringLiteral("none");
    reply.frequency = 1;
    book.putMove(SfenReplay::positionAfter(hirateWithPly, QStringLiteral("7g7f")), reply);
    return book;
}
} // namespace

/// 別の定跡を足す: 無い局面は手数違いの登録もそのまま写し、同じ指し手は選んだ扱いで合わせる
void TestJosekiBook::mergeFrom_combinesBooks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("incoming.db"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(incomingBookText(kHirateSfen));
    }
    const JosekiLoadResult incoming = JosekiBookFile::load(path);
    QVERIFY(incoming.success);
    const QString after76 = SfenReplay::positionAfter(kHirateSfenWithPly, QStringLiteral("7g7f"));
    const QString board26 = SfenReplay::positionAfter(kHirateSfenWithPly, QStringLiteral("2g2f")).section(QLatin1Char(' '), 0, 2);

    // 既定: 今の定跡の値を残し、出現頻度を足し合わせる
    JosekiBook book = currentBook(kHirateSfenWithPly);
    const JosekiBook::MergeStats stats = book.mergeFrom(incoming.book, {});
    QCOMPARE(stats.addedPositions, 1);
    QCOMPARE(stats.addedMoves, 4);
    QCOMPARE(stats.updatedMoves, 2);
    const JosekiMove kept = book.movesAt(kHirateSfenWithPly).at(0);
    QCOMPARE(kept.frequency, 14);
    QCOMPARE(kept.value, 30);
    QCOMPARE(kept.nextMove, QStringLiteral("3c3d"));
    QCOMPARE(kept.comment, QStringLiteral("今の定跡"));
    QCOMPARE(book.movesAt(kHirateSfenWithPly).at(1).move, QStringLiteral("2g2f"));
    QCOMPARE(book.movesAt(after76).at(0).frequency, 6);
    QVERIFY(book.movesAt(after76).at(0).comment.isEmpty());
    // 読めない予想応手の文字列も失わない
    QCOMPARE(book.movesAt(after76).at(1).nextMove, QStringLiteral("abc"));
    QCOMPARE(book.registeredPlies(board26), (QList<int>{21, 23}));
    QCOMPARE(book.movesAt(board26 + QStringLiteral(" 23")).constFirst().move, QStringLiteral("3c3d"));

    // 足す定跡の値にし、出現頻度は足さない（足す側にコメントがあればそれにする）
    JosekiBook preferred = currentBook(kHirateSfenWithPly);
    JosekiBook::MergeOptions options;
    options.preferIncoming = true;
    options.sumFrequency = false;
    (void)preferred.mergeFrom(incoming.book, options);
    const JosekiMove replaced = preferred.movesAt(kHirateSfenWithPly).at(0);
    QCOMPARE(replaced.frequency, 4);
    QCOMPARE(replaced.value, 99);
    QCOMPARE(replaced.depth, 40);
    QCOMPARE(replaced.nextMove, QStringLiteral("8c8d"));
    QCOMPARE(replaced.comment, QStringLiteral("今の定跡"));
    QCOMPARE(preferred.movesAt(after76).at(0).comment, QStringLiteral("受け"));
    QCOMPARE(preferred.movesAt(after76).at(0).value, 10);
}

/// 定跡ファイルを足すのはワーカースレッドで行い、終わったら変更ありにする（保存はしない）
void TestJosekiBook::session_startMergeBook()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("incoming.db"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(incomingBookText(kHirateSfen));
    }
    JosekiBookSession session;
    session.addMove(kHirateSfenWithPly, makeMove(QStringLiteral("7g7f"), 30, 20, 10));
    session.setModified(false);
    QSignalSpy merged(&session, &JosekiBookSession::bookMerged);
    QSignalSpy changed(&session, &JosekiBookSession::contentChanged);
    QVERIFY(session.startMergeBook(path, {}));
    QVERIFY(session.isBusy());
    QCOMPARE(session.activity(), JosekiBookSession::Activity::Merging);
    // 足している間は別の読み書きを始めない
    QVERIFY(!session.startMergeBook(path, {}));
    QVERIFY(merged.wait(5000));
    QVERIFY(!session.isBusy());
    QVERIFY(session.isModified());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(merged.constFirst().at(0).toString(), path);
    QCOMPARE(session.movesAt(kHirateSfenWithPly).constFirst().frequency, 14);
    QCOMPARE(session.positionCount(), 3);

    QSignalSpy failed(&session, &JosekiBookSession::bookMergeFailed);
    QVERIFY(session.startMergeBook(dir.filePath(QStringLiteral("missing.db")), {}));
    QVERIFY(failed.wait(5000));
    QCOMPARE(session.positionCount(), 3);
}

void TestJosekiBook::load_keepsRegistrationsPerPly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("plies.db"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // 21手目と23手目に別々に登録（CRLF の行も読む）。23手目の 6g6f は値が違う
        file.write("#YANEURAOU-DB2016 1.00\r\n"
                   "sfen " + kHirateSfen.toUtf8() + " 21\r\n"
                   "6g6f 1c1d 0 32 2\r\n"
                   "5i6h 6b6c 0 32 1\r\n"
                   "#21手目のコメント\r\n"
                   "sfen " + kHirateSfen.toUtf8() + " 23\r\n"
                   "4i5h 6b6c 0 32 1\r\n"
                   "6g6f 4a3b 5 16 3\r\n");
    }
    const JosekiLoadResult loaded = JosekiBookFile::load(path);
    QVERIFY(loaded.success);
    JosekiBook book = loaded.book;
    QCOMPARE(book.positionCount(), 1);
    QCOMPARE(book.registeredPlies(kHirateSfen), (QList<int>{21, 23}));

    // 引くときは同じ手数の登録、無ければ最初の登録
    const QString at21 = kHirateSfen + QStringLiteral(" 21");
    const QString at23 = kHirateSfen + QStringLiteral(" 23");
    QCOMPARE(book.movesAt(at21).size(), 2);
    QCOMPARE(book.movesAt(at21).at(1).comment, QStringLiteral("21手目のコメント"));
    QCOMPARE(book.movesAt(at23).size(), 2);
    QCOMPARE(book.movesAt(at23).at(1).nextMove, QStringLiteral("4a3b"));
    QCOMPARE(book.movesAt(kHirateSfen + QStringLiteral(" 1")).first().move, QStringLiteral("6g6f"));
    QCOMPARE(book.registeredSfen(at23), at23);
    QCOMPARE(book.registeredSfen(kHirateSfen), at21);

    // 編集は引いた登録だけを変える。最後の手を消すとその登録だけが消える
    QVERIFY(book.updateMove(at23, makeMove(QStringLiteral("4i5h"), 7, 20, 4)));
    QCOMPARE(book.movesAt(at21).size(), 2);
    book.registerMergeMove(at23, QStringLiteral("6g6f"));
    QCOMPARE(book.movesAt(at23).at(1).frequency, 4);
    QCOMPARE(book.movesAt(at21).at(0).frequency, 2);

    const QString savedPath = dir.filePath(QStringLiteral("saved.db"));
    const JosekiSaveResult saved = JosekiBookFile::save(savedPath, book);
    QVERIFY(saved.success);
    QCOMPARE(saved.savedCount, 2);
    QFile file(savedPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()),
             QStringLiteral("#YANEURAOU-DB2016 1.00\n"
                            "sfen %1 21\n"
                            "6g6f 1c1d 0 32 2\n"
                            "5i6h 6b6c 0 32 1\n"
                            "#21手目のコメント\n"
                            "sfen %1 23\n"
                            "4i5h none 7 20 4\n"
                            "6g6f 4a3b 5 16 4\n").arg(kHirateSfen));

    QVERIFY(book.removeMove(at23, QStringLiteral("4i5h")));
    QVERIFY(book.removeMove(at23, QStringLiteral("6g6f")));
    QCOMPARE(book.registeredPlies(kHirateSfen), QList<int>{21});
    QCOMPARE(book.movesAt(at23).size(), 2);  // 23手目の登録が無くなったので最初の登録を引く
}

/// 1つの登録に同じ指し手が重なっていれば最初の行を使い、後の行のコメントは付けない。
/// 以前のこのアプリが書いた持ち駒の並び（先後交互）は、標準の並びで書き直す
void TestJosekiBook::load_duplicateMoveAndLegacyHands()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("dup.db"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#YANEURAOU-DB2016 1.00\n"
                   "sfen 4k4/9/9/9/9/9/9/9/4K4 b BbPp 1\n"
                   "7g7f none 10 1 1\n"
                   "#最初\n"
                   "7g7f none 20 2 2\n"
                   "#重なった行\n");
    }
    const JosekiLoadResult loaded = JosekiBookFile::load(path);
    QVERIFY(loaded.success);
    const QList<JosekiMove> moves = loaded.book.movesAt(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b BPbp 1"));
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().value, 10);
    QCOMPARE(moves.first().comment, QStringLiteral("最初"));

    const QString savedPath = dir.filePath(QStringLiteral("saved.db"));
    QVERIFY(JosekiBookFile::save(savedPath, loaded.book).success);
    QFile file(savedPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(QString::fromUtf8(file.readAll()).contains(QStringLiteral("sfen 4k4/9/9/9/9/9/9/9/4K4 b BPbp 1\n")));
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

    // 保存できなければ何も変えない（登録済みの印も付けない）。保存はワーカースレッドで行い、終わりを知らせる
    const KifuMergeEntry entry{1, kHirateSfenWithPly, QStringLiteral("7g7f"), {}, false};
    QSignalSpy finished(&session, &JosekiBookSession::mergeFinished);
    QSignalSpy failed(&session, &JosekiBookSession::saveFailed);
    QVERIFY(session.startRegisterMergeMoves({entry}));
    QVERIFY(session.isBusy());
    QCOMPARE(session.activity(), JosekiBookSession::Activity::Saving);
    QVERIFY(!session.startRegisterMergeMoves({entry}));  // 保存中は始めない
    QVERIFY(finished.wait());
    QCOMPARE(finished.takeFirst().at(0).toBool(), false);
    QCOMPARE(failed.count(), 1);
    QVERIFY(!failed.takeFirst().at(0).toString().isEmpty());
    QVERIFY(!session.isBusy());
    QCOMPARE(session.movesAt(kHirateSfen).first().frequency, 5);
    QVERIFY(session.mergeRegisteredMoves().isEmpty());

    // 保存できるようになれば登録でき、同じ手をもう一度登録しても数えない
    QVERIFY(QDir().mkpath(directory));
    for (int i = 0; i < 2; ++i) {
        QVERIFY(session.startRegisterMergeMoves({entry}));
        QVERIFY(finished.wait());
        QCOMPARE(finished.takeFirst().at(0).toBool(), true);
    }
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

/// 定跡手は詰めた形（指し手の番号）で持つ。駒打ち・成り・すべてのマスの指し手と、USI として読めない文字列
/// （小文字の駒打ちを含む）を、読み込み・保存の後も元の文字のまま返す
void TestJosekiBook::packedMoves_keepEveryNotation()
{
    JosekiBook book;
    const QStringList moves = {QStringLiteral("1a9i"), QStringLiteral("9i1a+"), QStringLiteral("8h2b+"),
                               QStringLiteral("P*5e"), QStringLiteral("R*1a"), QStringLiteral("L*9i"),
                               QStringLiteral("p*5e"), QStringLiteral("resign"), QStringLiteral("(none)")};
    for (const QString &move : moves) {
        JosekiMove entry = makeMove(move, -12, 3, 4);
        entry.nextMove = move == QStringLiteral("8h2b+") ? QStringLiteral("win") : QStringLiteral("G*4b");
        book.putMove(kHirateSfen, entry);
    }
    QVERIFY(book.hasMove(kHirateSfen, QStringLiteral("p*5e")));
    QVERIFY(!book.hasMove(kHirateSfen, QStringLiteral("S*5e")));
    QVERIFY(!book.hasMove(kHirateSfen, QStringLiteral("unknown")));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("notation.db"));
    QVERIFY(JosekiBookFile::save(path, book).success);
    const JosekiLoadResult loaded = JosekiBookFile::load(path);
    QVERIFY(loaded.success);
    const JosekiBook *sources[] = {&book, &loaded.book};
    for (const JosekiBook *source : sources) {
        const QList<JosekiMove> stored = source->movesAt(kHirateSfen);
        QCOMPARE(stored.size(), moves.size());
        for (qsizetype i = 0; i < moves.size(); ++i) {
            QCOMPARE(stored.at(i).move, moves.at(i));
            QCOMPARE(stored.at(i).nextMove,
                     moves.at(i) == QStringLiteral("8h2b+") ? QStringLiteral("win") : QStringLiteral("G*4b"));
            QCOMPARE(stored.at(i).value, -12);
            QCOMPARE(stored.at(i).frequency, 4);
        }
    }
    QVERIFY(book.removeMove(kHirateSfen, QStringLiteral("resign")));
    QCOMPARE(book.movesAt(kHirateSfen).size(), moves.size() - 1);
}

/// コメントの書き換え・消去・別の手のコメントが混ざらないこと
void TestJosekiBook::comments_updateAndRemove()
{
    JosekiBook book;
    book.putMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 0, 0, 1, QStringLiteral("角道")));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 0, 0, 1, QStringLiteral("飛車先")));
    QVERIFY(book.updateMove(kHirateSfen, makeMove(QStringLiteral("7g7f"), 0, 0, 1, QStringLiteral("角道を開ける"))));
    QVERIFY(book.updateMove(kHirateSfen, makeMove(QStringLiteral("2g2f"), 0, 0, 1)));
    book.putMove(kHirateSfen, makeMove(QStringLiteral("5g5f"), 0, 0, 1, QStringLiteral("中央")));
    const QList<JosekiMove> moves = book.movesAt(kHirateSfen);
    QCOMPARE(moves.at(0).comment, QStringLiteral("角道を開ける"));
    QVERIFY(moves.at(1).comment.isEmpty());
    QCOMPARE(moves.at(2).comment, QStringLiteral("中央"));

    // 予想応手を渡さない（null）更新は予想応手を変えない
    JosekiMove withReply = makeMove(QStringLiteral("8h7g"));
    withReply.nextMove = QStringLiteral("3c3d");
    book.putMove(kHirateSfen, withReply);
    JosekiMove keepReply = makeMove(QStringLiteral("8h7g"), 5);
    keepReply.nextMove = QString();
    QVERIFY(book.updateMove(kHirateSfen, keepReply));
    QCOMPARE(book.movesAt(kHirateSfen).last().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(book.movesAt(kHirateSfen).last().value, 5);
}

/// 比率の列は、この局面の出現頻度の合計に対する割合（合計が0なら空）
void TestJosekiBook::tableModel_showsFrequencyRatio()
{
    JosekiTableModel model;
    const QString hirate = SfenUtils::hirateSfen();
    model.setMoves(hirate, {makeMove(QStringLiteral("7g7f"), 0, 0, 3), makeMove(QStringLiteral("2g2f"), 0, 0, 1)}, true);
    QLocale locale = QLocale::system();
    locale.setNumberOptions(QLocale::DefaultNumberOptions);
    QCOMPARE(model.index(0, JosekiTableModel::Ratio).data().toString(), locale.toString(75.0, 'f', 1) + QLatin1Char('%'));
    QCOMPARE(model.index(1, JosekiTableModel::Ratio).data().toString(), locale.toString(25.0, 'f', 1) + QLatin1Char('%'));
    QCOMPARE(model.headerData(JosekiTableModel::Ratio, Qt::Horizontal).toString(), QStringLiteral("比率"));

    model.setMoves(hirate, {makeMove(QStringLiteral("7g7f"), 0, 0, 0)}, true);
    QVERIFY(model.index(0, JosekiTableModel::Ratio).data().toString().isEmpty());
}

/// 予想応手が「なし」の手には、導いた応手を灰色の括弧付きで出す。予想応手を書いた手には出さない
void TestJosekiBook::tableModel_showsDerivedReplyInGray()
{
    JosekiTableModel model;
    const QString hirate = SfenUtils::hirateSfen();
    JosekiMove written = makeMove(QStringLiteral("2g2f"));
    written.nextMove = QStringLiteral("8c8d");
    model.setMoves(hirate, {makeMove(QStringLiteral("7g7f")), written, makeMove(QStringLiteral("5g5f"))}, true,
                   {QStringLiteral("3c3d"), QStringLiteral("3c3d"), QString()});
    const auto next = [&model](int row, int role) { return model.index(row, JosekiTableModel::NextMove).data(role); };

    QCOMPARE(next(0, Qt::DisplayRole).toString(), QStringLiteral("（△３四歩(33)）"));
    QCOMPARE(next(0, Qt::ForegroundRole).value<QColor>(), QColor(Qt::darkGray));
    QVERIFY(next(0, Qt::ToolTipRole).toString().endsWith(QStringLiteral("\n3c3d")));

    QCOMPARE(next(1, Qt::DisplayRole).toString(), QStringLiteral("△８四歩(83)"));
    QVERIFY(!next(1, Qt::ForegroundRole).isValid());
    QCOMPARE(next(1, Qt::ToolTipRole).toString(), QStringLiteral("8c8d"));

    QCOMPARE(next(2, Qt::DisplayRole).toString(), QStringLiteral("なし"));
    QVERIFY(!next(2, Qt::ForegroundRole).isValid());

    // 導いた応手を渡さなければ「なし」のまま
    model.setMoves(hirate, {makeMove(QStringLiteral("7g7f"))}, true);
    QCOMPARE(next(0, Qt::DisplayRole).toString(), QStringLiteral("なし"));
}

/// 棋譜ファイルからのマージは KIF 以外の形式も読む（形式はファイルから判定する）
void TestJosekiBook::mergeEntries_readOtherKifuFormats()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("game.csa"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("V2.2\nPI\n+\n+7776FU\n-3334FU\n%TORYO\n");
    }
    QList<KifuMergeEntry> entries;
    QString error;
    QVERIFY2(JosekiMergeEntries::fromKifuFile(path, entries, &error), qPrintable(error));
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).usiMove, QStringLiteral("7g7f"));
    QCOMPARE(JosekiBook::positionKey(entries.at(0).sfen), JosekiBook::positionKey(SfenUtils::hirateSfen()));
    QCOMPARE(entries.at(1).usiMove, QStringLiteral("3c3d"));
    QCOMPARE(JosekiBook::positionKey(entries.at(1).sfen),
             JosekiBook::positionKey(SfenReplay::positionAfter(SfenUtils::hirateSfen(), QStringLiteral("7g7f"))));
}

QTEST_MAIN(TestJosekiBook)
#include "tst_joseki_book.moc"
