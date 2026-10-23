/// @file tst_joseki_import.cpp
/// @brief 複数の棋譜から定跡に登録する手を集める処理（JosekiImport）と、その登録（JosekiBookSession::importMoves）のテスト

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QLabel>
#include <QDialogButtonBox>

#include "josekianalysisregisterdialog.h"
#include "josekibookmergedialog.h"
#include "josekibooksession.h"
#include "josekiimport.h"
#include "josekiimportdialog.h"
#include "josekisettings.h"
#include "sfenreplay.h"
#include "sfenutils.h"

namespace {

/// ▲７六歩（コメント付き）△３四歩 ▲２六歩 まで、後手の投了（先手の勝ち）。3手目に変化 ▲６六歩
const char kKifWithVariation[] =
    "手合割：平手\n"
    "先手：A\n"
    "後手：B\n"
    "手数----指手---------消費時間--\n"
    "   1 ７六歩(77)   ( 0:00/00:00:00)\n"
    "*角道を開ける\n"
    "*２行目\n"
    "   2 ３四歩(33)   ( 0:00/00:00:00)\n"
    "   3 ２六歩(27)   ( 0:00/00:00:00)+\n"
    "   4 投了\n"
    "まで3手で先手の勝ち\n"
    "\n"
    "変化：3手\n"
    "   3 ６六歩(67)   ( 0:00/00:00:00)\n";

/// ▲７六歩 △８四歩 まで、先手の投了（後手の勝ち）
const char kCsaWhiteWins[] = "V2.2\nPI\n+\n+7776FU\n-8384FU\n%TORYO\n";

/// ▲７六歩 △３四歩 で中断（勝敗なし）
const char kCsaInterrupted[] = "V2.2\nPI\n+\n+7776FU\n-3334FU\n%CHUDAN\n";

QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
{
    const QString path = dir.filePath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return QString();
    file.write(content);
    return path;
}

QStringList movesOf(const QList<JosekiImport::Move> &moves)
{
    QStringList usi;
    for (const JosekiImport::Move &move : moves) usi.append(move.usiMove);
    return usi;
}

int countOf(const QList<JosekiImport::Move> &moves, const QString &usiMove)
{
    for (const JosekiImport::Move &move : moves) {
        if (move.usiMove == usiMove) return move.count;
    }
    return 0;
}

} // namespace

class TestJosekiImport : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QStringList m_games;  ///< 先手の勝ち（変化あり）・後手の勝ち・勝敗なし

    JosekiImport::Result collect(const JosekiImport::Options &options) const
    {
        return JosekiImport::collect(m_games, options);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_games << writeFile(m_dir, QStringLiteral("games/a.kifu"), QByteArray(kKifWithVariation))
                << writeFile(m_dir, QStringLiteral("games/sub/b.csa"), QByteArray(kCsaWhiteWins))
                << writeFile(m_dir, QStringLiteral("games/sub/c.csa"), QByteArray(kCsaInterrupted));
        for (const QString &game : std::as_const(m_games)) QVERIFY(!game.isEmpty());
    }

    /// 同じ局面と指し手は棋譜の数で数え、見つけた順に並べる
    void collect_countsGamesPerMove()
    {
        const JosekiImport::Result result = collect({});
        QCOMPARE(result.gameCount, 3);
        QVERIFY(result.failedFiles.isEmpty());
        QCOMPARE(movesOf(result.moves), (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d"),
                                                      QStringLiteral("2g2f"), QStringLiteral("8c8d")}));
        QCOMPARE(countOf(result.moves, QStringLiteral("7g7f")), 3);
        QCOMPARE(countOf(result.moves, QStringLiteral("3c3d")), 2);
        QCOMPARE(result.moves.constFirst().sfen, SfenUtils::hirateSfen());
        // コメントは頼まれたときだけ
        QVERIFY(result.moves.constFirst().comment.isEmpty());
    }

    /// 手数の上限・最小出現回数で絞る
    void collect_limitsPlyAndCount()
    {
        JosekiImport::Options options;
        options.maxPly = 1;
        QCOMPARE(movesOf(collect(options).moves), QStringList{QStringLiteral("7g7f")});
        options = {};
        options.minCount = 2;
        QCOMPARE(movesOf(collect(options).moves), (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
    }

    /// 先手だけ・後手だけ・勝った側だけ（勝敗の付かない棋譜は使わない）
    void collect_filtersSides()
    {
        JosekiImport::Options options;
        options.sides = JosekiImport::Sides::BlackOnly;
        QCOMPARE(movesOf(collect(options).moves), (QStringList{QStringLiteral("7g7f"), QStringLiteral("2g2f")}));
        options.sides = JosekiImport::Sides::WhiteOnly;
        QCOMPARE(movesOf(collect(options).moves), (QStringList{QStringLiteral("3c3d"), QStringLiteral("8c8d")}));
        options.sides = JosekiImport::Sides::WinnerOnly;
        const JosekiImport::Result winners = collect(options);
        QCOMPARE(movesOf(winners.moves), (QStringList{QStringLiteral("7g7f"), QStringLiteral("2g2f"), QStringLiteral("8c8d")}));
        QCOMPARE(countOf(winners.moves, QStringLiteral("7g7f")), 1);
    }

    /// 変化の手も集められる。棋譜のコメントは1行にして入れる
    void collect_variationsAndComments()
    {
        JosekiImport::Options options;
        options.includeVariations = true;
        options.useComments = true;
        const JosekiImport::Result result = collect(options);
        QVERIFY(movesOf(result.moves).contains(QStringLiteral("6g6f")));
        QCOMPARE(countOf(result.moves, QStringLiteral("6g6f")), 1);
        QCOMPARE(result.moves.constFirst().comment, QStringLiteral("角道を開ける ２行目"));
    }

    /// フォルダはサブフォルダの棋譜まで名前順に集め、棋譜でないファイルと重複は除く。読めない棋譜は数える
    void expandPaths_andFailures()
    {
        const QString note = writeFile(m_dir, QStringLiteral("games/readme.txt"), QByteArray("memo"));
        const QStringList files = JosekiImport::expandPaths({m_dir.filePath(QStringLiteral("games")), m_games.at(1)});
        QCOMPARE(files.size(), 3);
        QVERIFY(!files.contains(note));
        QVERIFY(files.at(0).endsWith(QStringLiteral("a.kifu")));
        QVERIFY(files.at(1).endsWith(QStringLiteral("b.csa")));

        const QString broken = writeFile(m_dir, QStringLiteral("broken.jkf"), QByteArray("{ not json"));
        const JosekiImport::Result result = JosekiImport::collect({broken, m_games.at(1)}, {});
        QCOMPARE(result.gameCount, 1);
        QCOMPARE(result.failedFiles, QStringList{broken});
    }

    /// 進み具合を1つ読むたびに知らせ、false を返したら止める
    void collect_progressCanCancel()
    {
        QList<int> seen;
        const JosekiImport::Result result = JosekiImport::collect(m_games, {}, [&seen](int done) {
            seen.append(done);
            return done < 2;
        });
        QVERIFY(result.canceled);
        QVERIFY(result.moves.isEmpty());
        QCOMPARE(seen, (QList<int>{1, 2}));
    }

    /// 登録: あれば出現頻度に局数を足し、無ければ足す。コメントはコメントの無い手だけに入れる
    void session_importMoves()
    {
        JosekiBookSession session;
        const QString hirate = SfenUtils::hirateSfen();
        session.addMove(hirate, {QStringLiteral("7g7f"), QStringLiteral("3c3d"), 25, 18, 4, QStringLiteral("元のコメント")});
        session.setModified(false);
        const QString after = SfenReplay::positionAfter(hirate, QStringLiteral("7g7f"));
        const QList<JosekiImport::Move> moves{
            {hirate, QStringLiteral("7g7f"), 3, QStringLiteral("棋譜のコメント")},
            {after, QStringLiteral("3c3d"), 2, QStringLiteral("棋譜のコメント")},
        };
        const JosekiBookSession::ImportStats stats = session.importMoves(moves);
        QCOMPARE(stats.added, 1);
        QCOMPARE(stats.updated, 1);
        QVERIFY(session.isModified());
        const JosekiMove kept = session.movesAt(hirate).constFirst();
        QCOMPARE(kept.frequency, 7);
        QCOMPARE(kept.value, 25);
        QCOMPARE(kept.nextMove, QStringLiteral("3c3d"));
        QCOMPARE(kept.comment, QStringLiteral("元のコメント"));
        const JosekiMove added = session.movesAt(after).constFirst();
        QCOMPARE(added.frequency, 2);
        QCOMPARE(added.nextMove, QStringLiteral("none"));
        QCOMPARE(added.comment, QStringLiteral("棋譜のコメント"));
    }

    /// ダイアログ: フォルダの棋譜をワーカースレッドで集めて登録し、条件を保存する
    void importDialog_registersFolder()
    {
        JosekiBookSession session;
        const QString folder = m_dir.filePath(QStringLiteral("games"));
        JosekiImportDialog dialog(&session, {folder, folder});
        auto *list = dialog.findChild<QListWidget *>(QStringLiteral("josekiImportFiles"));
        QCOMPARE(list->count(), 1);
        dialog.findChild<QSpinBox *>(QStringLiteral("josekiImportMinCount"))->setValue(2);
        dialog.start();
        QVERIFY(dialog.isRunning());
        QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("josekiImportStart"))->isEnabled());
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("josekiImportStatus"));
        QTRY_VERIFY2(status->text().contains(QStringLiteral("棋譜 3 局")), qPrintable(status->text()));
        QVERIFY(!dialog.isRunning());
        QCOMPARE(session.movesAt(SfenUtils::hirateSfen()).constFirst().frequency, 3);
        QCOMPARE(session.positionCount(), 2);
        QVERIFY(session.isModified());
        QCOMPARE(JosekiSettings::josekiImportMinCount(), 2);
        QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("josekiImportStart"))->isEnabled());
    }

    /// ドロップされたもののうち、手元のファイル・フォルダだけを使う
    void importDialog_localPaths()
    {
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(m_games.at(0)), QUrl(QStringLiteral("https://example.com/a.kif"))});
        QCOMPARE(JosekiImportDialog::localPaths(&mime), QStringList{m_games.at(0)});
        QVERIFY(JosekiImportDialog::localPaths(nullptr).isEmpty());
    }

    /// 定跡ファイルを足すダイアログ: ファイルがあるときだけ足せ、同じ指し手の扱いを保存する
    void bookMergeDialog_optionsPersist()
    {
        JosekiSettings::josekiBookMergePreferIncoming.set(false);
        JosekiSettings::josekiBookMergeSumFrequency.set(true);
        JosekiBookMergeDialog dialog(m_dir.path());
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(!ok->isEnabled());
        dialog.findChild<QLineEdit *>(QStringLiteral("josekiBookMergePath"))->setText(m_games.at(1));
        QVERIFY(ok->isEnabled());
        QVERIFY(!dialog.options().preferIncoming);
        QVERIFY(dialog.options().sumFrequency);
        dialog.findChild<QRadioButton *>(QStringLiteral("josekiBookMergePreferIncoming"))->setChecked(true);
        dialog.findChild<QCheckBox *>(QStringLiteral("josekiBookMergeSumFrequency"))->setChecked(false);
        dialog.accept();
        QCOMPARE(dialog.filePath(), m_games.at(1));
        QVERIFY(JosekiSettings::josekiBookMergePreferIncoming());
        QVERIFY(!JosekiSettings::josekiBookMergeSumFrequency());
        JosekiBookMergeDialog reopened(m_dir.path());
        QVERIFY(reopened.options().preferIncoming);
        QVERIFY(!reopened.options().sumFrequency);
    }

    /// 棋譜解析の結果をまとめて登録する範囲: 初めは全体。終わりは始めより前にならず、手番を選ばなければ押せない
    void analysisRegisterDialog_range()
    {
        JosekiAnalysisRegisterDialog dialog(3, 40);
        QCOMPARE(dialog.range().fromPly, 3);
        QCOMPARE(dialog.range().toPly, 40);
        auto *from = dialog.findChild<QSpinBox *>(QStringLiteral("josekiRegisterFromPly"));
        auto *to = dialog.findChild<QSpinBox *>(QStringLiteral("josekiRegisterToPly"));
        to->setValue(10);
        from->setValue(20);
        QCOMPARE(dialog.range().toPly, 20);
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        dialog.findChild<QCheckBox *>(QStringLiteral("josekiRegisterBlack"))->setChecked(false);
        QVERIFY(ok->isEnabled());
        dialog.findChild<QCheckBox *>(QStringLiteral("josekiRegisterWhite"))->setChecked(false);
        QVERIFY(!ok->isEnabled());
        dialog.findChild<QCheckBox *>(QStringLiteral("josekiRegisterWhite"))->setChecked(true);
        dialog.accept();
        QVERIFY(!JosekiSettings::josekiAnalysisRegisterBlack());
        QVERIFY(JosekiSettings::josekiAnalysisRegisterWhite());
    }
};

QTEST_MAIN(TestJosekiImport)
#include "tst_joseki_import.moc"
