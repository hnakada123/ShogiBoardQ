/// @file tst_app_kifu_load.cpp
/// @brief 棋譜の読み込み（ファイル・貼り付け・局面）を、本体の合成ルートで組み立てた部品で確かめるテスト
///
/// 読み込みは KifuFileController → KifuLoader → 分岐ツリーの1本の経路を通る。
/// 形式の判定・上書き保存先・未保存の印・失敗時に元の棋譜を残すことを、振る舞いで確かめる。

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "evaluationgraphpresenter.h"
#include "evaluationseries.h"
#include "kifudocument.h"
#include "sfenutils.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifuexport.h"
#include "kifufilecontroller.h"
#include "kifuloader.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "notificationpresenter.h"

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
        players.seat(Turn::Black).humanName = QStringLiteral("前の先手");
        players.seat(Turn::White).humanName = QStringLiteral("前の後手");
        TimeControl timeControl;
        timeControl.black.mainMs = timeControl.white.mainMs = 600000;
        timeControl.black.incrementMs = timeControl.white.incrementMs = 10000;
        p.document->beginMatch(players, timeControl, false);
        p.document->recordMatchEnd();

        // 対局者名も開始日時もない棋譜
        QVERIFY(p.kifuFileController->loadKifuText(QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n")));
        QVERIFY(p.document->players().seat(Turn::Black).humanName.isEmpty());
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

    /// 局面編集の後に0手のまま保存した棋譜は、どの形式でも開ける（開始局面が書かれていれば指し手は要らない）
    void positionOnlyRecordsOpenInEveryFormat_data()
    {
        QTest::addColumn<int>("format");
        for (const KifuFormat format : {KifuFormat::Kif, KifuFormat::Ki2, KifuFormat::Csa, KifuFormat::Jkf,
                                        KifuFormat::Usen, KifuFormat::Usi})
            QTest::newRow(qPrintable(KifuFormats::name(format))) << static_cast<int>(format);
    }

    void positionOnlyRecordsOpenInEveryFormat()
    {
        QFETCH(int, format);
        MainWindow window;
        auto& p = window.composer().parts();
        const QString position = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
        p.document->replaceWithPosition(position);
        const QString text = KifuExport::lines(*p.document, static_cast<KifuFormat>(format)).join(QLatin1Char('\n'));
        p.document->replaceWithPosition(SfenUtils::hirateSfen());
        p.document->markSaved(QString());
        NotificationPresenter::ScopedCapture shown;
        QVERIFY2(p.kifuFileController->loadKifuText(text), qPrintable(text + shown.errors().join(QLatin1Char('\n'))));
        QCOMPARE(p.branchTree->root()->sfen(), position);
        p.document->markSaved(QString());
    }

    void emptyHirateRecordsLoad_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("kif") << QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n");
        QTest::newRow("csa") << QStringLiteral("V3.0\nPI\n+\n");
        QTest::newRow("jkf") << QStringLiteral("{\"header\":{},\"initial\":{\"preset\":\"HIRATE\"},\"moves\":[{}]}");
        QTest::newRow("jkf-default") << QStringLiteral("{\"header\":{},\"moves\":[{}]}");
    }

    /// 平手の開始局面だけの棋譜も開き、棋譜欄と盤面を開始局面へ戻す。
    void emptyHirateRecordsLoad()
    {
        QFETCH(QString, text);
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        NotificationPresenter::ScopedCapture shown;
        QVERIFY2(p.kifuFileController->loadKifuText(text), qPrintable(shown.errors().join(QLatin1Char('\n'))));
        QVERIFY(shown.errors().isEmpty());
        QCOMPARE(p.branchTree->root()->sfen(), SfenUtils::hirateSfen());
        QCOMPARE(p.branchTree->nodeCount(), 1);
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        // 貼り付けた棋譜は0手でも保存できるよう、未保存の棋譜として扱う。
        QVERIFY(p.document->isModified());
        QVERIFY(p.document->filePath().isEmpty());
        p.document->markSaved(QString());
    }

    void emptyRecordNeedsRecognizableContent()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        NotificationPresenter::ScopedCapture shown;
        const QString header = QStringLiteral("手数----指手---------消費時間--\n");
        QVERIFY(!p.kifuFileController->loadKifuText(QStringLiteral("this is not a kifu")));
        QVERIFY(shown.errors().join(QLatin1Char('\n')).contains(QStringLiteral("指し手を取得できませんでした")));
        QVERIFY2(p.kifuFileController->loadKifuText(QStringLiteral("手合割：香落ち\n") + header),
                 qPrintable(shown.errors().join(QLatin1Char('\n'))));
        QCOMPARE(p.branchTree->root()->sfen(), QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
        p.document->markSaved(QString());
    }

    /// 開いたままの局面集ビューア・棋譜貼り付けから、対局などを始めた後に反映しても棋譜を置き換えない
    void recordIsNotReplacedOutsideIdle()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        const QString root = p.branchTree->root()->sfen();
        const QStringList before = p.branchTree->mainRecordLine().usiMoves();
        ActivityLease consideration = window.composer().activity().begin(AppActivityState::Activity::DuringConsideration, nullptr);
        QVERIFY(!p.kifuFileController->canReplaceRecord());
        QSignalSpy messages(p.kifuFileController, &KifuFileController::statusMessage);

        p.kifuFileController->onSfenCollectionPositionSelected(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1"));
        p.kifuFileController->onKifuPasteImportRequested(QStringLiteral("position startpos moves 2g2f"));
        QCOMPARE(messages.count(), 2);
        QCOMPARE(p.branchTree->root()->sfen(), root);
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), before);

        consideration.reset();
        QVERIFY(p.kifuFileController->canReplaceRecord());
    }

    /// 読めないファイル・壊れたテキストは、元の棋譜と上書き保存先を残してエラーを知らせる
    void failedLoadKeepsTheRecord()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        const QStringList before = p.branchTree->mainRecordLine().usiMoves();
        const QString target = p.document->filePath();
        QSignalSpy errors(p.kifuLoader, &KifuLoader::errorOccurred);
        // 知らせはダイアログにせず記録する（自動化 API と同じ）
        NotificationPresenter::ScopedCapture shown;

        QVERIFY(!p.kifuFileController->loadKifuFile(fixture(QStringLiteral("missing.kif"))));
        QVERIFY(!p.kifuFileController->loadKifuText(QStringLiteral("this is not a kifu")));
        QVERIFY(errors.count() >= 2);
        QVERIFY(shown.errors().size() >= 2);
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), before);
        QCOMPARE(p.document->filePath(), target);
    }

    /// 警告のある棋譜は読み込み、知らせはエラーでなく警告にする（自動化 API の kifu.load は warnings で返す）
    void warningIsNotReportedAsError()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy errors(p.kifuLoader, &KifuLoader::errorOccurred);
        QSignalSpy warnings(p.kifuLoader, &KifuLoader::warningOccurred);
        NotificationPresenter::ScopedCapture shown;
        // 終局の後ろの指し手は読み飛ばして警告する
        const QString text = QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n"
                                            "   1 ７六歩(77)\n   2 投了\n   3 ２六歩(27)\n");
        QVERIFY(p.kifuFileController->loadKifuText(text));
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), QStringList{QStringLiteral("7g7f")});
        QCOMPARE(errors.count(), 0);
        QCOMPARE(warnings.count(), 1);
        QVERIFY(shown.errors().isEmpty());
        QCOMPARE(shown.notices().size(), 1);
        p.document->markSaved(QString());
    }

    /// 盤と持駒として成り立たない開始局面（持駒に金五枚）の棋譜は、CLI と同じく開かずにエラーにする
    void invalidStartPositionIsRejected()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy errors(p.kifuLoader, &KifuLoader::errorOccurred);
        NotificationPresenter::ScopedCapture shown;
        const QString text = QStringLiteral(
            "後手の持駒：なし\n"
            "  ９ ８ ７ ６ ５ ４ ３ ２ １\n"
            "+---------------------------+\n"
            "|v香v桂v銀v金v玉v金v銀v桂v香|一\n"
            "| ・v飛 ・ ・ ・ ・ ・v角 ・|二\n"
            "|v歩v歩v歩v歩v歩v歩v歩v歩v歩|三\n"
            "| ・ ・ ・ ・ ・ ・ ・ ・ ・|四\n"
            "| ・ ・ ・ ・ ・ ・ ・ ・ ・|五\n"
            "| ・ ・ ・ ・ ・ ・ ・ ・ ・|六\n"
            "| 歩 歩 歩 歩 歩 歩 歩 歩 歩|七\n"
            "| ・ 角 ・ ・ ・ ・ ・ 飛 ・|八\n"
            "| 香 桂 銀 金 玉 金 銀 桂 香|九\n"
            "+---------------------------+\n"
            "先手の持駒：金五\n"
            "手数----指手---------消費時間--\n"
            "   1 ７六歩(77)\n");
        QVERIFY(!p.kifuFileController->loadKifuText(text));
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.at(0).at(0).toString().contains(QStringLiteral("駒の数が上限を超えています")));
        QVERIFY(p.branchTree->mainRecordLine().usiMoves().isEmpty());
    }

    /// 棋譜を置き換えると、文書の置き換えの知らせを受けて前の評価値グラフが消える。読めなかったときは残す
    void replacingRecordClearsEvaluationGraph()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        p.evalGraph->beginAnalysis(QStringLiteral("Engine"), 0);
        p.evalGraph->plotAnalysisScore(1, UsiScore::cp(120));
        QCOMPARE(p.evalGraph->series()->count(0), 1);
        NotificationPresenter::ScopedCapture shown;
        QVERIFY(!p.kifuFileController->loadKifuText(QStringLiteral("this is not a kifu")));
        QCOMPARE(p.evalGraph->series()->count(0), 1);
        QVERIFY(p.kifuFileController->loadKifuFile(fixture(QStringLiteral("test_basic.kif"))));
        QCOMPARE(p.evalGraph->series()->count(0), 0);
    }

    /// 非同期の読み込みも同じ経路で入り、後から始めた読み込みが残る
    void asyncLoadUsesTheSamePath()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QSignalSpy finished(p.kifuLoader, &KifuLoader::loadFinished);
        p.kifuLoader->loadFileAsync(fixture(QStringLiteral("test_branch.kif")));
        p.kifuLoader->loadTextAsync(QStringLiteral("position startpos moves 9g9f"));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.last().first().toBool());
        QCOMPARE(p.branchTree->mainRecordLine().usiMoves(), QStringList{QStringLiteral("9g9f")});
    }
};

QTEST_MAIN(TestAppKifuLoad)
#include "tst_app_kifu_load.moc"
