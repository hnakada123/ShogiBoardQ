#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include "tsumecollectiondialog.h"
#include "tsumeplaydialog.h"
#include "tsumepositionpreview.h"
#include "tsumeshogisettings.h"
#include "mainwindow.h"
#include "applicationfonts.h"

class TestTsumeCollectionGui : public QObject
{
    Q_OBJECT
    QTemporaryDir files;
    QList<TsumeProblem> problems;
    QTimer driver;
    int ticks = 0;
    bool solve = false;
    bool completed = false;
    bool timedOut = false;
    QTimer modalDriver;
    QTimer modalWatchdog;
    QPointer<TsumeCollectionDialog> modalCollection;
    QString closeAction;
    QString modalError;
    int modalStage = 0;

    QString collectionPath() const { return files.filePath(QStringLiteral("1003.txt")); }
    QList<QPushButton*> cards(TsumeCollectionDialog& window)
    { return window.findChildren<QPushButton*>(QStringLiteral("tsumeProblemCard")); }
    void drivePlay()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) { box->accept(); return; }
        auto* play = qobject_cast<TsumePlayDialog*>(QApplication::activeModalWidget());
        if (!play) return;
        if (++ticks > 500) { timedOut = true; play->close(); return; }
        auto* session = play->findChild<TsumeGameSession*>();
        if (session->state() == TsumeGameSession::State::Solved || (!solve && session->state() == TsumeGameSession::State::Ready)) {
            completed = true;
            if (closeAction == QStringLiteral("escape")) QTest::keyClick(play, Qt::Key_Escape);
            else if (closeAction == QStringLiteral("close")) play->close();
            else QTest::mouseClick(play->findChild<QPushButton*>(QStringLiteral("tsumeBackToCollection")), Qt::LeftButton);
        } else if (solve && session->state() == TsumeGameSession::State::Ready) {
            const int remaining = session->remainingPlies();
            if (remaining == 5) play->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-play.png"));
            session->play(remaining == 5 ? QStringLiteral("3c5c+") : remaining == 3 ? QStringLiteral("1c4c") : QStringLiteral("4c4d"));
        }
    }
    void driveModalRoundtrip()
    {
        auto* collection = qobject_cast<TsumeCollectionDialog*>(QApplication::activeModalWidget());
        if (!collection) return;
        if (modalStage == 0) {
            modalCollection = collection;
            if (!collection->loadFile(collectionPath())) { modalError = QStringLiteral("load"); collection->close(); return; }
            collection->findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"))->setValue(2);
            modalStage = 1;
            QTest::mouseClick(cards(*collection).first(), Qt::LeftButton);
            // ここでテストを終えず、次のイベント処理後も一覧が生存・操作可能か確認する。
            return;
        }
        if (collection != modalCollection || !completed || timedOut || !collection->isVisible()) {
            modalError = QStringLiteral("collection did not regain input");
            collection->close();
            return;
        }
        auto* page = collection->findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"));
        if (modalStage == 1) {
            if (page->value() != 2) { modalError = QStringLiteral("page was lost"); collection->close(); return; }
            QTest::mouseClick(collection->findChild<QPushButton*>(QStringLiteral("tsumeNextPage")), Qt::LeftButton);
            if (page->value() != 3) { modalError = QStringLiteral("page button did not respond"); collection->close(); return; }
            completed = solve = false;
            ticks = 0;
            modalStage = 2;
            QTest::mouseClick(cards(*collection).first(), Qt::LeftButton);
        } else if (modalStage == 2) {
            if (page->value() != 3) modalError = QStringLiteral("second return lost page");
            modalStage = 3;
            QTest::keyClick(collection, Qt::Key_Escape);
        }
    }
    void modalTimeout()
    {
        timedOut = true;
        if (auto* modal = QApplication::activeModalWidget()) modal->close();
        if (modalCollection) modalCollection->close();
    }
private slots:
    void initTestCase()
    {
        QVERIFY(files.isValid());
        QFile fixture(QStringLiteral(REPO "/tests/fixtures/tsume_positions_with_moves.sfen"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        problems = TsumeCollection::parse(QString::fromUtf8(fixture.readAll())).problems;
        QCOMPARE(problems.size(), 5);
        QFile collection(collectionPath());
        QVERIFY(collection.open(QIODevice::WriteOnly));
        for (int i = 0; i < 1003; ++i) collection.write(problems[i % 5].sfen.toUtf8() + '\n');
        connect(&driver, &QTimer::timeout, this, &TestTsumeCollectionGui::drivePlay);
        driver.start(20);
        connect(&modalDriver, &QTimer::timeout, this, &TestTsumeCollectionGui::driveModalRoundtrip);
        modalWatchdog.setSingleShot(true);
        connect(&modalWatchdog, &QTimer::timeout, this, &TestTsumeCollectionGui::modalTimeout);
    }
    void init()
    {
        TsumeshogiSettings::setCollectionPreferences({});
        TsumeshogiSettings::setPlayPreferences({});
        TsumeshogiSettings::setTsumeCollectionFontSize(10);
        TsumeshogiSettings::setTsumePlayFontSize(10);
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tsume_progress.sqlite"));
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tsume_cache.sqlite"));
        ticks = 0;
        completed = timedOut = solve = false;
        closeAction.clear();
        modalError.clear();
        modalStage = 0;
    }
    void menuModalRoundtrip_data()
    {
        QTest::addColumn<QString>("exitAction");
        QTest::addColumn<bool>("solveProblem");
        QTest::newRow("back") << QStringLiteral("back") << false;
        QTest::newRow("escape") << QStringLiteral("escape") << false;
        QTest::newRow("close") << QStringLiteral("close") << false;
        QTest::newRow("solved-back") << QStringLiteral("back") << true;
    }
    void menuModalRoundtrip()
    {
        QFETCH(QString, exitAction);
        QFETCH(bool, solveProblem);
        closeAction = exitAction;
        solve = solveProblem;
        MainWindow mainWindow;
        mainWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mainWindow));
        auto* action = mainWindow.findChild<QAction*>(QStringLiteral("actionTsumePlay"));
        QVERIFY(action);
        modalDriver.start(30);
        modalWatchdog.start(15000);
        action->trigger(); // 本番と同じ MainWindow → 一覧の exec() → 対局の exec() を通る。
        modalDriver.stop();
        modalWatchdog.stop();
        QVERIFY2(!timedOut, "Modal dialog flow timed out");
        QVERIFY2(modalError.isEmpty(), qPrintable(modalError));
        QCOMPARE(modalStage, 3); // 一覧が閉じられるのは利用者が一覧を終了した後だけ。
        QVERIFY(modalCollection.isNull());
        QTRY_VERIFY(!QApplication::activeModalWidget());
        // 一覧を終了した後、実メインウィンドウのマウス操作も復帰する。
        auto* menu = mainWindow.menuBar();
        QVERIFY(!menu->actions().isEmpty());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(menu->actions().first()).center());
        QTRY_VERIFY(QApplication::activePopupWidget());
        QTest::keyClick(QApplication::activePopupWidget(), Qt::Key_Escape);
    }
    void paginationAndPersistence()
    {
        {
            TsumeCollectionDialog window;
            QVERIFY(window.loadFile(collectionPath()));
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto* page = window.findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"));
            auto* size = window.findChild<QComboBox*>(QStringLiteral("tsumePageSize"));
            QCOMPARE(cards(window).size(), 10);
            QCOMPARE(window.findChildren<TsumePositionPreview*>().size(), 10);
            QTRY_VERIFY(cards(window).first()->height() >= cards(window).first()->findChild<TsumePositionPreview*>()->height() + 40);
            QCOMPARE(page->maximum(), 101);
            auto* increase = window.findChild<QToolButton*>(QStringLiteral("tsumeCollectionFontIncrease"));
            auto* decrease = window.findChild<QToolButton*>(QStringLiteral("tsumeCollectionFontDecrease"));
            QVERIFY(increase && decrease);
            QTest::mouseClick(increase, Qt::LeftButton);
            QTest::mouseClick(increase, Qt::LeftButton);
            QTest::mouseClick(decrease, Qt::LeftButton);
            QCOMPARE(cards(window).first()->findChild<QLabel*>(QStringLiteral("cardProgress"))->font().pointSize(), 11);
            QCOMPARE(size->view()->font().pointSize(), 11);
            page->setValue(100);
            QCOMPARE(cards(window).first()->findChild<QLabel*>(QStringLiteral("cardProgress"))->font().pointSize(), 11);
            QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 990);
            window.findChild<QPushButton*>(QStringLiteral("tsumeNextPage"))->click();
            QCOMPARE(cards(window).size(), 3);
            QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 1000);
            QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("tsumeNextPage"))->isEnabled());
            for (int count : {20, 50, 100, 10}) {
                size->setCurrentIndex(size->findData(count));
                QCOMPARE(cards(window).size(), count);
                QCOMPARE(page->value(), 1);
            }
            page->setValue(2);
            // 一覧を見て解析が完了しても挑戦履歴は増えない。
            QTRY_VERIFY(cards(window).first()->findChild<QLabel*>(QStringLiteral("cardLength"))->text().contains(QStringLiteral("5手詰")));
            TsumeProgressStore store;
            QVERIFY(store.open());
            QCOMPARE(store.progress(TsumeCollection::positionId(problems[0].sfen)).attempts, 0);
            window.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-collection.png"));
            window.close();
        }
        TsumeCollectionDialog restored;
        restored.show();
        QTRY_COMPARE(cards(restored).size(), 10);
        QCOMPARE(restored.findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"))->value(), 2);
        QCOMPARE(cards(restored).first()->property("problemIndex").toInt(), 10);
        QCOMPARE(restored.font().pointSize(), 11);
        QCOMPARE(TsumeshogiSettings::tsumePlayFontSize(), 10);
    }
    void progressFilters()
    {
        TsumeProgressStore store;
        QVERIFY(store.open());
        QVERIFY(store.recordAttempt(TsumeCollection::positionId(problems[0].sfen)));
        QVERIFY(store.recordSolved(TsumeCollection::positionId(problems[0].sfen)));
        QVERIFY(store.recordAttempt(TsumeCollection::positionId(problems[1].sfen)));
        TsumeCollectionDialog window;
        QVERIFY(window.loadFile(collectionPath()));
        auto* filter = window.findChild<QComboBox*>(QStringLiteral("tsumeProgressFilter"));
        auto* summary = window.findChild<QLabel*>(QStringLiteral("tsumePageSummary"));
        filter->setCurrentIndex(1);
        QVERIFY(summary->text().startsWith(QStringLiteral("601問中")));
        QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 2);
        filter->setCurrentIndex(2);
        QVERIFY(summary->text().startsWith(QStringLiteral("201問中")));
        QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 1);
        filter->setCurrentIndex(3);
        QVERIFY(summary->text().startsWith(QStringLiteral("201問中")));
        QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 0);
    }
    void selectionHistoryAndReturn()
    {
        TsumeCollectionDialog window;
        QVERIFY(window.loadFile(collectionPath()));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* page = window.findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"));
        page->setValue(2);
        solve = true;
        cards(window).first()->click(); // モーダル対局はdriverで解いて一覧へ戻す
        QVERIFY(!timedOut);
        QVERIFY(completed);
        QVERIFY(window.isVisible());
        QCOMPARE(page->value(), 2);
        QCOMPARE(cards(window).first()->property("problemIndex").toInt(), 10);
        QVERIFY(cards(window).first()->findChild<QLabel*>(QStringLiteral("cardProgress"))->text().contains(QStringLiteral("正答済み")));
        TsumeProgressStore store;
        QVERIFY(store.open());
        const auto id = TsumeCollection::positionId(problems[0].sfen);
        QCOMPARE(store.progress(id).attempts, 1);
        QCOMPARE(store.progress(id).solves, 1);
        solve = completed = false;
        ticks = 0;
        cards(window).first()->click(); // 再挑戦を途中でやめても正答済みを保持
        QVERIFY(completed && !timedOut);
        QCOMPARE(store.progress(id).attempts, 2);
        QCOMPARE(store.progress(id).solves, 1);
        QCOMPARE(page->value(), 2);
    }
    void emptyFilterAndNonMate()
    {
        TsumeCollectionDialog window;
        window.show();
        QVERIFY(window.loadFile(collectionPath()));
        window.findChild<QComboBox*>(QStringLiteral("tsumeProgressFilter"))->setCurrentIndex(3);
        QVERIFY(cards(window).isEmpty());
        QVERIFY(!window.findChild<QSpinBox*>(QStringLiteral("tsumePageNumber"))->isEnabled());
        window.findChild<QComboBox*>(QStringLiteral("tsumeProgressFilter"))->setCurrentIndex(0);
        QFile file(files.filePath(QStringLiteral("nomate.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("k8/9/9/9/9/9/9/9/9 b - 1\n");
        file.close();
        QVERIFY(window.loadFile(file.fileName()));
        QTRY_VERIFY(!cards(window).first()->isEnabled());
        QVERIFY(cards(window).first()->findChild<QLabel*>(QStringLiteral("cardLength"))->text().contains(QStringLiteral("不詰")));
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumeCollectionGuiTest"));
    ApplicationFonts::initialize();
    TestTsumeCollectionGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_tsume_collection_gui.moc"
