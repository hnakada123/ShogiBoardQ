/// @file tst_app_kifu_load.cpp
/// @brief 棋譜の読み込み（ファイル・貼り付け・局面）を、本体の合成ルートで組み立てた部品で確かめるテスト
///
/// 読み込みは KifuFileController → KifuLoadCoordinator → 分岐ツリーの1本の経路を通る。
/// 形式の判定・上書き保存先・未保存の印・失敗時に元の棋譜を残すことを、振る舞いで確かめる。

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "appstate.h"
#include "kifudocument.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifuexport.h"
#include "kifufilecontroller.h"
#include "kifuloadcoordinator.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "uinotificationservice.h"

namespace {

QString fixture(const QString& name)
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
}

} // namespace

class TestAppKifuLoad : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// どの形式のファイルも同じ経路で分岐ツリーに入り、上書き保存先になる。未保存の印は付かない
    void eachFormatLoadsIntoTheTree_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("firstMove");
        QTest::newRow("kif") << QStringLiteral("test_basic.kif") << QStringLiteral("7g7f");
        QTest::newRow("ki2") << QStringLiteral("test_basic.ki2") << QStringLiteral("7g7f");
        QTest::newRow("csa") << QStringLiteral("test_basic.csa") << QStringLiteral("7g7f");
        QTest::newRow("jkf") << QStringLiteral("test_basic.jkf") << QStringLiteral("7g7f");
        QTest::newRow("usi") << QStringLiteral("test_basic.usi") << QStringLiteral("7g7f");
        QTest::newRow("usen") << QStringLiteral("test_basic.usen") << QStringLiteral("7g7f");
    }

    void eachFormatLoadsIntoTheTree()
    {
        QFETCH(QString, file);
        QFETCH(QString, firstMove);
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(file)));
        const QStringList moves = p.branchTree->mainRecordLine().usiMoves();
        QVERIFY2(!moves.isEmpty(), qPrintable(file));
        QCOMPARE(moves.first(), firstMove);
        QCOMPARE(p.document->filePath(), fixture(file));
        QVERIFY(!p.document->isModified());
        // 読み込んだら開始局面を表示し、棋譜欄は本譜を出す
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        QCOMPARE(p.kifuRecord->rowCount(), static_cast<int>(p.branchTree->mainLine().size()));
    }

    /// 対局の後に別の棋譜を読み込んだら、前の対局の対局者・持ち時間・開始日時を書き出しに使わない（B6・B7）
    void loadedKifuForgetsPreviousMatch()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        // 時間のある対局を終えた状態（対局者と持ち時間・開始日時の記録が残っている）
        MatchPlayers players;
        players.blackHumanName = QStringLiteral("前の先手");
        players.whiteHumanName = QStringLiteral("前の後手");
        TimeControl timeControl;
        timeControl.black.mainMs = timeControl.white.mainMs = 600000;
        timeControl.black.incrementMs = timeControl.white.incrementMs = 10000;
        p.document->beginMatch(players, timeControl, false);
        p.document->recordMatchEnd();

        // 対局者名も開始日時もない棋譜
        QVERIFY(p.kifuFileController->loadKifuText(QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n")));
        QVERIFY(p.document->players().blackHumanName.isEmpty());
        QVERIFY(!p.document->timeRecord().hasTimeControl());
        QVERIFY(!p.document->timeRecord().startDateTime().isValid());
        QVERIFY(!p.document->timeRecord().endDateTime().isValid());

        const QString csa = KifuExport::lines(*p.document, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(!csa.contains(QStringLiteral("$TIME")), qPrintable(csa));
        QVERIFY2(!csa.contains(QStringLiteral("$START_TIME")), qPrintable(csa));
        QVERIFY2(!csa.contains(QStringLiteral("前の先手")), qPrintable(csa));
        QVERIFY2(!csa.contains(QStringLiteral("前の後手")), qPrintable(csa));
    }

    /// 分岐とコメントも分岐ツリーに入る
    void branchesAndCommentsAreLoaded()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_branch.kif"))));
        QVERIFY(p.branchTree->lineCount() > 1);
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_comments.kif"))));
        bool hasComment = false;
        const QList<KifuBranchNode*> line = p.branchTree->mainLine();
        for (const KifuBranchNode* node : line) hasComment = hasComment || !node->comment().isEmpty();
        QVERIFY(hasComment);
    }

    /// 貼り付けたテキストは形式を内容から判定する。ファイルではないので上書き保存先は消す
    void pastedTextIsDetectedAndClearsOverwriteTarget()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        QVERIFY(!p.document->filePath().isEmpty());

        QVERIFY(p.kifuFileController->loadKifuText(QStringLiteral("position startpos moves 2g2f 8c8d")));
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), (QStringList{QStringLiteral("2g2f"), QStringLiteral("8c8d")}));
        QVERIFY(p.document->filePath().isEmpty());

        const QString csa = QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n");
        QVERIFY(p.kifuFileController->loadKifuText(csa));
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), (QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
    }

    /// SFEN・BOD の局面だけのテキストは、その局面だけの棋譜にする
    void positionOnlyTextBecomesTheStartPosition()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        const QString position = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
        QVERIFY(p.kifuFileController->loadKifuText(QStringLiteral("sfen ") + position));
        QCOMPARE(p.branchTree->root()->sfen(), position);
        QCOMPARE(p.branchTree->nodeCount(), 1);
        // 指し手のない USI ファイルも局面として読む
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_sfen.usi"))));
        QVERIFY(!p.branchTree->root()->sfen().isEmpty());
    }

    /// 局面集から選んだ局面は、未保存の印を付けず、上書き保存先を消す
    void sfenCollectionSelectionIsNotDirty()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        const QString position = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1");
        p.kifuFileController->onSfenCollectionPositionSelected(position);
        QCOMPARE(p.branchTree->root()->sfen(), position);
        QCOMPARE(p.navState->currentSfen(), position);
        QVERIFY(!p.document->isModified());
        QVERIFY(p.document->filePath().isEmpty());
    }

    /// 読めないファイル・壊れたテキストは、元の棋譜と上書き保存先を残してエラーを知らせる
    void failedLoadKeepsTheRecord()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        const QStringList before = p.branchTree->mainRecordLine().usiMoves();
        const QString target = p.document->filePath();
        QSignalSpy errors(p.kifuLoadCoordinator, &KifuLoadCoordinator::errorOccurred);
        // 知らせはダイアログにせず記録する（自動化 API と同じ）
        UiNotificationService::ScopedCapture shown;

        QVERIFY(!p.kifuFileController->loadKifuFile(fixture(QStringLiteral("missing.kif"))));
        QVERIFY(!p.kifuFileController->loadKifuText(QStringLiteral("this is not a kifu")));
        QVERIFY(errors.count() >= 2);
        QVERIFY(shown.errors().size() >= 2);
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), before);
        QCOMPARE(p.document->filePath(), target);
    }

    /// 非同期の読み込みも同じ経路で入り、後から始めた読み込みが残る
    void asyncLoadUsesTheSamePath()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy finished(p.kifuLoadCoordinator, &KifuLoadCoordinator::loadFinished);
        p.kifuLoadCoordinator->loadFileAsync(fixture(QStringLiteral("test_branch.kif")));
        p.kifuLoadCoordinator->loadTextAsync(QStringLiteral("position startpos moves 9g9f"));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.last().first().toBool());
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), QStringList{QStringLiteral("9g9f")});
    }
};

QTEST_MAIN(TestAppKifuLoad)
#include "tst_app_kifu_load.moc"
