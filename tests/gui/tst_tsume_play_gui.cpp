#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTemporaryDir>
#include <QTimer>
#include "tsumeplaydialog.h"
#include "tsumegamesession.h"
#include "tsumeshogisettings.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "usimovecoordinateconverter.h"

class TestTsumePlayGui : public QObject
{
    Q_OBJECT
    QTimer dismissTimer;
    QString lastNotice;
    QElapsedTimer replyTimer;
    qint64 noticeDelay = -1;
    TsumePlayDialog* dialog = nullptr;

    void clickMove(const QString& move)
    {
        auto* view = dialog->findChild<ShogiView*>();
        const auto from = UsiMoveCoordinateConverter::parseMoveFrom(move, true);
        const auto to = UsiMoveCoordinateConverter::parseMoveTo(move);
        QVERIFY(from && to);
        // 描画のマス矩形は盤の原点相対。入力変換を使って駒台も含む実座標を探す。
        for (const QPoint square : {QPoint(from->file, from->rank), QPoint(to->file, to->rank)}) {
            QPoint pixel(-1, -1);
            for (int y = 0; y < view->height() && pixel.x() < 0; y += 4) {
                for (int x = 0; x < view->width(); x += 4) {
                    if (view->clickedSquare({x, y}) == square) { pixel = {x, y}; break; }
                }
            }
            QVERIFY(pixel.x() >= 0);
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, pixel);
        }
    }
private slots:
    void initTestCase()
    {
        connect(&dismissTimer, &QTimer::timeout, this, &TestTsumePlayGui::dismissNotice);
        dismissTimer.start(30);
    }
    void init()
    {
        TsumeshogiSettings::setPlayPreferences({});
        lastNotice.clear();
        replyTimer.invalidate();
        noticeDelay = -1;
    }
private:
    void recordReply(const QString& sfen, const QString& move)
    {
        if (!move.isEmpty() && sfen.section(QLatin1Char(' '), 1, 1) == QStringLiteral("b"))
            replyTimer.start();
    }
    void dismissNotice()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) return;
        lastNotice = box->text();
        if (lastNotice.contains(QStringLiteral("応手")) && dialog) {
            if (replyTimer.isValid()) noticeDelay = replyTimer.elapsed();
            dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-refutation.png"));
        }
        for (auto* button : box->buttons()) {
            if (button->text() == QStringLiteral("成る")) { button->click(); return; }
        }
        box->accept();
    }
private slots:
    void solveByBoardClicks()
    {
        TsumePlayDialog window;
        dialog = &window;
        window.show();
        QVERIFY(window.loadFile(QStringLiteral(REPO "/tests/fixtures/tsume_positions_with_moves.sfen")));
        auto* session = window.findChild<TsumeGameSession*>();
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(window.findChild<QComboBox*>()->count(), 5);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-play.png"));
        clickMove(QStringLiteral("3c5c+"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(session->remainingPlies(), 3);
        clickMove(QStringLiteral("1c4c"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(session->remainingPlies(), 1);
        clickMove(QStringLiteral("4c4d"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Solved);
        QVERIFY(lastNotice.contains(QStringLiteral("正解")));
        dialog = nullptr;
    }
    void refutationAndProblemSwitch_data()
    {
        QTest::addColumn<int>("problemIndex");
        QTest::addColumn<QString>("move");
        QTest::addColumn<QString>("notice");
        QTest::newRow("nomate") << 2 << QStringLiteral("R*1i") << QStringLiteral("詰みを防がれ");
        QTest::newRow("too-long") << 3 << QStringLiteral("S*4b") << QStringLiteral("残り3手以内では詰みません");
    }
    void refutationAndProblemSwitch()
    {
        QFETCH(int, problemIndex);
        QFETCH(QString, move);
        QFETCH(QString, notice);
        TsumePlayDialog window;
        dialog = &window;
        window.show();
        QVERIFY(window.loadFile(QStringLiteral(REPO "/tests/fixtures/tsume_positions.sfen")));
        auto* selector = window.findChild<QComboBox*>();
        auto* session = window.findChild<TsumeGameSession*>();
        connect(session, &TsumeGameSession::positionChanged, this, &TestTsumePlayGui::recordReply);
        selector->setCurrentIndex(problemIndex);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        clickMove(move);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Failed);
        QVERIFY(replyTimer.isValid());
        // 応手後の盤面が反映されても通知はまだ出ず、GUIは操作可能なまま。
        auto* board = window.findChild<ShogiView*>()->board();
        QCOMPARE(board->convertBoardToSfen(), session->sfen().section(QLatin1Char(' '), 0, 0));
        QVERIFY(lastNotice.isEmpty());
        QTest::qWait(600);
        QVERIFY(lastNotice.isEmpty());
        QTRY_VERIFY(lastNotice.contains(notice));
        QVERIFY(noticeDelay >= 1000);
        session->undo();
        QCOMPARE(session->state(), TsumeGameSession::State::Ready);
        for (int i = 0; i < 20; ++i) selector->setCurrentIndex(i % 5);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(selector->currentIndex(), 4);
        window.close();
        dialog = nullptr;
    }
    void pendingNoticeCancelled_data()
    {
        QTest::addColumn<QString>("action");
        QTest::newRow("undo") << QStringLiteral("undo");
        QTest::newRow("switch") << QStringLiteral("switch");
        QTest::newRow("close") << QStringLiteral("close");
    }
    void pendingNoticeCancelled()
    {
        QFETCH(QString, action);
        TsumePlayDialog window;
        dialog = &window;
        window.show();
        QVERIFY(window.loadFile(QStringLiteral(REPO "/tests/fixtures/tsume_positions.sfen")));
        auto* selector = window.findChild<QComboBox*>();
        auto* session = window.findChild<TsumeGameSession*>();
        selector->setCurrentIndex(2);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(session->play(QStringLiteral("R*1i")));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Failed);
        QVERIFY(lastNotice.isEmpty());
        if (action == QStringLiteral("undo")) session->undo();
        else if (action == QStringLiteral("switch")) selector->setCurrentIndex(0);
        else window.close();
        QTest::qWait(1200);
        QVERIFY(lastNotice.isEmpty());
        dialog = nullptr;
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir settings;
    qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumePlayGuiTest"));
    TestTsumePlayGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_tsume_play_gui.moc"
