#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QWheelEvent>
#include "tsumesolutionreplay.h"
#include "tsumeprogressstore.h"
#include "tsumecollection.h"
#include "tsumeplaydialog.h"
#include "tsumegamesession.h"
#include "tsumeshogisettings.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "usimovecoordinateconverter.h"
#include "applicationfonts.h"

class TestTsumePlayGui : public QObject
{
    Q_OBJECT
    QTimer dismissTimer;
    QString lastNotice;
    QElapsedTimer replyTimer;
    qint64 noticeDelay = -1;
    TsumePlayDialog* dialog = nullptr;
    QList<TsumeProblem> problems;
    QString promotionChoice;
    QRect promotionPreviewRect;
    QImage promotionPreview;
    QString promotionSfen;

    QRect squareRect(ShogiView* view, const QPoint& square) const
    {
        QRect rect;
        for (int y = 0; y < view->height(); y += 4) {
            for (int x = 0; x < view->width(); x += 4) {
                if (view->clickedSquare({x, y}) == square) rect |= QRect(x, y, 1, 1);
            }
        }
        return rect;
    }

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
        QFile file(QStringLiteral(REPO "/tests/fixtures/tsume_positions_with_moves.sfen"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        problems = TsumeCollection::parse(QString::fromUtf8(file.readAll())).problems;
        QCOMPARE(problems.size(), 5);
    }
    void init()
    {
        TsumeshogiSettings::setPlayPreferences({});
        TsumeshogiSettings::setTsumePlayFontSize(10);
        lastNotice.clear();
        replyTimer.invalidate();
        noticeDelay = -1;
        promotionChoice = QStringLiteral("成る");
        promotionPreviewRect = {};
        promotionPreview = {};
        promotionSfen.clear();
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
        if (lastNotice == QStringLiteral("成りますか？") && dialog && !promotionPreviewRect.isEmpty()) {
            auto* view = dialog->findChild<ShogiView*>();
            promotionPreview = view->grab(promotionPreviewRect).toImage();
            promotionSfen = view->board()->convertBoardToSfen();
            dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-promotion.png"));
        }
        if (lastNotice.contains(QStringLiteral("応手")) && dialog) {
            if (replyTimer.isValid()) noticeDelay = replyTimer.elapsed();
            dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-refutation.png"));
        }
        for (auto* button : box->buttons()) {
            if (button->text() == promotionChoice) { button->click(); return; }
        }
        box->accept();
    }
private slots:
    void promotionKeepsDraggedPiece_data()
    {
        QTest::addColumn<QString>("choice");
        QTest::addColumn<QString>("move");
        QTest::newRow("promote") << QStringLiteral("成る") << QStringLiteral("3c3d+");
        QTest::newRow("plain") << QStringLiteral("成らない") << QStringLiteral("3c3d");
        QTest::newRow("reject-non-check") << QStringLiteral("成る") << QStringLiteral("3c3b+");
    }
    void promotionKeepsDraggedPiece()
    {
        QFETCH(QString, choice);
        QFETCH(QString, move);
        TsumePlayDialog window;
        dialog = &window;
        promotionChoice = choice;
        window.setProblem(problems[0], 1, {}, nullptr, 5);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* session = window.findChild<TsumeGameSession*>();
        auto* view = window.findChild<ShogiView*>();
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        const auto to = UsiMoveCoordinateConverter::parseMoveTo(move);
        QVERIFY(to);
        const QRect fromRect = squareRect(view, {3, 3});
        const QRect toRect = squareRect(view, {to->file, to->rank});
        QVERIFY(!fromRect.isEmpty() && !toRect.isEmpty());
        promotionPreviewRect = fromRect.united(toRect);
        const QImage initial = view->grab(promotionPreviewRect).toImage();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, fromRect.center());
        QTest::mouseMove(view, toRect.center());
        QTest::qWait(30);
        const QImage dragged = view->grab(promotionPreviewRect).toImage();
        QVERIFY(dragged != initial);
        QSignalSpy positions(session, &TsumeGameSession::positionChanged);
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, toRect.center());
        // 成り選択中も移動先の駒表示を維持し、局面の確定は選択後に行う。
        QVERIFY(!promotionPreview.isNull());
        QCOMPARE(promotionPreview, dragged);
        QCOMPARE(promotionSfen, problems[0].sfen.section(QLatin1Char(' '), 0, 0));
        if (to->rank == 2) {
            // 王手でない着手を拒否した後は、駒が移動元に戻り選択も解除される。
            QVERIFY(positions.isEmpty());
            QCOMPARE(view->grab(promotionPreviewRect).toImage(), initial);
            QCOMPARE(session->state(), TsumeGameSession::State::Ready);
        } else {
            QVERIFY(!positions.isEmpty());
            QCOMPARE(positions.first().at(1).toString(), move);
            QCOMPARE(view->board()->convertBoardToSfen(), session->sfen().section(QLatin1Char(' '), 0, 0));
        }
        dialog = nullptr;
    }
    void solveByBoardClicks()
    {
        TsumePlayDialog window;
        dialog = &window;
        window.show();
        window.setProblem(problems[0], 1, {}, nullptr, 5);
        auto* session = window.findChild<TsumeGameSession*>();
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* view = window.findChild<ShogiView*>();
        const int squareSize = view->squareSize();
        const QString initialSfen = session->sfen();
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("tsumeReduceBoard")), Qt::LeftButton);
        QCOMPARE(view->squareSize(), squareSize - 1);
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("tsumeEnlargeBoard")), Qt::LeftButton);
        QCOMPARE(view->squareSize(), squareSize);
        for (int delta : {120, -120}) {
            const QPoint pos = view->rect().center();
            QWheelEvent wheel(pos, view->mapToGlobal(pos), {}, QPoint(0, delta), Qt::NoButton,
                              Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view, &wheel);
            QCOMPARE(view->squareSize(), squareSize + (delta > 0 ? 1 : 0));
            QVERIFY(wheel.isAccepted());
        }
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("tsumeFlipBoard")), Qt::LeftButton);
        QVERIFY(view->flipMode());
        QCOMPARE(session->sfen(), initialSfen);
        window.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-play.png"));
        // 回転した盤でも同じ座標の着手で最後まで解ける。
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

        auto* session = window.findChild<TsumeGameSession*>();
        connect(session, &TsumeGameSession::positionChanged, this, &TestTsumePlayGui::recordReply);
        window.setProblem(problems[problemIndex], problemIndex + 1, {}, nullptr, 5);
        window.setProblemNavigation(true, true); // 一覧から開いたときと同じ表示で撮影する
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
        for (int i = 0; i < 20; ++i) window.setProblem(problems[i % 5], i % 5 + 1, {}, nullptr, 5);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(session->sfen(), problems[4].sfen);
        window.close();
        dialog = nullptr;
    }
    void pendingNoticeCancelled_data()
    {
        QTest::addColumn<QString>("action");
        QTest::newRow("undo") << QStringLiteral("undo");
        QTest::newRow("switch") << QStringLiteral("switch");
        QTest::newRow("close") << QStringLiteral("close");
        QTest::newRow("solution") << QStringLiteral("solution");
    }
    void pendingNoticeCancelled()
    {
        QFETCH(QString, action);
        TsumePlayDialog window;
        dialog = &window;
        window.show();

        auto* session = window.findChild<TsumeGameSession*>();
        window.setProblem(problems[2], 3, {}, nullptr, 5);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(session->play(QStringLiteral("R*1i")));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Failed);
        QVERIFY(lastNotice.isEmpty());
        if (action == QStringLiteral("undo")) session->undo();
        else if (action == QStringLiteral("switch")) window.setProblem(problems[0], 1, {}, nullptr, 5);
        else if (action == QStringLiteral("solution")) window.findChild<QPushButton*>(QStringLiteral("tsumeShowSolution"))->click();
        else window.close();
        QTest::qWait(1200);
        QVERIFY(lastNotice.isEmpty());
        dialog = nullptr;
    }
    void solutionPlaybackAndResume()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        TsumePlayDialog window;
        dialog = &window;
        auto problem = problems[0];
        problem.referenceMoves.clear(); // 初期局面だけのファイルでも再生する。
        window.setProblem(problem, 1, {}, &store, 5);
        window.setProblemNavigation(true, true); // 一覧から開いたときと同じ表示で撮影する
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* session = window.findChild<TsumeGameSession*>();
        auto* replay = window.findChild<TsumeSolutionReplay*>();
        auto* board = window.findChild<ShogiView*>()->board();
        auto* first = window.findChild<QPushButton*>(QStringLiteral("tsumeSolutionFirst"));
        auto* previous = window.findChild<QPushButton*>(QStringLiteral("tsumeSolutionPrevious"));
        auto* next = window.findChild<QPushButton*>(QStringLiteral("tsumeSolutionNext"));
        auto* last = window.findChild<QPushButton*>(QStringLiteral("tsumeSolutionLast"));
        auto* resume = window.findChild<QPushButton*>(QStringLiteral("tsumeReturnToGame"));
        auto* showSolution = window.findChild<QPushButton*>(QStringLiteral("tsumeShowSolution"));
        auto* solutionText = window.findChild<QPlainTextEdit*>(QStringLiteral("tsumeSolutionText"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(!previous->isEnabled());
        QVERIFY(!resume->isEnabled());
        QVERIFY(!first->isVisible() && !next->isVisible() && !last->isVisible() && !resume->isVisible());
        QVERIFY(!solutionText->isVisible() && solutionText->toPlainText().isEmpty());
        clickMove(QStringLiteral("3c5c+"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        const QString saved = session->sfen();
        auto* increase = window.findChild<QToolButton*>(QStringLiteral("tsumePlayFontIncrease"));
        auto* decrease = window.findChild<QToolButton*>(QStringLiteral("tsumePlayFontDecrease"));
        QVERIFY(increase && decrease);
        QTest::mouseClick(increase, Qt::LeftButton);
        QTest::mouseClick(increase, Qt::LeftButton);
        QTest::mouseClick(decrease, Qt::LeftButton);
        QCOMPARE(window.findChild<QLabel*>(QStringLiteral("tsumeStatus"))->font().pointSize(), 11);
        QCOMPARE(TsumeshogiSettings::tsumePlayFontSize(), 11);
        QCOMPARE(session->sfen(), saved);
        {
            TsumePlayDialog restored;
            QCOMPARE(restored.font().pointSize(), 11);
        }
        lastNotice.clear(); // 実着手時の成り選択は、再生中の通知とは分けて確認する。
        const auto id = TsumeCollection::positionId(problem.sfen);
        QTest::mouseClick(showSolution, Qt::LeftButton);
        QTRY_VERIFY(replay->available());
        QCOMPARE(replay->currentPly(), 0);
        QVERIFY(first->isVisible() && next->isVisible() && last->isVisible() && resume->isVisible());
        QVERIFY(solutionText->isVisible() && solutionText->isReadOnly());
        QVERIFY(solutionText->toPlainText().startsWith(QStringLiteral("▲５三飛成(33)")));
        QCOMPARE(solutionText->toPlainText().count(QChar(u'▲')) + solutionText->toPlainText().count(QChar(u'△')), 5);
        QTest::mouseClick(next, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 1);
        QCOMPARE(session->sfen(), saved);
        shogi::Position expected;
        QVERIFY(expected.set_sfen(problem.sfen.toStdString(), true));
        QVERIFY(expected.apply_usi_move("3c5c+"));
        QCOMPARE(board->convertBoardToSfen(), QString::fromStdString(expected.to_sfen()).section(QLatin1Char(' '), 0, 0));
        clickMove(QStringLiteral("1c4c")); // 再生中の盤クリックでは対局も再生位置も進めない。
        QCOMPARE(session->sfen(), saved);
        QCOMPARE(replay->currentPly(), 1);
        QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("tsumeUndo"))->isEnabled());
        QTest::mouseClick(next, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 2); // 玉方の応手も1手として再生
        QTest::mouseClick(previous, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 1);
        QTest::mouseClick(first, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 0);
        QVERIFY(!first->isEnabled() && !previous->isEnabled());
        QCOMPARE(board->convertBoardToSfen(), problem.sfen.section(QLatin1Char(' '), 0, 0));
        QTest::mouseClick(last, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 5);
        QVERIFY(!next->isEnabled() && !last->isEnabled());
        QVERIFY2(lastNotice.isEmpty(), qPrintable(lastNotice));
        QCOMPARE(store.progress(id).attempts, 1);
        QCOMPARE(store.progress(id).solves, 0);
        window.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-solution.png"));
        QTest::mouseClick(resume, Qt::LeftButton);
        QVERIFY(!solutionText->isVisible() && solutionText->toPlainText().isEmpty());
        QVERIFY(!next->isVisible() && !resume->isVisible());
        QCOMPARE(board->convertBoardToSfen(), saved.section(QLatin1Char(' '), 0, 0));
        QCOMPARE(session->remainingPlies(), 3);
        QVERIFY(window.findChild<QPushButton*>(QStringLiteral("tsumeUndo"))->isEnabled());
        clickMove(QStringLiteral("1c4c"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        clickMove(QStringLiteral("4c4d"));
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Solved);
        QCOMPARE(store.progress(id).solves, 1);
        QTest::mouseClick(showSolution, Qt::LeftButton);
        QTest::mouseClick(last, Qt::LeftButton);
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("tsumeRestart")), Qt::LeftButton);
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(store.progress(id).attempts, 2);
        QCOMPARE(store.progress(id).solves, 1);
        QVERIFY(!resume->isEnabled());
        QVERIFY(!solutionText->isVisible() && !next->isVisible());
        QTest::mouseClick(showSolution, Qt::LeftButton);
        QTest::mouseClick(next, Qt::LeftButton);
        QCOMPARE(replay->currentPly(), 1);
        dialog = nullptr;
    }
    void problemNavigationButtons()
    {
        TsumePlayDialog window;
        dialog = &window;
        auto* previous = window.findChild<QPushButton*>(QStringLiteral("tsumePreviousProblem"));
        auto* next = window.findChild<QPushButton*>(QStringLiteral("tsumeNextProblem"));
        QVERIFY(previous && next);
        // 単独で開いた対局画面（一覧なし）では前後の問題へ移動できない。
        QVERIFY(!previous->isEnabled() && !next->isEnabled());
        QSignalSpy previousRequests(&window, &TsumePlayDialog::previousProblemRequested);
        QSignalSpy nextRequests(&window, &TsumePlayDialog::nextProblemRequested);
        window.setProblem(problems[0], 1, {}, nullptr, 5);
        window.setProblemNavigation(false, true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(!previous->isEnabled() && next->isEnabled());
        QTest::mouseClick(next, Qt::LeftButton);
        QCOMPARE(nextRequests.size(), 1);
        QVERIFY(previousRequests.isEmpty());
        // 問題の切替は一覧側が行うので、ボタンを押しただけでは対局は変わらない。
        auto* session = window.findChild<TsumeGameSession*>();
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        QCOMPARE(session->sfen(), problems[0].sfen);
        window.setProblemNavigation(true, false);
        QVERIFY(previous->isEnabled() && !next->isEnabled());
        QTest::mouseClick(previous, Qt::LeftButton);
        QCOMPARE(previousRequests.size(), 1);
        QCOMPARE(nextRequests.size(), 1);
        window.findChild<QSpinBox*>(QStringLiteral("tsumeTimeLimit"))->setValue(9);
        QCOMPARE(window.timeoutSec(), 9);
        dialog = nullptr;
    }
    void cancelPendingSolution()
    {
        TsumePlayDialog window;
        dialog = &window;
        window.setProblem(problems[0], 1, QStringLiteral(APP_BUILD "/tests/mock_tsume_engine"), nullptr, 5);
        window.show();
        auto* session = window.findChild<TsumeGameSession*>();
        auto* replay = window.findChild<TsumeSolutionReplay*>();
        QTRY_COMPARE(session->state(), TsumeGameSession::State::Ready);
        qputenv("SHOGI_TEST_MATE_MODE", "slow");
        window.findChild<QPushButton*>(QStringLiteral("tsumeShowSolution"))->click();
        QTest::qWait(100);
        qunsetenv("SHOGI_TEST_MATE_MODE");
        QVERIFY(replay->loading());
        window.findChild<QPushButton*>(QStringLiteral("tsumeReturnToGame"))->click();
        QVERIFY(!replay->loading());
        QTest::qWait(2100);
        auto* board = window.findChild<ShogiView*>()->board();
        QCOMPARE(board->convertBoardToSfen(), problems[0].sfen.section(QLatin1Char(' '), 0, 0));
        QCOMPARE(session->state(), TsumeGameSession::State::Ready);
        QVERIFY(lastNotice.isEmpty());
        auto* solutionText = window.findChild<QPlainTextEdit*>(QStringLiteral("tsumeSolutionText"));
        QVERIFY(!solutionText->isVisible() && solutionText->toPlainText().isEmpty());
        dialog = nullptr;
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir settings;
    qputenv("XDG_CONFIG_HOME", settings.path().toUtf8());
    qputenv("XDG_DATA_HOME", settings.path().toUtf8());
    qputenv("XDG_CACHE_HOME", settings.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumePlayGuiTest"));
    ApplicationFonts::initialize();
    TestTsumePlayGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_tsume_play_gui.moc"
