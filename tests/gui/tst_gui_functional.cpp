#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QChart>
#include <QLineSeries>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QLayout>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QRadioButton>
#include <QTextEdit>
#include <QTextBlock>
#include <QSpinBox>
#include <QSettings>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTableWidget>
#include <QTranslator>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QVBoxLayout>
#include "mainwindow.h"
#include "shogiview.h"
#include "shogiviewlayout.h"
#include "boardinteractioncontroller.h"
#include "elidelabel.h"
#include "shogiboard.h"
#include "recordpane.h"
#include "tablestyles.h"
#include "kifurecordlistmodel.h"
#include "kifubranchtree.h"
#include "gamerecordmodel.h"
#include "evaluationchartwidget.h"
#include "evaluationchartview.h"
#include "branchtreemanager.h"
#include "sfenpositiontracer.h"
#include "settingscommon.h"
#include "appsettings.h"
#include "applicationfonts.h"
#include "boardappearance.h"
#include "boardcolorpresets.h"
#include "boardcolordialog.h"
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"
#include "engineanalysistab.h"
#include "engineinfowidget.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"
#include "gamesettings.h"
#include "sfencollectiondialog.h"
#include "kifupastedialog.h"
#include "josekiwindow.h"
#include "josekimovedialog.h"
#include "josekimoveinputwidget.h"
#include "menuwindow.h"
#include "menubuttonwidget.h"
#include "enginelistsettings.h"
#include "commenteditorpanel.h"
#include "considerationtabmanager.h"
#include "gameinfopanecontroller.h"
#include "analysissettings.h"
#include "kifuanalysisdialog.h"
#include "analysisresultspresenter.h"
#include "kifuanalysislistmodel.h"
#include "shogienginethinkingmodel.h"

class GuiAudit : public QObject
{
    Q_OBJECT
    std::unique_ptr<MainWindow> window;
    QTimer dialogTimer;
    QString dialogMode, dialogPath, dialogClass, dialogTitle;
    bool dialogHandled = false;
    QList<QUrl> urls;
    QStringList clicked;
    QString selectedCollection;
    QStringList dialogMessages;
    const QString initial = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    ShogiView* board() const { return window->findChild<ShogiView*>(); }
    RecordPane* record() const { return window->findChild<RecordPane*>(); }
    QString boardSfen() const { return board()->board()->convertBoardToSfen(); }
    bool hasKifuPasteDialog() const
    {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (qobject_cast<KifuPasteDialog*>(widget)) return true;
        }
        return false;
    }
    QPoint squarePoint(int file, int rank) const
    {
        const QPoint wanted(file, rank);
        for (int y = 0; y < board()->height(); y += 8)
            for (int x = 0; x < board()->width(); x += 8)
                if (board()->clickedSquare(QPoint(x, y)) == wanted)
                    return QPoint(x, y) + QPoint(board()->fieldSize().width()/3, board()->fieldSize().height()/3);
        return {};
    }
    QAction* action(const QString& name) const { return window->findChild<QAction*>(name); }
    ShogiViewLayout editLayout() const
    {
        ShogiViewLayout layout;
        layout.setSquareSize(board()->squareSize());
        layout.setStandGapCols(0.7);
        layout.setFlipMode(board()->flipMode());
        layout.recalcLayoutParams(board()->font());
        return layout;
    }
    QPoint editPoint(int file, int rank) const
    {
        const auto layout = editLayout();
        if (file == 12) return board()->pieceBoxCellRect(rank).center();
        if (file <= 9)
            return layout.calculateSquareRectangleBasedOnBoardState(file, rank, 9, 9)
                .translated(layout.offsetX(), layout.offsetY()).center();
        const QRect stand = file == 10 ? layout.blackStandBoundingRect(9, 9)
                                      : layout.whiteStandBoundingRect(9, 9);
        const int type = file == 10 ? rank - 1 : 9 - rank;
        const bool top = (file == 10) == board()->flipMode();
        const int col = top ? 1 - type % 2 : type % 2;
        const int row = top ? type / 2 : 3 - type / 2;
        return stand.topLeft() + QPoint((2 * col + 1) * board()->fieldSize().width() / 2,
                                         (2 * row + 1) * board()->fieldSize().height() / 2);
    }
    void editMove(const QPoint& from, const QPoint& to)
    {
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(from.x(), from.y()));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(to.x(), to.y()));
    }
    void click(const QString& name)
    {
        auto* a = action(name);
        QVERIFY2(a, qPrintable(name));
        clickAction(a);
    }
    void clickAction(QAction* a)
    {
        const QString name = a->objectName().isEmpty() ? a->text() : a->objectName();
        QVERIFY2(a->isEnabled(), qPrintable(name + " is disabled"));
        QMenu* owner = nullptr;
        for (auto* m : window->findChildren<QMenu*>()) {
            if (m->actions().contains(a)) { owner = m; break; }
        }
        QVERIFY2(owner, qPrintable(name + " has no visible menu"));
        clicked.append(name);
        owner->popup(window->mapToGlobal(QPoint(40, 40)));
        QTest::qWait(20);
        QTest::mouseClick(owner, Qt::LeftButton, Qt::NoModifier, owner->actionGeometry(a).center());
        QCoreApplication::processEvents();
    }
    void selectPieceStyle(const QString& style)
    {
        click("actionBoardAppearance");
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTabWidget*>("appearanceSections")->setCurrentIndex(0);
        dialog->findChild<QComboBox*>("appearancePieceFilter")->setCurrentIndex(0);
        auto* list = dialog->findChild<QListWidget*>("appearancePieces");
        QVERIFY(list && list->count() == 21);
        const int row = static_cast<int>(AppSettings::availablePieceStyles().indexOf(style));
        QVERIFY(row >= 0);
        auto* item = list->item(row);
        list->scrollToItem(item);
        QTest::qWait(20);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(item).center());
        QCOMPARE(AppSettings::pieceStyle(), style);
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
    }
    void openAppearanceDetails()
    {
        click("actionBoardAppearance");
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        dialog->findChild<QTabWidget*>("appearanceSections")->setCurrentIndex(5);
    }
    void armDialog(const QString& mode = "close", const QString& path = {})
    {
        dialogMode = mode; dialogPath = path;
        dialogHandled = false; dialogClass.clear(); dialogTitle.clear();
        dialogTimer.start(30);
    }
    void paste(const QString& text, bool waitForLoad = true)
    {
        click("actionPasteKifu");
        QDialog* dialog = nullptr;
        for (auto* w : QApplication::topLevelWidgets())
            if (w->isVisible() && QString(w->metaObject()->className()) == "KifuPasteDialog")
                dialog = qobject_cast<QDialog*>(w);
        QVERIFY(dialog);
        auto* editor = dialog->findChild<QPlainTextEdit*>();
        QVERIFY(editor);
        QApplication::clipboard()->setText(text);
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(editor->toPlainText(), text);
        QPushButton* import = nullptr;
        for (auto* b : dialog->findChildren<QPushButton*>())
            if (b->text() == QStringLiteral("取り込む")) import = b;
        QVERIFY(import);
        QTest::mouseClick(import, Qt::LeftButton);
        if (waitForLoad) QTRY_VERIFY(!hasKifuPasteDialog());
    }
    void sampleGame()
    {
        paste("position startpos moves 7g7f 3c3d 2g2f 8c8d");
        QVERIFY(record()->kifuView()->model()->rowCount() >= 5);
    }
    QString copy(const QString& name)
    {
        QApplication::clipboard()->clear();
        click(name);
        return QApplication::clipboard()->text();
    }
    void snapshot(const QString& name)
    {
        window->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/") + name + ".png");
    }

public slots:
    void handleDialog()
    {
        for (auto* w : QApplication::topLevelWidgets()) {
            auto* d = qobject_cast<QDialog*>(w);
            if (!d || !d->isVisible()) continue;
            if (dialogMode == "file" && !qobject_cast<QFileDialog*>(d)) continue;
            if (dialogMode == "messages" && !qobject_cast<QMessageBox*>(d)) continue;
            if (dialogMode != "auto") dialogTimer.stop();
            dialogHandled = true;
            dialogClass = d->metaObject()->className();
            dialogTitle = d->windowTitle();
            qInfo() << "dialog" << dialogMode << dialogClass << dialogTitle;
            if (auto* mb = qobject_cast<QMessageBox*>(d)) {
                dialogMessages.append(mb->text());
                qInfo() << "message" << mb->text();
            }
            d->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/") + dialogClass + ".png");
            if (dialogMode == "file") {
                auto* fd = qobject_cast<QFileDialog*>(d);
                if (!fd) { d->reject(); return; }
                fd->selectFile(dialogPath);
                if (auto* name = fd->findChild<QLineEdit*>("fileNameEdit")) name->setText(dialogPath);
                QMetaObject::invokeMethod(fd, "accept", Qt::DirectConnection);
            } else if (dialogMode == "joseki-add" || dialogMode == "joseki-edit") {
                auto* moveDialog = qobject_cast<JosekiMoveDialog*>(d);
                QVERIFY(moveDialog);
                auto* buttons = d->findChild<QDialogButtonBox*>();
                QVERIFY(buttons);
                if (dialogMode == "joseki-add") {
                    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
                    moveDialog->setMove(QStringLiteral("2g2f"));
                } else {
                    QCOMPARE(moveDialog->move(), QStringLiteral("7g7f"));
                    auto* reply = d->findChild<JosekiMoveInputWidget*>("nextMoveInput");
                    QVERIFY(reply && reply->isVisible());
                }
                moveDialog->setNextMove(QStringLiteral("8c8d"));
                QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
                QCoreApplication::processEvents();
                QVERIFY(d->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/") + dialogMode + ".png"));
                QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            } else if (dialogMode == "bookmark") {
                auto* input = qobject_cast<QInputDialog*>(d);
                QVERIFY(input);
                input->setTextValue(dialogPath);
                auto* buttons = input->findChild<QDialogButtonBox*>();
                QVERIFY(buttons); QVERIFY(buttons->button(QDialogButtonBox::Ok));
                QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            } else if (dialogMode.startsWith("game")) {
                auto* p1 = d->findChild<QComboBox*>("comboBoxPlayer1");
                auto* p2 = d->findChild<QComboBox*>("comboBoxPlayer2");
                if (!p1 || !p2) { d->reject(); return; }
                p1->setCurrentIndex(0); p2->setCurrentIndex(0);
                if (dialogMode == "gameEngineBlack") p1->setCurrentIndex(1);
                if (dialogMode == "gameEngineWhite") p2->setCurrentIndex(1);
                if (dialogMode == "gameEngines") { p1->setCurrentIndex(1); p2->setCurrentIndex(1); }
                d->findChild<QLineEdit*>("lineEditHumanName1")->setText("Audit Black");
                d->findChild<QLineEdit*>("lineEditHumanName2")->setText("Audit White");
                if (auto* bb = d->findChild<QDialogButtonBox*>()) {
                    if (auto* ok = bb->button(QDialogButtonBox::Ok)) {
                        QTest::mouseClick(ok, Qt::LeftButton); return;
                    }
                }
                d->reject();
            } else if (dialogMode == "yes") {
                auto* mb = qobject_cast<QMessageBox*>(d);
                if (mb && mb->button(QMessageBox::Yes)) mb->button(QMessageBox::Yes)->click();
                else d->accept();
            } else if (dialogMode == "discard") {
                auto* mb = qobject_cast<QMessageBox*>(d);
                if (mb && mb->button(QMessageBox::Discard)) mb->button(QMessageBox::Discard)->click();
                else d->accept();
            } else if (dialogMode == "collection") {
                operateCollection(d);
            } else if (dialogMode == "startAnalysis") {
                auto* bb = d->findChild<QDialogButtonBox*>();
                if (!bb || !bb->button(QDialogButtonBox::Ok)) { d->reject(); return; }
                if (auto* time = d->findChild<QSpinBox*>("byoyomiSec")) time->setValue(1);
                QTest::mouseClick(bb->button(QDialogButtonBox::Ok), Qt::LeftButton);
            } else if (dialogMode == "register") {
                auto* add = d->findChild<QPushButton*>("addEngineButton");
                if (!add) { d->reject(); return; }
                armDialog("file", QStringLiteral(AUDIT_DIR "/mock_usi.py"));
                QTimer nested;
                connect(&nested, &QTimer::timeout, this, &GuiAudit::handleDialog);
                nested.start(30);
                QTest::mouseClick(add, Qt::LeftButton);
                nested.stop();
                QTest::qWait(100);
                d->close();
            } else if (dialogMode == "generator") {
                QPushButton *start = nullptr, *stop = nullptr;
                for (auto* b : d->findChildren<QPushButton*>()) {
                    if (b->text() == QStringLiteral("開始")) start = b;
                    if (b->text() == QStringLiteral("停止")) stop = b;
                }
                QVERIFY(start); QVERIFY(stop); QVERIFY(start->isEnabled());
                QTest::mouseClick(start, Qt::LeftButton); QTest::qWait(300);
                QVERIFY(stop->isEnabled()); QVERIFY(!start->isEnabled());
                QTest::mouseClick(stop, Qt::LeftButton);
                QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(), 2000);
                d->close();
            } else {
                d->reject();
            }
            return;
        }
    }
    void captureUrl(const QUrl& url) { urls.append(url); }
    void quitFromMenu() { click("actionQuit"); }
    void operateCollection(QDialog* d)
    {
        auto* sv = d->findChild<ShogiView*>(); QVERIFY(sv);
        QPushButton *open = nullptr, *next = nullptr, *select = nullptr;
        for (auto* b : d->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("ファイルを開く")) open = b;
            if (b->text().contains(QStringLiteral("次へ"))) next = b;
            if (b->text() == QStringLiteral("選択")) select = b;
        }
        QVERIFY(open); QVERIFY(next); QVERIFY(select);
        armDialog("file", QStringLiteral(REPO "/tests/fixtures/test_collection.sfen"));
        QTimer nested;
        connect(&nested, &QTimer::timeout, this, &GuiAudit::handleDialog);
        nested.start(30);
        QTest::mouseClick(open, Qt::LeftButton); QVERIFY(dialogHandled);
        nested.stop();
        QCOMPARE(sv->board()->convertBoardToSfen(), initial);
        QVERIFY(next->isEnabled()); QTest::mouseClick(next, Qt::LeftButton);
        selectedCollection = sv->board()->convertBoardToSfen(); QVERIFY(selectedCollection != initial);
        QTest::mouseClick(select, Qt::LeftButton);
        d->close();
    }
private slots:
    void initTestCase()
    {
        connect(&dialogTimer, &QTimer::timeout, this, &GuiAudit::handleDialog);
        QDesktopServices::setUrlHandler("https", this, "captureUrl");
        QDesktopServices::setUrlHandler("http", this, "captureUrl");
    }
    void init()
    {
        SettingsCommon::openSettings().clear();
        const QString test = QTest::currentTestFunction();
        if (test.startsWith("engine") || test == "consideration") {
            auto& settings = SettingsCommon::openSettings();
            settings.beginWriteArray("Engines"); settings.setArrayIndex(0);
            settings.setValue("name", "Audit USI");
            settings.setValue("path", QStringLiteral(AUDIT_DIR "/mock_usi.py"));
            settings.setValue("author", "GUI audit fixture");
            settings.endArray(); settings.sync();
        }
        window = std::make_unique<MainWindow>();
        window->resize(1400, 1000);
        window->show();
        QTest::qWait(50);
        QVERIFY(board()); QVERIFY(record());
        urls.clear();
        dialogMessages.clear();
    }
    void cleanup()
    {
        dialogTimer.stop();
        if (window && QTest::currentTestFailed()) snapshot(QString("failure-") + QTest::currentTestFunction());
        for (auto* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<QDialog*>(w)) d->reject();
        if (window) {
            // 終局した棋譜も未保存になるため、終了確認に応答する。
            armDialog("discard");
            window->close();
            dialogTimer.stop();
        }
        QCoreApplication::processEvents();
        window.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void cleanupTestCase()
    {
        QDesktopServices::unsetUrlHandler("http");
        QDesktopServices::unsetUrlHandler("https");
        clicked.removeDuplicates();
        QFile f(QStringLiteral(AUDIT_DIR "/clicked-actions.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonArray::fromStringList(clicked)).toJson());
    }
    void startupMenuInventory()
    {
        QCOMPARE(boardSfen(), initial);
        QCOMPARE(window->menuBar()->actions().size(), 6);
        QJsonArray menus;
        for (auto* menu : window->findChildren<QMenu*>()) {
            QJsonArray items;
            for (auto* a : menu->actions()) {
                if (a->isSeparator()) continue;
                items.append(QJsonObject{{"name", a->objectName()}, {"text", a->text()},
                    {"enabled", a->isEnabled()}, {"visible", a->isVisible()},
                    {"submenu", a->menu() != nullptr}});
            }
            menus.append(QJsonObject{{"menu", menu->title()}, {"name", menu->objectName()}, {"actions", items}});
        }
        QFile f(QStringLiteral(AUDIT_DIR "/menus.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(menus).toJson());
        snapshot("startup");
    }
    void usiLogPresentation()
    {
        sampleGame();
        auto* dock = window->findChild<QDockWidget*>("UsiLogDock");
        auto* panel = window->findChild<UsiLogPanel*>();
        QVERIFY(dock);
        QVERIFY(panel);
        dock->show();
        dock->raise();
        UsiCommLogModel first;
        UsiCommLogModel second;
        first.setEngineName(QStringLiteral("Hayanagi 1.5.0"));
        second.setEngineName(QStringLiteral("Engine 2"));
        panel->setModels(&first, &second);
        first.appendUsiCommLog(QStringLiteral("▶ E1: position startpos moves 7g7f 3c3d 2g2f 8c8d"));
        first.appendUsiCommLog(QStringLiteral("▶ E1: go btime 278000 wtime 280000 byoyomi 3000"));
        for (int i = 1; i <= 9; ++i) {
            first.appendUsiCommLog(QStringLiteral("◀ E1: info depth %1 seldepth %2 score cp -16 nodes %3 time %4 nps 313918 hashfull 45 pv 2h5h 3a4b 2g2f 8c8d 7g7f")
                .arg(i).arg(i + 3).arg(i * 35801).arg(i * 100));
        }
        first.appendUsiCommLog(QStringLiteral("◀ E1: bestmove 2h5h"));
        second.appendUsiCommLog(QStringLiteral("◀ E2: readyok"));
        QTest::qWait(50);
        auto* view = dock->findChild<QPlainTextEdit*>("usiLogView");
        auto* input = dock->findChild<QLineEdit*>("usiCommandInput");
        QVERIFY(view && input);
        QVERIFY(input->mapTo(dock, QPoint()).y() > view->mapTo(dock, QPoint()).y() + view->height());
        snapshot("usi-log-main");
        first.setEngineName(QStringLiteral("Hayanagi 1.5.0 / Custom configuration with a long name"));
        dock->setFloating(true);
        dock->resize(560, 350);
        QTest::qWait(50);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/usi-log-narrow.png")));
        dock->findChild<QAction*>("usiLogWrap")->trigger();
        QCOMPARE(view->lineWrapMode(), QPlainTextEdit::WidgetWidth);
        QTest::qWait(50);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/usi-log-wrapped.png")));
        panel->setModels(nullptr, nullptr);
    }
    void windowWidthFollowsContent()
    {
        armDialog("file", QStringLiteral(REPO "/tests/fixtures/test_basic.kif"));
        click("actionOpenKifuFile");
        QVERIFY(dialogHandled);
        QTRY_VERIFY(record()->kifuView()->model()->rowCount() > 1);
        record()->onToggleBookmarkColumn(false);
        record()->onToggleCommentColumn(false);
        record()->onToggleTimeColumn(true);
        auto* table = record()->kifuView();
        auto* dock = window->findChild<QDockWidget*>("RecordPaneDock");
        QVERIFY(dock);

        QTRY_COMPARE(table->viewport()->width(), table->horizontalHeader()->length());
        QTRY_COMPARE(dock->width(), record()->maximumWidth());
        QTRY_COMPARE(window->width(), window->layout()->minimumSize().width());
        QTRY_COMPARE(board()->mapTo(window.get(), QPoint()).x(), window->contentsRect().left());
        QTRY_COMPARE(window->minimumWidth(), window->maximumWidth());
        const int initialWidth = window->width();
        const int initialHeight = window->height();
        window->resize(initialWidth + 200, initialHeight + 100);
        QCOMPARE(window->width(), initialWidth);
        QCOMPARE(window->height(), initialHeight + 100);
        QCOMPARE(board()->mapTo(window.get(), QPoint()).x(), window->contentsRect().left());
        const int boardRight = board()->mapTo(window.get(), QPoint(board()->width(), 0)).x();
        QVERIFY(dock->x() - boardRight < 10);
        snapshot("fixed-window-width");

        for (int i = 0; i < 6; ++i) QTest::mouseClick(record()->fontIncreaseButton(), Qt::LeftButton);
        QTRY_VERIFY(window->width() > initialWidth);
        QTRY_COMPARE(table->horizontalScrollBar()->maximum(), 0);
        const QString time = table->model()->index(1, 1).data().toString();
        QVERIFY(!time.isEmpty());
        QVERIFY(table->columnWidth(1) >= table->fontMetrics().horizontalAdvance(time) + 6);
        QVERIFY(table->visualRect(table->model()->index(1, 1)).right() < table->viewport()->width());
        QCOMPARE(window->minimumWidth(), window->maximumWidth());
        snapshot("fixed-window-large-kifu-font");

        const int largeFontWidth = window->width();
        board()->enlargeBoard();
        QTRY_VERIFY(window->width() > largeFontWidth);
        board()->reduceBoard();
        QTRY_COMPARE(window->width(), largeFontWidth);

        record()->onToggleTimeColumn(false);
        QTRY_VERIFY(window->width() < largeFontWidth);
        record()->onToggleTimeColumn(true);
        QTRY_COMPARE(window->width(), largeFontWidth);
        for (int i = 0; i < 6; ++i) QTest::mouseClick(record()->fontDecreaseButton(), Qt::LeftButton);
        QTRY_COMPARE(window->width(), initialWidth);

        // しおり・コメントを併用しても指し手と消費時間は全体を表示する。
        record()->onToggleBookmarkColumn(true);
        record()->onToggleCommentColumn(true);
        QTRY_COMPARE(table->horizontalScrollBar()->maximum(), 0);
        QVERIFY(table->visualRect(table->model()->index(1, 1)).right() < table->viewport()->width());
    }
    void boardZoomPreservesWindowState_data()
    {
        QTest::addColumn<bool>("fullScreen");
        QTest::addColumn<bool>("enlargeFirst");
        QTest::newRow("maximized-enlarge") << false << true;
        QTest::newRow("maximized-shrink") << false << false;
        QTest::newRow("fullscreen-enlarge") << true << true;
        QTest::newRow("fullscreen-shrink") << true << false;
    }
    void boardZoomPreservesWindowState()
    {
        QFETCH(bool, fullScreen);
        QFETCH(bool, enlargeFirst);
        QTRY_COMPARE(window->width(), window->layout()->minimumSize().width());
        const int normalWidth = window->width();
        const Qt::WindowState state = fullScreen ? Qt::WindowFullScreen : Qt::WindowMaximized;
        if (fullScreen) window->showFullScreen();
        else window->showMaximized();
        QTest::qWait(100);
        QVERIFY(window->windowState().testFlag(state));
        const QRect geometry = window->geometry();

        // 拡大・縮小を交互にメニューから実行し、盤だけが変化することを確認する。
        for (int i = 0; i < 3; ++i) {
            const bool enlarge = (i % 2 == 0) == enlargeFirst;
            const int fieldWidth = board()->fieldSize().width();
            click(enlarge ? "actionEnlargeBoard" : "actionShrinkBoard");
            QTRY_COMPARE(board()->fieldSize().width(), fieldWidth + (enlarge ? 1 : -1));
            QTest::qWait(100);
            QVERIFY(window->windowState().testFlag(state));
            QCOMPARE(window->geometry(), geometry);
        }
        snapshot(QStringLiteral("board-zoom-") + QTest::currentDataTag());

        // 通常表示に戻すと、変更後の盤サイズに合わせて横幅を固定し直す。
        window->showNormal();
        QTRY_COMPARE(window->width(), window->layout()->minimumSize().width());
        QTRY_COMPARE(window->minimumWidth(), window->maximumWidth());
        QVERIFY(!window->isMaximized() && !window->isFullScreen());
        QVERIFY(enlargeFirst ? window->width() > normalWidth : window->width() < normalWidth);
        click(enlargeFirst ? "actionShrinkBoard" : "actionEnlargeBoard");
        QTRY_COMPARE(window->width(), normalWidth);
    }
    void pieceStyles()
    {
        QVERIFY(!window->findChild<QMenu*>("menuPieceStyle"));
        QVERIFY(!action("actionPieceStyleStandard"));
        selectPieceStyle(QStringLiteral("standard"));
        QVERIFY(!hasKifuPasteDialog());
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        const auto pawn = QIcon(":/pieces/Sente_fu45.svg").pixmap(90).toImage();
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);
        ShogiView secondary;
        QCOMPARE(secondary.piece('P').pixmap(90).toImage(), pawn);
        QCOMPARE(boardSfen(), initial);
        snapshot("pieces-standard");

        click("actionFlipBoard");
        QCOMPARE(board()->piece('K').pixmap(90).toImage(),
                 QIcon(":/pieces/Gote_ou45.svg").pixmap(90).toImage());
        QCOMPARE(board()->piece('k').pixmap(90).toImage(),
                 QIcon(":/pieces/Sente_gyoku45.svg").pixmap(90).toImage());
        selectPieceStyle(QStringLiteral("standard"));
        QVERIFY(board()->flipMode());
        snapshot("pieces-standard-flipped");
        click("actionFlipBoard");

        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);

        // 成駒・持ち駒・駒打ち矢印も標準の駒で描画する。
        board()->board()->setSfen(QStringLiteral(
            "4k4/9/3+r+b+s+n+l+p/9/9/9/+P+L+N+S+B+R3/9/4K4 b 2GSNL8P2gsnl8p 1"));
        ShogiView::Arrow drop;
        drop.toFile = 5;
        drop.toRank = 5;
        drop.dropPiece = 'P';
        board()->setArrows({drop});
        const auto position = board()->toImage();
        QVERIFY(!position.isNull());
        board()->setPieces();
        QCOMPARE(board()->toImage(), position);
        snapshot("pieces-standard-promoted-and-hand");
    }
    void pieceVariants_data()
    {
        QTest::addColumn<QString>("style");
        for (const auto& style : AppSettings::availablePieceStyles())
            if (style != QStringLiteral("standard")) QTest::newRow(qPrintable(style)) << style;
    }
    void pieceVariants()
    {
        QFETCH(QString, style);
        ShogiView secondary;
        const auto standardPawn = board()->piece('P').pixmap(90).toImage();
        selectPieceStyle(style);
        QCOMPARE(AppSettings::pieceStyle(), style);
        QVERIFY(!hasKifuPasteDialog());
        const QString prefix = QStringLiteral(":/pieces/%1/").arg(style);
        const auto pawn = QIcon(prefix + "Sente_fu45.svg").pixmap(90).toImage();
        QVERIFY(pawn != standardPawn);
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);
        QCOMPARE(secondary.piece('P').pixmap(90).toImage(), pawn);
        QVERIFY(board()->boardColors() == BoardColors{});
        QCOMPARE(boardSfen(), initial);
        QVERIFY(board()->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/pieces-%1.png").arg(style)));

        click("actionFlipBoard");
        QCOMPARE(board()->piece('K').pixmap(90).toImage(), QIcon(prefix + "Gote_ou45.svg").pixmap(90).toImage());
        QCOMPARE(board()->piece('k').pixmap(90).toImage(), QIcon(prefix + "Sente_gyoku45.svg").pixmap(90).toImage());
        selectPieceStyle(QStringLiteral("standard"));
        selectPieceStyle(style);
        click("actionFlipBoard");

        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);

        // 開いている配色ダイアログの表示名・見本も選択した駒に追従する。
        openAppearanceDetails();
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* label = dialog->findChild<QLabel*>("boardColorPresetLabel");
        QVERIFY(label && label->text().contains(BoardColorPresets::pieceStyleName(style)));
        dialog->close();

        // キャッシュを作った後でも成駒・持駒・駒打ち矢印が切り替わる。
        board()->board()->setSfen(QStringLiteral(
            "4k4/9/3+r+b+s+n+l+p/9/9/9/+P+L+N+S+B+R3/9/4K4 b 2GSNL8P2gsnl8p 1"));
        ShogiView::Arrow drop;
        drop.toFile = 5;
        drop.toRank = 5;
        drop.dropPiece = 'P';
        board()->setArrows({drop});
        const auto variantImage = board()->toImage();
        QVERIFY(!variantImage.isNull());
        selectPieceStyle(QStringLiteral("standard"));
        QVERIFY(board()->toImage() != variantImage);
        selectPieceStyle(style);
        QCOMPARE(board()->toImage(), variantImage);
    }
    void appearanceComponents_data()
    {
        QTest::addColumn<int>("componentIndex");
        QTest::addColumn<QString>("listName");
        QTest::addColumn<int>("tabIndex");
        QTest::newRow("boards") << 0 << QStringLiteral("appearanceBoards") << 1;
        QTest::newRow("stands") << 1 << QStringLiteral("appearanceStands") << 3;
        QTest::newRow("information") << 2 << QStringLiteral("appearanceInformation") << 4;
        QTest::newRow("backgrounds") << 3 << QStringLiteral("appearanceBackgrounds") << 2;
    }
    void appearanceComponents()
    {
        QFETCH(int, componentIndex);
        QFETCH(QString, listName);
        QFETCH(int, tabIndex);
        click("actionBoardAppearance");
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* sections = dialog->findChild<QTabWidget*>("appearanceSections");
        sections->setCurrentIndex(tabIndex);
        auto* list = dialog->findChild<QListWidget*>(listName);
        auto* preview = dialog->findChild<BoardAppearancePreview*>();
        QVERIFY(list && preview);
        QCOMPARE(list->count(), 20);
        const auto component = static_cast<BoardAppearanceCatalog::Component>(componentIndex);
        const auto samples = BoardAppearanceCatalog::samples(component);
        ShogiView secondary;
        for (int row = 0; row < list->count(); ++row) {
            auto colors = board()->boardColors();
            auto visuals = board()->boardVisuals();
            const auto previousImage = preview->image();
            BoardAppearanceCatalog::apply(component, samples.at(row), colors, visuals);
            auto* item = list->item(row);
            list->scrollToItem(item);
            QTest::qWait(20);
            QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(item).center());
            QCOMPARE(list->currentRow(), row);
            QVERIFY(board()->boardColors() == colors);
            QVERIFY(board()->boardVisuals() == visuals);
            QVERIFY(secondary.boardColors() == colors);
            QVERIFY(secondary.boardVisuals() == visuals);
            if (row > 0) QVERIFY(preview->image() != previousImage);
            QCOMPARE(boardSfen(), initial);
        }
        QVERIFY(dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/appearance-%1.png").arg(listName)));
        const auto expectedColors = board()->boardColors();
        const auto expectedVisuals = board()->boardVisuals();
        dialog->resize(1120, 760);
        const auto expectedSize = dialog->size();
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        click("actionBoardAppearance");
        dialog = window->findChild<BoardColorDialog*>();
        QCOMPARE(dialog->size(), expectedSize);
        QCOMPARE(dialog->findChild<QTabWidget*>("appearanceSections")->currentIndex(), tabIndex);
        QCOMPARE(dialog->findChild<QListWidget*>(listName)->currentRow(), 19);
        QVERIFY(board()->boardColors() == expectedColors);
        QVERIFY(board()->boardVisuals() == expectedVisuals);
    }
    void appearanceBoardBackground()
    {
        click("actionBoardAppearance");
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* sections = dialog->findChild<QTabWidget*>("appearanceSections");
        auto* boards = dialog->findChild<QListWidget*>("appearanceBoards");
        auto* backgrounds = dialog->findChild<QListWidget*>("appearanceBackgrounds");
        auto* combination = dialog->findChild<QComboBox*>("appearanceCombination");
        QVERIFY(sections && boards && backgrounds && combination);
        QVERIFY(QMetaObject::invokeMethod(combination, "activated", Q_ARG(int, 2)));
        auto custom = board()->boardColors();
        custom.background = QColor("#123456");
        BoardAppearance::instance().setColors(custom);
        QCOMPARE(backgrounds->currentRow(), -1);
        QCOMPARE(boards->currentRow(), 8);
        QCOMPARE(combination->currentIndex(), -1);
        sections->setCurrentIndex(1);
        auto* boardItem = boards->item(1);
        boards->scrollToItem(boardItem);
        QTest::qWait(30);
        QTest::mouseClick(boards->viewport(), Qt::LeftButton, Qt::NoModifier, boards->visualItemRect(boardItem).center());
        QCOMPARE(boards->currentRow(), 1);
        QCOMPARE(board()->boardColors().background, custom.background);
        QCOMPARE(backgrounds->currentRow(), -1);
        const auto boardIcon = boardItem->icon().pixmap(142, 86).toImage();
        auto expected = board()->boardColors();
        const auto expectedVisuals = board()->boardVisuals();
        const auto previousImage = board()->toImage();
        sections->setCurrentIndex(2);
        auto* backgroundItem = backgrounds->item(15);
        backgrounds->scrollToItem(backgroundItem);
        QTest::qWait(30);
        QTest::mouseClick(backgrounds->viewport(), Qt::LeftButton, Qt::NoModifier,
                         backgrounds->visualItemRect(backgroundItem).center());
        QCOMPARE(backgrounds->currentRow(), 15);
        QCOMPARE(boards->currentRow(), 1);
        expected.background = QColor("#354957");
        QVERIFY(board()->boardColors() == expected);
        QVERIFY(board()->boardVisuals() == expectedVisuals);
        QCOMPARE(boardItem->icon().pixmap(142, 86).toImage(), boardIcon);
        QVERIFY(board()->toImage() != previousImage);
        QCOMPARE(boardSfen(), initial);
        QVERIFY(dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/appearance-background.png")));

        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        click("actionBoardAppearance");
        dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(board()->boardColors() == expected);
        QVERIFY(board()->boardVisuals() == expectedVisuals);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceBoards")->currentRow(), 1);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceBackgrounds")->currentRow(), 15);
        QCOMPARE(dialog->findChild<QTabWidget*>("appearanceSections")->currentIndex(), 2);
        QCOMPARE(dialog->findChild<QComboBox*>("appearanceCombination")->currentIndex(), -1);
    }
    void appearanceWindow()
    {
        click("actionBoardAppearance");
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* preview = dialog->findChild<BoardAppearancePreview*>();
        auto* pieces = dialog->findChild<QListWidget*>("appearancePieces");
        auto* combinations = dialog->findChild<QComboBox*>("appearanceCombination");
        auto* filter = dialog->findChild<QComboBox*>("appearancePieceFilter");
        QVERIFY(preview && pieces && combinations && filter);
        for (int family = 0; family < filter->count(); ++family) {
            filter->setCurrentIndex(family);
            int visible = 0;
            for (int i = 0; i < pieces->count(); ++i) if (!pieces->item(i)->isHidden()) ++visible;
            QCOMPARE(visible, family == 0 ? 21 : 5);
        }
        filter->setCurrentIndex(0);
        combinations->showPopup();
        QTest::qWait(30);
        auto* list = combinations->view();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                         list->visualRect(list->model()->index(2, 0)).center());
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("deep_ebony"));
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceBoards")->currentRow(), 8);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceStands")->currentRow(), 8);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceInformation")->currentRow(), 16);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceBackgrounds")->currentRow(), 8);
        filter->setCurrentIndex(4);
        QTest::qWait(30);
        QVERIFY(dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/appearance-window-ebony.png")));
        auto* position = dialog->findChild<QComboBox*>("appearancePreviewPosition");
        auto* flip = dialog->findChild<QPushButton*>("appearancePreviewFlip");
        const auto initialImage = preview->image();
        position->setCurrentIndex(1);
        QVERIFY(preview->image() != initialImage);
        const auto middle = preview->image();
        QTest::mouseClick(flip, Qt::LeftButton);
        QVERIFY(preview->image() != middle);
        QCOMPARE(boardSfen(), initial);
        QVERIFY(!board()->flipMode());
        QTest::mouseClick(dialog->findChild<QPushButton*>("restoreOpeningAppearanceButton"), Qt::LeftButton);
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        QVERIFY(board()->boardColors() == BoardColors{});
        QVERIFY(board()->boardVisuals() == BoardVisuals{});
        // 標準外の色にすると見本の選択表示が外れる。
        auto custom = board()->boardColors();
        custom.stand = QColor("#123456");
        BoardAppearance::instance().setColors(custom);
        QCOMPARE(dialog->findChild<QListWidget*>("appearanceStands")->currentRow(), -1);
        QCOMPARE(combinations->currentIndex(), -1);
    }
    void boardFlipKeepsLayout()
    {
        const QStringList blackInfo = {"blackPlayerCard", "blackNameLabel", "blackClockLabel", "turnLabelBlack"};
        const QStringList whiteInfo = {"whitePlayerCard", "whiteNameLabel", "whiteClockLabel", "turnLabelWhite"};
        for (const int size : {30, 53, 100}) {
            board()->setSquareSize(size - 1);
            click("actionEnlargeBoard");
            QTest::qWait(50);
            QVERIFY(!board()->flipMode());
            const QRect windowRect = window->geometry();
            const QRect viewRect(board()->mapToGlobal(QPoint()), board()->size());
            const auto normalLayout = editLayout();
            QList<QRect> leftInfoRects, rightInfoRects;
            for (qsizetype i = 0; i < blackInfo.size(); ++i) {
                auto* black = board()->findChild<QWidget*>(blackInfo.at(i));
                auto* white = board()->findChild<QWidget*>(whiteInfo.at(i));
                QVERIFY(black && white);
                rightInfoRects.append(black->geometry());
                leftInfoRects.append(white->geometry());
            }
            for (int iteration = 0; iteration < 12; ++iteration) {
                click("actionFlipBoard");
                const bool flipped = iteration % 2 == 0;
                QCOMPARE(board()->flipMode(), flipped);
                QCOMPARE(window->geometry(), windowRect);
                QCOMPARE(QRect(board()->mapToGlobal(QPoint()), board()->size()), viewRect);
                const auto layout = editLayout();
                QCOMPARE(layout.boardSurfaceRect(9, 9), normalLayout.boardSurfaceRect(9, 9));
                QCOMPARE(flipped ? layout.blackStandBoundingRect(9, 9) : layout.whiteStandBoundingRect(9, 9),
                         normalLayout.whiteStandBoundingRect(9, 9));
                QCOMPARE(flipped ? layout.whiteStandBoundingRect(9, 9) : layout.blackStandBoundingRect(9, 9),
                         normalLayout.blackStandBoundingRect(9, 9));
                for (qsizetype i = 0; i < blackInfo.size(); ++i) {
                    QCOMPARE(board()->findChild<QWidget*>(flipped ? blackInfo.at(i) : whiteInfo.at(i))->geometry(),
                             leftInfoRects.at(i));
                    QCOMPARE(board()->findChild<QWidget*>(flipped ? whiteInfo.at(i) : blackInfo.at(i))->geometry(),
                             rightInfoRects.at(i));
                }
                // 同じ画面座標が、反転後の対応するマスを指すことも確認する。
                for (int file = 1; file <= 9; ++file) {
                    for (int rank = 1; rank <= 9; ++rank) {
                        const QPoint point = normalLayout.calculateSquareRectangleBasedOnBoardState(file, rank, 9, 9)
                            .translated(normalLayout.offsetX(), normalLayout.offsetY()).center();
                        QCOMPARE(board()->clickedSquare(point), flipped ? QPoint(10 - file, 10 - rank) : QPoint(file, rank));
                    }
                }
                QCOMPARE(boardSfen(), initial);
                if (size == 53 && iteration < 2) {
                    QVERIFY(board()->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/board-flip-%1.png")
                                                    .arg(flipped ? "flipped" : "normal")));
                }
            }
        }
    }

    void boardThemes()
    {
        ShogiView secondary;
        openAppearanceDetails();
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* tabs = dialog->findChild<QTabWidget*>("boardColorTabs");
        tabs->setCurrentIndex(5);
        auto* combo = dialog->findChild<QComboBox*>("boardThemeCombo");
        QVERIFY(combo);
        QCOMPARE(combo->count(), 3);
        for (int i = 0; i < combo->count(); ++i) {
            combo->showPopup();
            QTest::qWait(20);
            auto* list = combo->view();
            const auto index = list->model()->index(i, 0);
            list->scrollTo(index);
            QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualRect(index).center());
            QTRY_COMPARE(combo->currentIndex(), i);
            QVERIFY(board()->boardColors() == BoardColorPresets::themes().at(i).colors);
            QVERIFY(secondary.boardColors() == board()->boardColors());
            QVERIFY(board()->boardVisuals() == BoardVisuals{});
            QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
            QVERIFY(board()->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/board-theme-%1.png").arg(i)));
            QCOMPARE(boardSfen(), initial);
        }
        auto* grain = dialog->findChild<QCheckBox*>("boardWoodGrain");
        auto* shadow = dialog->findChild<QCheckBox*>("boardPieceShadow");
        auto* scale = dialog->findChild<QSpinBox*>("boardPieceScale");
        QVERIFY(grain && shadow && scale);
        const auto textured = board()->grab().toImage();
        QTest::mouseClick(grain, Qt::LeftButton, Qt::NoModifier, QPoint(8, grain->height() / 2));
        QTest::mouseClick(shadow, Qt::LeftButton, Qt::NoModifier, QPoint(8, shadow->height() / 2));
        scale->setValue(94);
        QVERIFY(board()->boardVisuals() == (BoardVisuals{false, false, 94}));
        QVERIFY(board()->grab().toImage() != textured);
        QCOMPARE(combo->currentIndex(), -1);
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(board()->boardVisuals() == (BoardVisuals{false, false, 94}));
        openAppearanceDetails();
        dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        combo = dialog->findChild<QComboBox*>("boardThemeCombo");
        QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, 0)));
        QVERIFY(board()->boardVisuals() == BoardVisuals{});
        QVERIFY(board()->boardColors() == BoardColors{});
        dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/board-appearance-dialog.png"));
        guard = dialog;
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        snapshot("board-appearance-main");
        click("actionFlipBoard");
        QVERIFY(board()->flipMode());
        snapshot("board-appearance-flipped");
        QCOMPARE(boardSfen(), initial);
    }

    void boardColors()
    {
        BoardAppearance::instance().setVisuals({false, true, 108, false});
        AppSettings::setBoardColorDialogTab(0);
        const BoardColors defaults;
        const BoardColors custom{QColor("#182838"), QColor("#c0d8d0"),
                                 QColor("#6a8494"), QColor("#193b45")};
        ShogiView secondary;
        QSignalSpy changed(&BoardAppearance::instance(), &BoardAppearance::colorsChanged);
        selectPieceStyle(QStringLiteral("standard"));
        openAppearanceDetails();
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog && dialog->isVisible());
        auto* picker = dialog->findChild<QColorDialog*>("boardColorPicker");
        QVERIFY(picker);
        const QStringList names = {"backgroundColorButton", "boardColorButton", "standColorButton", "gridColorButton"};
        const QList<QColor> values = {custom.background, custom.board, custom.stand, custom.grid};
        board()->setUrgencyVisuals(ShogiView::Urgency::Warn5);
        const QString activeStyle = board()->blackClockLabel()->styleSheet();
        for (qsizetype i = 0; i < names.size(); ++i) {
            auto* button = dialog->findChild<QPushButton*>(names.at(i));
            QVERIFY(button);
            QTest::mouseClick(button, Qt::LeftButton);
            QTRY_VERIFY(picker->isVisible());
            picker->setCurrentColor(values.at(i));
            auto* pickerButtons = picker->findChild<QDialogButtonBox*>();
            QVERIFY(pickerButtons);
            QTest::mouseClick(pickerButtons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            QTRY_VERIFY(!picker->isVisible());
            QCOMPARE(button->text(), values.at(i).name().toUpper());
        }
        QCOMPARE(changed.count(), 4);
        QVERIFY(board()->boardColors() == custom);
        QVERIFY(secondary.boardColors() == custom);
        QVERIFY(AppSettings::boardColors() == custom);
        QCOMPARE(board()->blackClockLabel()->styleSheet(), activeStyle);
        QCOMPARE(board()->whiteClockLabel()->palette().color(QPalette::WindowText), BoardColors{}.clockText);
        QCOMPARE(boardSfen(), initial);
        dialog->resize(580, 360);
        dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/board-colors-dialog.png"));

        // 色選択のキャンセルは設定・表示を変更しない。
        QTest::mouseClick(dialog->findChild<QPushButton*>(names.first()), Qt::LeftButton);
        QTRY_VERIFY(picker->isVisible());
        picker->setCurrentColor(Qt::magenta);
        picker->resize(700, 460);
        const QSize pickerSize = picker->size();
        picker->reject();
        QCOMPARE(changed.count(), 4);
        QVERIFY(board()->boardColors() == custom);
        QCOMPARE(AppSettings::boardColorPickerSize(), pickerSize);
        const QSize dialogSize = dialog->size();
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        QCOMPARE(AppSettings::boardColorDialogSize(), dialogSize);
        const auto image = board()->grab().toImage();
        for (const auto& color : values) {
            int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (image.pixelColor(x, y) == color) ++count;
            QVERIFY2(count > 100, qPrintable(color.name()));
        }
        QCOMPARE(image.pixelColor(squarePoint(5, 5)), custom.board);
        click("actionCopyBoardToClipboard");
        QCOMPARE(QApplication::clipboard()->image(), board()->grab().toImage());
        snapshot("board-colors-custom");
        click("actionFlipBoard");
        QVERIFY(board()->boardColors() == custom);
        QCOMPARE(board()->grab().toImage().pixelColor(squarePoint(5, 5)), custom.board);

        // 再作成した盤面・ダイアログにも設定が復元される。
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(board()->boardColors() == custom);
        openAppearanceDetails();
        dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        QCOMPARE(dialog->size(), dialogSize);
        QCOMPARE(dialog->findChild<QPushButton*>(names.first())->text(), custom.background.name().toUpper());
        auto* reset = dialog->findChild<QPushButton*>("resetBoardColorsButton");
        QVERIFY(reset);
        QTest::mouseClick(reset, Qt::LeftButton);
        QVERIFY(AppSettings::boardColors() == defaults);
        QVERIFY(board()->boardColors() == defaults);
        QVERIFY(secondary.boardColors() == defaults);
        QCOMPARE(changed.count(), 5);
        QTest::mouseClick(reset, Qt::LeftButton);
        QCOMPARE(changed.count(), 5);
        QCOMPARE(boardSfen(), initial);
    }
    void boardInformationColors()
    {
        ShogiView secondary;
        openAppearanceDetails();
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* tabs = dialog->findChild<QTabWidget*>("boardColorTabs");
        auto* picker = dialog->findChild<QColorDialog*>("boardColorPicker");
        QVERIFY(tabs && picker);
        QCOMPARE(tabs->count(), 6);
        struct Choice {
            const char* button;
            BoardColors::Member member;
            QColor color;
            int tab;
        };
        const QList<Choice> choices = {
            {"cardBackgroundColorButton", &BoardColors::cardBackground, QColor("#dde6f0"), 1},
            {"cardBorderColorButton", &BoardColors::cardBorder, QColor("#647589"), 1},
            {"activeCardBorderColorButton", &BoardColors::activeCardBorder, QColor("#235da1"), 1},
            {"turnBackgroundColorButton", &BoardColors::turnBackground, QColor("#315e97"), 2},
            {"turnBorderColorButton", &BoardColors::turnBorder, QColor("#112f58"), 2},
            {"turnTextColorButton", &BoardColors::turnText, QColor("#fff4ba"), 2},
            {"nameBackgroundColorButton", &BoardColors::nameBackground, QColor("#fae8ce"), 3},
            {"nameBorderColorButton", &BoardColors::nameBorder, QColor("#af7e48"), 3},
            {"nameTextColorButton", &BoardColors::nameText, QColor("#593f2b"), 3},
            {"clockBackgroundColorButton", &BoardColors::clockBackground, QColor("#e4efd3"), 4},
            {"clockBorderColorButton", &BoardColors::clockBorder, QColor("#788f54"), 4},
            {"clockTextColorButton", &BoardColors::clockText, QColor("#345b3d"), 4},
            {"clockWarningTextColorButton", &BoardColors::clockWarningText, QColor("#996722"), 4},
            {"clockCriticalTextColorButton", &BoardColors::clockCriticalText, QColor("#aa2961"), 4}
        };
        auto expected = board()->boardColors();
        board()->setActiveSide(true);
        for (const auto& choice : choices) {
            tabs->setCurrentIndex(choice.tab);
            auto* button = dialog->findChild<QPushButton*>(QLatin1String(choice.button));
            QVERIFY(button && button->isVisible());
            QTest::mouseClick(button, Qt::LeftButton);
            QTRY_VERIFY(picker->isVisible());
            QCOMPARE(picker->testOption(QColorDialog::ShowAlphaChannel), BoardColors::supportsTransparency(choice.member));
            picker->setCurrentColor(choice.color);
            auto* buttons = picker->findChild<QDialogButtonBox*>();
            QVERIFY(buttons);
            QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            QTRY_VERIFY(!picker->isVisible());
            expected.*choice.member = choice.color;
            QVERIFY(AppSettings::boardColors() == expected);
            QVERIFY(board()->boardColors() == expected);
            QVERIFY(secondary.boardColors() == expected);
            QCOMPARE(button->text(), choice.color.name().toUpper());
        }
        QCOMPARE(board()->blackNameLabel()->palette().color(QPalette::WindowText), expected.nameText);
        QCOMPARE(board()->blackClockLabel()->palette().color(QPalette::WindowText), expected.clockText);
        QCOMPARE(board()->findChild<QLabel*>("turnLabelBlack")->palette().color(QPalette::WindowText), expected.turnText);
        board()->setUrgencyVisuals(ShogiView::Urgency::Warn5);
        QCOMPARE(board()->blackClockLabel()->palette().color(QPalette::WindowText), expected.clockCriticalText);
        snapshot("board-information-colors");
        dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/board-information-colors-dialog.png"));

        // おすすめ配色は盤面の4色だけを変更し、表示色は保持する。
        tabs->setCurrentIndex(0);
        auto* combo = dialog->findChild<QComboBox*>("boardColorPresetCombo");
        QVERIFY(combo);
        combo->showPopup();
        QTest::qWait(20);
        auto* list = combo->view();
        const auto index = list->model()->index(0, 0);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualRect(index).center());
        const auto preset = BoardColorPresets::palettes().first().colors;
        expected.background = preset.background;
        expected.board = preset.board;
        expected.stand = preset.stand;
        expected.grid = preset.grid;
        QVERIFY(board()->boardColors() == expected);
        QCOMPARE(combo->currentIndex(), 0);
        QCOMPARE(board()->blackClockLabel()->palette().color(QPalette::WindowText), expected.clockCriticalText);

        // 半透明の保存、キャンセル、プリセットの選択表示を確認する。
        tabs->setCurrentIndex(3);
        auto* nameBackground = dialog->findChild<QPushButton*>("nameBackgroundColorButton");
        QTest::mouseClick(nameBackground, Qt::LeftButton);
        QTRY_VERIFY(picker->isVisible());
        picker->setCurrentColor(QColor(255, 255, 255, 128));
        picker->reject();
        QVERIFY(board()->boardColors() == expected);
        QTest::mouseClick(nameBackground, Qt::LeftButton);
        QTRY_VERIFY(picker->isVisible());
        expected.nameBackground = QColor(255, 255, 255, 128);
        picker->setCurrentColor(expected.nameBackground);
        picker->accept();
        QVERIFY(board()->boardColors() == expected);
        QCOMPARE(nameBackground->text(), expected.nameBackground.name(QColor::HexArgb).toUpper());
        QCOMPARE(combo->currentIndex(), 0);

        // ダイアログの選択タブと配色を、再作成したウィンドウでも復元する。
        tabs->setCurrentIndex(4);
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        QCOMPARE(AppSettings::boardColorDialogTab(), 4);
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(board()->boardColors() == expected);
        QCOMPARE(board()->whiteNameLabel()->palette().color(QPalette::WindowText), expected.nameText);
        openAppearanceDetails();
        dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        tabs = dialog->findChild<QTabWidget*>("boardColorTabs");
        QCOMPARE(tabs->currentIndex(), 4);
        QCOMPARE(dialog->findChild<QPushButton*>("clockTextColorButton")->text(), expected.clockText.name().toUpper());
        QTest::mouseClick(dialog->findChild<QPushButton*>("resetBoardColorsButton"), Qt::LeftButton);
        QVERIFY(board()->boardColors() == BoardColors{});
        QVERIFY(secondary.boardColors() == BoardColors{});
        QVERIFY(AppSettings::boardColors() == BoardColors{});
    }
    void boardColorPresets_data()
    {
        QTest::addColumn<QString>("style");
        QTest::newRow("standard") << QStringLiteral("standard");
    }
    void boardColorPresets()
    {
        BoardAppearance::instance().setVisuals({false, true, 108, false});
        AppSettings::setBoardColorDialogTab(0);
        QFETCH(QString, style);
        selectPieceStyle(style);
        openAppearanceDetails();
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        auto* combo = dialog->findChild<QComboBox*>("boardColorPresetCombo");
        QVERIFY(combo);
        auto* label = dialog->findChild<QLabel*>("boardColorPresetLabel");
        QVERIFY(label);
        QVERIFY(label->text().contains(BoardColorPresets::pieceStyleName(style)));
        const auto presets = BoardColorPresets::palettes();
        QCOMPARE(presets.size(), 5);
        QCOMPARE(combo->count(), 5);
        QCOMPARE(combo->currentIndex(), -1);
        ShogiView secondary;
        QSignalSpy changed(&BoardAppearance::instance(), &BoardAppearance::colorsChanged);
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(combo->itemText(i), presets.at(i).name);
            QVERIFY(!combo->itemIcon(i).pixmap(120, 72).isNull());
            combo->showPopup();
            QTest::qWait(20);
            auto* view = combo->view();
            if (i == 0) view->window()->grab().save(
                QStringLiteral(AUDIT_DIR "/screenshots/presets-options-%1.png").arg(style));
            const auto index = view->model()->index(i, 0);
            view->scrollTo(index);
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(index).center());
            QTRY_COMPARE(combo->currentIndex(), i);
            QVERIFY(board()->boardColors() == presets.at(i).colors);
            QVERIFY(secondary.boardColors() == presets.at(i).colors);
            QVERIFY(AppSettings::boardColors() == presets.at(i).colors);
            QCOMPARE(changed.count(), i + 1);
            QCOMPARE(boardSfen(), initial);
            QCOMPARE(board()->grab().toImage().pixelColor(squarePoint(5, 5)), presets.at(i).colors.board);
            const QString imagePath = QStringLiteral(AUDIT_DIR "/screenshots/preset-%1-%2.png").arg(style).arg(i);
            QVERIFY(board()->grab().save(imagePath));
        }
        dialog->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/presets-dialog-%1.png").arg(style));
        const auto selectedColors = presets.last().colors;
        QPointer<BoardColorDialog> guard(dialog);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        openAppearanceDetails();
        dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog);
        combo = dialog->findChild<QComboBox*>("boardColorPresetCombo");
        QVERIFY(combo);
        QCOMPARE(combo->currentIndex(), 4);
        QVERIFY(board()->boardColors() == selectedColors);

        // 個別調整でカスタム表示に変わり、候補の配色に戻すと選択も同期する。
        auto* colorButton = dialog->findChild<QPushButton*>("gridColorButton");
        QVERIFY(colorButton);
        QTest::mouseClick(colorButton, Qt::LeftButton);
        auto* picker = dialog->findChild<QColorDialog*>("boardColorPicker");
        QVERIFY(picker && picker->isVisible());
        picker->setCurrentColor(Qt::magenta);
        picker->accept();
        QCOMPARE(combo->currentIndex(), -1);
        QCOMPARE(AppSettings::boardColors().grid, QColor(Qt::magenta));
        BoardAppearance::instance().setColors(selectedColors);
        QCOMPARE(combo->currentIndex(), 4);
        QTest::mouseClick(dialog->findChild<QPushButton*>("resetBoardColorsButton"), Qt::LeftButton);
        QCOMPARE(combo->currentIndex(), -1);
        QVERIFY(AppSettings::boardColors() == BoardColors{});
    }
    void dialogs_data()
    {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("expected");
        for (const auto& pair : QList<QPair<QString,QString>>{
            {"actionOpenKifuFile", "QFileDialog"}, {"actionSaveAs", "QFileDialog"},
            {"actionStartGame", "StartGameDialog"}, {"actionCSA", "CsaGameDialog"},
            {"actionEngineSettings", "EngineRegistrationDialog"},
            {"actionAnalyzeKifu", "KifuAnalysisDialog"},
            {"actionTsumeShogiSearch", "TsumeShogiSearchDialog"},
            {"actionTsumeshogiGenerator", "TsumeshogiGeneratorDialog"},
            {"actionJishogiScore", "JishogiScoreDialog"},
            {"actionSfenCollectionViewer", "SfenCollectionDialog"},
            {"actionVersionInfo", "VersionDialog"}, {"actionAboutQt", "QMessageBox"},
            {"actionSaveDockLayout", "QInputDialog"}})
            QTest::newRow(qPrintable(pair.first)) << pair.first << pair.second;
    }
    void dialogs()
    {
        QFETCH(QString, name); QFETCH(QString, expected);
        if (name == "actionAnalyzeKifu") sampleGame();
        armDialog(); click(name);
        QTRY_VERIFY_WITH_TIMEOUT(dialogHandled, 1500);
        QCOMPARE(dialogClass, expected);
        QVERIFY(window->isVisible());
    }
    void boardAppearance()
    {
        const bool flipped = board()->flipMode();
        const int size = board()->squareSize();
        click("actionFlipBoard"); QCOMPARE(board()->flipMode(), !flipped);
        QCOMPARE(boardSfen(), initial);
        click("actionFlipBoard"); QCOMPARE(board()->flipMode(), flipped);
        click("actionEnlargeBoard"); QVERIFY(board()->squareSize() > size);
        click("actionShrinkBoard"); QCOMPARE(board()->squareSize(), size);
        auto* tb = window->findChild<QToolBar*>(); QVERIFY(tb);
        const bool shown = tb->isVisible();
        click("actionToolBar"); QCOMPARE(tb->isVisible(), !shown);
        click("actionToolBar"); QCOMPARE(tb->isVisible(), shown);
    }
    void boardEditing()
    {
        QVERIFY(!board()->positionEditMode());
        click("actionStartEditPosition"); QVERIFY(board()->positionEditMode());
        click("actionReturnAllPiecesToStand"); QCOMPARE(boardSfen(), QString("9/9/9/9/9/9/9/9/9"));
        QCOMPARE(board()->board()->convertStandToSfen(), QStringLiteral("-"));
        QCOMPARE(board()->board()->pieceBoxCount(Piece::BlackKing), 2);
        click("actionSetTsumePosition"); QVERIFY(boardSfen().contains('k'));
        click("actionSetHiratePosition"); QCOMPARE(boardSfen(), initial);
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        const auto before = gc->currentPlayer();
        click("actionChangeTurn");
        QVERIFY(gc->currentPlayer() != before);
        click("actionChangeTurn"); QCOMPARE(gc->currentPlayer(), before);
        click("actionEndEditPosition"); QVERIFY(!board()->positionEditMode());
    }
    void boardEditingClearsRecord_data()
    {
        QTest::addColumn<int>("selectedRow");
        QTest::addColumn<bool>("finishedGame");
        QTest::addColumn<bool>("useExitButton");
        QTest::newRow("loaded-start") << 0 << false << false;
        QTest::newRow("loaded-middle") << 2 << false << true;
        QTest::newRow("loaded-last") << 4 << false << false;
        QTest::newRow("finished-game") << 3 << true << true;
        QTest::newRow("finished-start") << 0 << true << false;
        QTest::newRow("finished-middle") << 1 << true << true;
    }
    void boardEditingClearsRecord()
    {
        QFETCH(int, selectedRow);
        QFETCH(bool, finishedGame);
        QFETCH(bool, useExitButton);
        auto* view = record()->kifuView();
        auto* model = qobject_cast<KifuRecordListModel*>(view->model());
        QVERIFY(model);
        QStringList initialCells;
        for (int column = 0; column < model->columnCount(); ++column) {
            initialCells.append(model->index(0, column).data().toString());
        }

        if (finishedGame) {
            armDialog("game"); click("actionStartGame"); QVERIFY(dialogHandled);
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 7));
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 6));
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 3));
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 4));
            QTRY_COMPARE(model->rowCount(), 3);
            armDialog("auto"); click("actionResign");
            QTRY_VERIFY(!record()->isNavigationDisabled());
            QTRY_COMPARE(model->rowCount(), 4);
        } else {
            sampleGame();
        }
        const QModelIndex selected = model->index(selectedRow, 0);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->visualRect(selected).center());
        QTRY_COMPARE(view->currentIndex().row(), selectedRow);

        auto* gameRecord = window->findChild<GameRecordModel*>(); QVERIFY(gameRecord);
        gameRecord->setComment(0, QStringLiteral("Previous opening comment"));
        gameRecord->setBookmark(0, QStringLiteral("Previous opening bookmark"));
        const QString beforeBoard = boardSfen();
        const QString beforeHand = board()->board()->convertStandToSfen();
        const Turn beforeTurn = board()->board()->currentPlayer();

        auto* evalChart = window->evalChart(); QVERIFY(evalChart);
        const int xAxisLimit = evalChart->xAxisLimit();
        const int yAxisLimit = evalChart->yAxisLimit();
        evalChart->appendScoreP1(1, 150);
        evalChart->appendScoreP2(2, -200);
        evalChart->appendScoreP1Buffered(3, 250);
        evalChart->appendScoreP2Buffered(4, -300);
        QVERIFY(evalChart->countP1() > 0);
        QVERIFY(evalChart->countP2() > 0);

        click("actionStartEditPosition");
        QVERIFY(board()->positionEditMode());
        // 描画待ちの評価値も破棄し、編集開始後にグラフが復活しないことを確認する。
        evalChart->flushPendingScores();
        QCOMPARE(evalChart->countP1(), 0);
        QCOMPARE(evalChart->countP2(), 0);
        QCOMPARE(evalChart->currentPly(), 0);
        QCOMPARE(evalChart->xAxisLimit(), xAxisLimit);
        QCOMPARE(evalChart->yAxisLimit(), yAxisLimit);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(model->currentHighlightRow(), 0);
        for (int column = 0; column < model->columnCount(); ++column) {
            QCOMPARE(model->index(0, column).data().toString(), initialCells.at(column));
        }
        QCOMPARE(boardSfen(), beforeBoard);
        QCOMPARE(board()->board()->convertStandToSfen(), beforeHand);
        QCOMPARE(board()->board()->currentPlayer(), beforeTurn);

        click("actionSetTsumePosition");
        click("actionChangeTurn");
        const QString editedBoard = boardSfen();
        const QString editedHand = board()->board()->convertStandToSfen();
        const Turn editedTurn = board()->board()->currentPlayer();
        const QString editedSfen = QStringLiteral("%1 %2 %3 1")
                                      .arg(editedBoard, turnToSfen(editedTurn), editedHand);
        if (useExitButton) {
            auto* exitButton = board()->findChild<QPushButton*>("editExitButton");
            QVERIFY(exitButton);
            QTest::mouseClick(exitButton, Qt::LeftButton);
        } else {
            click("actionEndEditPosition");
        }
        QVERIFY(!board()->positionEditMode());
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->currentHighlightRow(), 0);
        auto* tree = gameRecord->branchTree(); QVERIFY(tree); QVERIFY(tree->root());
        QCOMPARE(tree->root()->sfen(), editedSfen);
        QCOMPARE(tree->root()->childCount(), 0);
        const QString exported = copy("actionCopyUSIAll");
        QVERIFY(exported.contains(editedSfen));
        QVERIFY(!exported.contains(QStringLiteral("7g7f")));

        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QCOMPARE(boardSfen(), editedBoard);
        QCOMPARE(board()->board()->convertStandToSfen(), editedHand);
        QCOMPARE(board()->board()->currentPlayer(), editedTurn);
        QCOMPARE(view->currentIndex().row(), 0);
        snapshot(QStringLiteral("board-edit-reset-") + QString::fromLatin1(QTest::currentDataTag()));

        click("actionStartEditPosition");
        QCOMPARE(boardSfen(), editedBoard);
        QCOMPARE(model->rowCount(), 1);
        click("actionEndEditPosition");
    }
    void boardEditingTurn_data()
    {
        QTest::addColumn<QString>("operation");
        for (const char* op : {"move", "rejected-move", "hirate", "tsume", "stands"})
            QTest::newRow(op) << QString::fromLatin1(op);
    }
    void boardEditingTurn()
    {
        QFETCH(QString, operation);
        click("actionStartEditPosition");
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        if (operation == "move" || operation == "rejected-move") {
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 3));
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier,
                              squarePoint(operation == "move" ? 3 : 4, 4));
            QCOMPARE(board()->board()->pieceCharacter(3, operation == "move" ? 4 : 3), Piece::WhitePawn);
            QCOMPARE(gc->currentPlayer(), ShogiGameController::Player1);
        } else if (operation == "stands") {
            click("actionReturnAllPiecesToStand");
            const auto hand = board()->board()->pieceStand();
            const auto box = board()->board()->pieceBox();
            click("actionChangeTurn");
            QCOMPARE(board()->board()->pieceStand(), hand);
            QCOMPARE(board()->board()->pieceBox(), box);
            QCOMPARE(gc->currentPlayer(), ShogiGameController::Player2);
        } else {
            click("actionChangeTurn");
            click(operation == "hirate" ? "actionSetHiratePosition" : "actionSetTsumePosition");
            QCOMPARE(gc->currentPlayer(), ShogiGameController::Player1);
        }
        const Turn turn = gc->currentPlayer() == ShogiGameController::Player1 ? Turn::Black : Turn::White;
        QCOMPARE(board()->board()->currentPlayer(), turn);
        const QString expected = QStringLiteral("%1 %2 %3 1")
            .arg(boardSfen(), turnToSfen(turn), board()->board()->convertStandToSfen());
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
        click("actionEndEditPosition");
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
    }
    void boardEditingSelectionReset_data()
    {
        QTest::addColumn<QString>("operation");
        for (const char* op : {"actionSetHiratePosition", "actionSetTsumePosition",
                               "actionReturnAllPiecesToStand", "finish", "begin"})
            QTest::newRow(op) << QString::fromLatin1(op);
    }
    void boardEditingSelectionReset()
    {
        QFETCH(QString, operation);
        if (operation != "begin") click("actionStartEditPosition");
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(2, 8));
        if (operation == "begin") {
            click("actionStartEditPosition");
        } else if (operation == "finish") {
            click("actionEndEditPosition");
            click("actionStartEditPosition");
        } else {
            click(operation);
        }
        const QString before = boardSfen();
        const auto hand = board()->board()->pieceStand();
        QSignalSpy moves(window->findChild<BoardInteractionController*>(), &BoardInteractionController::moveRequested);
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(5, 5));
        QCOMPARE(moves.count(), 0);
        QCOMPARE(boardSfen(), before);
        QCOMPARE(board()->board()->pieceStand(), hand);
        // 次の選択・移動が通常どおり成立することも確認する。
        click("actionSetHiratePosition");
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 5));
        QCOMPARE(board()->board()->pieceCharacter(7, 5), Piece::BlackPawn);
        click("actionEndEditPosition");
    }
    void boardEditingPieceTransfers_data()
    {
        QTest::addColumn<bool>("flipped");
        QTest::newRow("normal") << false;
        QTest::newRow("flipped") << true;
    }
    void boardEditingPieceTransfers()
    {
        QFETCH(bool, flipped);
        click("actionStartEditPosition");
        if (flipped) click("actionFlipBoard");
        click("actionReturnAllPiecesToStand");
        auto* model = board()->board();
        const auto originalBox = model->pieceBox();
        const auto originalHand = model->pieceStand();
        // 両側7種類を駒箱→駒台→盤→相手駒台→元の駒台→駒箱へ移す。
        for (int side : {10, 11}) {
            for (int type = 1; type <= 7; ++type) {
                const int rank = side == 10 ? type : 10 - type;
                const int otherSide = side == 10 ? 11 : 10;
                const Piece piece = model->pieceCharacter(side, rank);
                const Piece otherPiece = model->pieceCharacter(otherSide, 10 - rank);
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(12, type));
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(side, rank));
                QCOMPARE(model->pieceStandCount(piece), 1);
                QCOMPARE(model->pieceBoxCount(piece), originalBox.value(toBlack(piece)) - 1);
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(side, rank));
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(5, 5));
                QCOMPARE(model->pieceCharacter(5, 5), piece);
                QCOMPARE(model->pieceStandCount(piece), 0);
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(5, 5));
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(otherSide, 10 - rank));
                QCOMPARE(model->pieceCharacter(5, 5), Piece::None);
                QCOMPARE(model->pieceStandCount(otherPiece), 1);
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(otherSide, 10 - rank));
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(side, rank));
                QCOMPARE(model->pieceStandCount(piece), 1);
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(side, rank));
                QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(12, type));
                QCOMPARE(model->pieceStand(), originalHand);
                QCOMPARE(model->pieceBox(), originalBox);
            }
        }
        snapshot(flipped ? "board-edit-box-full-flipped" : "board-edit-box-full");
        click("actionEndEditPosition");
        QVERIFY(board()->pieceBoxRect().isEmpty());
        QCOMPARE(model->pieceBox(), originalBox);
        click("actionStartEditPosition");
        QVERIFY(!board()->pieceBoxRect().isEmpty());
        QCOMPARE(model->pieceBox(), originalBox);
        QCOMPARE(model->pieceStand(), originalHand);
        click("actionEndEditPosition");
    }
    void boardEditingPieceBox_data() { boardEditingPieceTransfers_data(); }
    void boardEditingPieceBox()
    {
        QFETCH(bool, flipped);
        QVERIFY(board()->pieceBoxRect().isEmpty());
        click("actionStartEditPosition");
        if (flipped) click("actionFlipBoard");
        auto* model = board()->board();
        QVERIFY(!board()->pieceBoxRect().isEmpty());
        for (int count : model->pieceBox()) QCOMPARE(count, 0);
        // 空の駒箱からは取り出せない。
        editMove({12, 8}, {5, 5});
        QCOMPARE(boardSfen(), initial);
        click("actionReturnAllPiecesToStand");
        QCOMPARE(model->pieceBoxCount(Piece::BlackKing), 2);
        QCOMPARE(model->convertStandToSfen(), QStringLiteral("-"));
        // 王を2枚とも先手向きで取り出し、右クリックで片方を後手の玉にする。
        editMove({12, 8}, {5, 1});
        QCOMPARE(model->pieceCharacter(5, 1), Piece::BlackKing);
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(5, 1));
        QCOMPARE(model->pieceCharacter(5, 1), Piece::WhiteKing);
        editMove({12, 8}, {5, 9});
        QCOMPARE(model->pieceCharacter(5, 9), Piece::BlackKing);
        editMove({12, 8}, {5, 5});
        QCOMPARE(model->pieceCharacter(5, 5), Piece::None);
        editMove({5, 9}, {12, 8});
        QCOMPARE(model->pieceBoxCount(Piece::BlackKing), 1);
        // 詰将棋用の片玉局面。成りと金の先後変更、通常の持ち駒も保持する。
        editMove({12, 7}, {5, 5});
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(5, 5));
        QCOMPARE(model->pieceCharacter(5, 5), Piece::BlackDragon);
        editMove({12, 5}, {4, 5});
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(4, 5));
        QCOMPARE(model->pieceCharacter(4, 5), Piece::WhiteGold);
        editMove({12, 6}, {10, 6});
        QCOMPARE(model->pieceStandCount(Piece::BlackBishop), 1);
        const auto box = model->pieceBox();
        const auto hand = model->pieceStand();
        const QString expected = copy("actionCopySFEN").trimmed();
        snapshot(flipped ? "board-edit-box-tsume-flipped" : "board-edit-box-tsume");
        // 編集終了では駒箱だけを隠し、再開時は未使用駒をそのまま表示する。
        const QPoint oldBoxCenter = board()->pieceBoxCellRect(8).center();
        auto* finish = board()->findChild<QPushButton*>("editExitButton"); QVERIFY(finish);
        QTest::mouseClick(finish, Qt::LeftButton);
        QVERIFY(board()->pieceBoxRect().isEmpty());
        QCOMPARE(board()->clickedSquare(oldBoxCenter), QPoint());
        QCOMPARE(model->pieceBox(), box);
        click("actionStartEditPosition");
        QVERIFY(!board()->pieceBoxRect().isEmpty());
        QCOMPARE(model->pieceBox(), box);
        QCOMPARE(model->pieceStand(), hand);
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
        click("actionEndEditPosition");
        const QString savedKif = copy("actionCopyKIF");
        armDialog("discard"); click("actionNewGame");
        dialogMessages.clear(); armDialog("messages"); paste(savedKif);
        QVERIFY2(dialogMessages.isEmpty(), qPrintable(dialogMessages.join('\n')));
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
        click("actionStartEditPosition");
        QCOMPARE(board()->board()->pieceBox(), box);
        QCOMPARE(board()->board()->pieceStand(), hand);
        // 保存・再読込後も、駒箱に残していた玉を取り出せる。
        editMove({12, 8}, {5, 9});
        QCOMPARE(board()->board()->pieceCharacter(5, 9), Piece::BlackKing);
        QCOMPARE(board()->board()->pieceBoxCount(Piece::BlackKing), 0);
        click("actionEndEditPosition");
    }
    void boardEditingPieceBoxLayout_data()
    {
        QTest::addColumn<bool>("flipped");
        QTest::addColumn<int>("size");
        for (bool flipped : {false, true}) {
            for (int size : {30, 50, 100}) {
                const QString name = QStringLiteral("%1-%2").arg(flipped ? "flipped" : "normal").arg(size);
                QTest::newRow(qPrintable(name)) << flipped << size;
            }
        }
    }
    void boardEditingPieceBoxLayout()
    {
        QFETCH(bool, flipped); QFETCH(int, size);
        click("actionStartEditPosition");
        if (flipped) click("actionFlipBoard");
        board()->setSquareSize(size - 1);
        click("actionEnlargeBoard");
        QCOMPARE(board()->squareSize(), size);
        click("actionReturnAllPiecesToStand");
        const QRect box = board()->pieceBoxRect();
        QVERIFY(!box.isEmpty());
        QVERIFY(board()->rect().contains(box));
        auto* card = board()->findChild<QWidget*>(flipped ? "blackPlayerCard" : "whitePlayerCard");
        QVERIFY(card);
        QVERIFY(!box.intersects(card->geometry()));
        QVERIFY(!box.intersects(editLayout().blackStandBoundingRect(9, 9)));
        QVERIFY(!box.intersects(editLayout().whiteStandBoundingRect(9, 9)));
        for (int rank = 1; rank <= 8; ++rank) {
            const QRect cell = board()->pieceBoxCellRect(rank);
            QVERIFY(box.contains(cell));
            QCOMPARE(board()->clickedSquare(cell.center()), QPoint(12, rank));
        }
        editMove({12, 8}, {5, 5});
        QCOMPARE(board()->board()->pieceCharacter(5, 5), Piece::BlackKing);
        editMove({5, 5}, {12, 8});
        QCOMPARE(board()->board()->pieceBoxCount(Piece::BlackKing), 2);
        click("actionEndEditPosition");
    }
    void boardEditingKingsStayOnBoard_data()
    {
        QTest::addColumn<bool>("flipped");
        QTest::addColumn<bool>("blackKing");
        QTest::addColumn<int>("standFile");
        for (bool flipped : {false, true}) {
            for (bool blackKing : {false, true}) {
                for (int standFile : {10, 11}) {
                    const QString name = QStringLiteral("%1-%2-to-%3")
                        .arg(flipped ? "flipped" : "normal", blackKing ? "black" : "white")
                        .arg(standFile);
                    QTest::newRow(qPrintable(name)) << flipped << blackKing << standFile;
                }
            }
        }
    }
    void boardEditingKingsStayOnBoard()
    {
        QFETCH(bool, flipped); QFETCH(bool, blackKing); QFETCH(int, standFile);
        click("actionStartEditPosition");
        if (flipped) click("actionFlipBoard");
        auto* model = board()->board();
        // 持ち駒がある状態でも王・玉の駒台への移動だけを拒否する。
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(6, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(10, 1));
        QCOMPARE(model->pieceStandCount(Piece::BlackPawn), 1);
        const auto before = model->boardData();
        const auto hand = model->pieceStand();
        const int rank = blackKing ? 9 : 1;
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(5, rank));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier,
                          editPoint(standFile, standFile == 10 ? 8 : 2));
        QCOMPARE(model->boardData(), before);
        QCOMPARE(model->pieceStand(), hand);
        QCOMPARE(model->pieceStandCount(Piece::BlackKing), 0);
        QCOMPARE(model->pieceStandCount(Piece::WhiteKing), 0);
        // 拒否後も盤上の王・玉は通常どおり移せる。
        const int destinationRank = blackKing ? 8 : 2;
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(5, rank));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(5, destinationRank));
        QCOMPARE(model->pieceCharacter(5, rank), Piece::None);
        QCOMPARE(model->pieceCharacter(5, destinationRank), blackKing ? Piece::BlackKing : Piece::WhiteKing);
        const auto expected = model->boardData();
        auto* finish = board()->findChild<QPushButton*>("editExitButton"); QVERIFY(finish);
        QTest::mouseClick(finish, Qt::LeftButton);
        QCOMPARE(model->boardData(), expected);
        QCOMPARE(model->pieceStand(), hand);
        click("actionStartEditPosition");
        QCOMPARE(model->boardData(), expected);
        QCOMPARE(model->pieceStand(), hand);
        QCOMPARE(model->boardData().count(Piece::BlackKing), 1);
        QCOMPARE(model->boardData().count(Piece::WhiteKing), 1);
        click("actionEndEditPosition");
    }
    void boardEditingPromotionAndCapture()
    {
        click("actionStartEditPosition");
        click("actionReturnAllPiecesToStand");
        auto* model = board()->board();
        for (int type : {1, 2, 3, 4, 6, 7}) {
            const Piece piece = model->pieceCharacter(12, type);
            const int count = model->pieceBoxCount(piece);
            const QPoint stand = editPoint(12, type);
            const QPoint center = squarePoint(5, 5);
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, stand);
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, center);
            for (Piece expected : {promote(piece), toWhite(piece), promote(toWhite(piece)), piece}) {
                QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, center);
                QCOMPARE(model->pieceCharacter(5, 5), expected);
            }
            QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, center);
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, center);
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, stand);
            QCOMPARE(model->pieceCharacter(5, 5), Piece::None);
            QCOMPARE(model->pieceBoxCount(piece), count);
        }
        click("actionSetHiratePosition");
        // 相手の成駒を取ると、生駒として持ち駒に加わる。
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, squarePoint(3, 3));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(2, 8));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 3));
        QCOMPARE(model->pieceCharacter(3, 3), Piece::BlackRook);
        QCOMPARE(model->pieceStandCount(Piece::BlackPawn), 1);
        // 同じマス、右クリック、空きマス・空の駒台で状態を壊さない。
        const QString before = boardSfen();
        for (Qt::MouseButton cancel : {Qt::LeftButton, Qt::RightButton}) {
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 3));
            QTest::mouseClick(board(), cancel, Qt::NoModifier, squarePoint(3, 3));
            QCOMPARE(boardSfen(), before);
        }
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(5, 5));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(10, 7));
        QCOMPARE(boardSfen(), before);
        click("actionEndEditPosition");
    }
    void boardEditingStandMargins_data() { boardEditingPieceTransfers_data(); }
    void boardEditingStandMargins()
    {
        QFETCH(bool, flipped);
        click("actionStartEditPosition");
        if (flipped) click("actionFlipBoard");
        click("actionReturnAllPiecesToStand");
        for (int side : {10, 11}) {
            const auto layout = editLayout();
            const QRect stand = side == 10 ? layout.blackStandBoundingRect(9, 9)
                                           : layout.whiteStandBoundingRect(9, 9);
            const QList<QPoint> outside = {
                QPoint(stand.left() - 1, stand.center().y()),
                QPoint(stand.right() + 1, stand.center().y()),
                QPoint(stand.center().x(), stand.top() - 1),
                QPoint(stand.center().x(), stand.bottom() + 1)};
            for (const QPoint& point : outside) QCOMPARE(board()->clickedSquare(point), QPoint());
        }
        click("actionEndEditPosition");
    }
    void boardEditingForcedPromotion_data()
    {
        QTest::addColumn<int>("side");
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("destinationRank");
        for (int side : {10, 11}) {
            for (int type : {1, 2, 3}) {
                for (int rank = 1; rank <= (type == 3 ? 2 : 1); ++rank) {
                    const QString name = QStringLiteral("%1-%2-%3").arg(side).arg(type).arg(rank);
                    QTest::newRow(qPrintable(name)) << side << type << (side == 10 ? rank : 10 - rank);
                }
            }
        }
    }
    void boardEditingForcedPromotion()
    {
        QFETCH(int, side); QFETCH(int, type); QFETCH(int, destinationRank);
        click("actionStartEditPosition");
        click("actionReturnAllPiecesToStand");
        auto* model = board()->board();
        const int standRank = side == 10 ? type : 10 - type;
        const Piece piece = model->pieceCharacter(side, standRank);
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        QVERIFY(gc->editPosition({12, type}, {side, standRank}));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(side, standRank));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(4, destinationRank));
        QCOMPARE(model->pieceCharacter(4, destinationRank), promote(piece));
        // 右クリックの巡回でも、行き所のない不成駒には戻らない。
        for (int i = 0; i < 4; ++i) {
            QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(4, destinationRank));
            QVERIFY(model->pieceCharacter(4, destinationRank) != piece);
        }
        click("actionEndEditPosition");
    }
    void boardEditingRejectedMoves()
    {
        click("actionStartEditPosition");
        auto* model = board()->board();
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        const QString before = boardSfen();
        const auto hand = model->pieceStand();
        // 二歩、味方駒、玉取り、空の駒台をGUIから操作する。
        const QList<QPair<QPoint, QPoint>> moves = {
            {{7, 7}, {6, 5}}, {{2, 8}, {5, 9}}, {{2, 8}, {5, 1}}, {{10, 7}, {5, 5}}};
        for (const auto& move : moves) {
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(move.first.x(), move.first.y()));
            QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(move.second.x(), move.second.y()));
            QCOMPARE(boardSfen(), before);
            QCOMPARE(model->pieceStand(), hand);
            QCOMPARE(gc->currentPlayer(), ShogiGameController::Player1);
        }
        // 不正な座標や駒台セルを渡されても、駒が消えたり別の種類に変わったりしない。
        const QList<QPair<QPoint, QPoint>> invalid = {
            {{0, 0}, {5, 1}}, {{5, 5}, {3, 3}}, {{7, 7}, {5, 0}},
            {{7, 7}, {13, 1}}, {{7, 7}, {10, 9}}, {{7, 7}, {11, 1}},
            {{7, 7}, {10, 7}}, {{7, 7}, {7, 7}}};
        for (const auto& move : invalid) {
            QVERIFY(!gc->editPosition(move.first, move.second));
            QCOMPARE(boardSfen(), before);
            QCOMPARE(model->pieceStand(), hand);
        }
        // 二歩になる先後変更は右クリックでもスキップする。
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(7, 7));
        QCOMPARE(model->pieceCharacter(7, 7), Piece::BlackPromotedPawn);
        QTest::mouseClick(board(), Qt::RightButton, Qt::NoModifier, editPoint(7, 7));
        QCOMPARE(model->pieceCharacter(7, 7), Piece::WhitePromotedPawn);
        click("actionEndEditPosition");
    }
    void boardEditingFromBranch()
    {
        QFile fixture(QStringLiteral(REPO "/tests/fixtures/test_branch.kif"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        paste(QString::fromUtf8(fixture.readAll()));
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        for (int i = 0; i < 3; ++i) QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        auto* branches = record()->branchView(); QVERIFY(branches->model());
        QTRY_VERIFY(branches->model()->rowCount() >= 2);
        const auto index = branches->model()->index(1, 0);
        QTest::mouseClick(branches->viewport(), Qt::LeftButton, Qt::NoModifier, branches->visualRect(index).center());
        const QString expected = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2PP5/PP2PPPPP/1B5R1/LNSGKGSNL w - 1");
        QTRY_COMPARE(boardSfen(), expected.section(' ', 0, 0));
        click("actionStartEditPosition");
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
        click("actionEndEditPosition");
        QCOMPARE(copy("actionCopySFEN").trimmed(), expected);
        QCOMPARE(record()->kifuView()->model()->rowCount(), 1);
        QVERIFY(copy("actionCopyUSIAll").contains(expected));
    }
    void boardEditingGameAndExport()
    {
        click("actionStartEditPosition");
        QVERIFY(!action("actionStartGame")->isEnabled());
        QVERIFY(!action("actionPasteKifu")->isEnabled());
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(7, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(10, 1));
        click("actionChangeTurn");
        const QString edited = copy("actionCopySFEN").trimmed();
        snapshot("board-edit-custom");
        auto* exitButton = board()->findChild<QPushButton*>("editExitButton"); QVERIFY(exitButton);
        QTest::mouseClick(exitButton, Qt::LeftButton);
        QVERIFY(!board()->positionEditMode());
        QVERIFY(action("actionStartGame")->isEnabled());
        const QString savedKif = copy("actionCopyKIF");
        QVERIFY(copy("actionCopyUSIAll").contains(edited));
        armDialog("game"); click("actionStartGame"); QVERIFY(dialogHandled);
        QCOMPARE(copy("actionCopySFEN").trimmed(), edited);
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(3, 3));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(3, 4));
        QTRY_COMPARE(record()->kifuView()->model()->rowCount(), 2);
        QCOMPARE(board()->board()->pieceCharacter(3, 4), Piece::WhitePawn);
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(10, 1));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, editPoint(7, 6));
        QTRY_COMPARE(record()->kifuView()->model()->rowCount(), 3);
        QCOMPARE(board()->board()->pieceCharacter(7, 6), Piece::BlackPawn);
        armDialog("auto"); click("actionResign");
        QTRY_VERIFY(!record()->isNavigationDisabled());
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QCOMPARE(copy("actionCopySFEN").trimmed(), edited);
        armDialog("discard"); click("actionNewGame");
        dialogMessages.clear();
        armDialog("messages");
        paste(savedKif);
        QVERIFY2(dialogMessages.isEmpty(), qPrintable(dialogMessages.join('\n')));
        auto* loadedRecord = window->findChild<GameRecordModel*>(); QVERIFY(loadedRecord);
        QCOMPARE(loadedRecord->branchTree()->root()->sfen(), edited);
        QVERIFY(copy("actionCopyUSIAll").contains(edited));
        QCOMPARE(copy("actionCopySFEN").trimmed(), edited);
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        QCOMPARE(gc->currentPlayer(), ShogiGameController::Player2);
        click("actionStartEditPosition");
        QCOMPARE(copy("actionCopySFEN").trimmed(), edited);
        click("actionEndEditPosition");
    }
    void closeWhileLoading()
    {
        paste("position startpos moves 7g7f 3c3d 2g2f 8c8d", false);
        window.reset();
        QTest::qWait(100);
    }
    void pasteNavigation()
    {
        sampleGame();
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        const QString last = boardSfen(); QVERIFY(last != initial);
        QCOMPARE(record()->kifuView()->currentIndex().row(), 4);
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QCOMPARE(boardSfen(), initial);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QCOMPARE(boardSfen(), QString("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL"));
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton); QCOMPARE(boardSfen(), last);
        snapshot("kifu-navigation");
        armDialog("discard");
        click("actionNewGame"); QCOMPARE(boardSfen(), initial);
    }
    void formatRoundTrip_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* n : {"actionCopyKIF", "actionCopyKI2", "actionCopyCSA", "actionCopyUSIAll", "actionCopyJKF", "actionCopyUSEN"})
            QTest::newRow(n) << QString(n);
    }
    void formatRoundTrip()
    {
        QFETCH(QString, name); sampleGame();
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        const auto last = boardSfen(); const auto text = copy(name);
        QVERIFY2(!text.isEmpty(), qPrintable(name));
        armDialog("discard");
        click("actionNewGame"); paste(text);
        const auto roundTripUsi = copy("actionCopyUSIAll");
        qInfo() << "round trip" << name << text << roundTripUsi;
        QVERIFY(roundTripUsi.contains("7g7f 3c3d 2g2f 8c8d"));
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        QCOMPARE(boardSfen(), last);
    }
    void currentPositionCopy_data()
    {
        QTest::addColumn<int>("ply");
        QTest::addColumn<QString>("expected");
        QTest::newRow("initial") << 0 << initial + " b - 1";
        QTest::newRow("one-ply") << 1 << QString("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        QTest::newRow("four-plies") << 4 << QString("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
    }
    void currentPositionCopy()
    {
        QFETCH(int, ply); QFETCH(QString, expected);
        sampleGame(); QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        for (int i = 0; i < ply; ++i) QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        const auto current = copy("actionCopyUSICurrent");
        QCOMPARE(current.contains("7g7f"), ply > 0);
        QCOMPARE(current.contains("8c8d"), ply == 4);
        QVERIFY(copy("actionCopyUSIAll").contains("8c8d"));
        const auto sfen = copy("actionCopySFEN");
        qInfo() << "copied SFEN" << sfen << "board" << boardSfen() << window->statusBar()->currentMessage();
        QCOMPARE(sfen, expected);
        const auto bod = copy("actionCopyBOD"); QVERIFY(bod.contains(QStringLiteral("歩")));
    }
    void fileOpenSave()
    {
        armDialog("file", QStringLiteral(REPO "/tests/fixtures/test_basic.kif"));
        click("actionOpenKifuFile"); QVERIFY(dialogHandled);
        QTRY_VERIFY(record()->kifuView()->model()->rowCount() > 2);
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        const auto last = boardSfen();
        const QString path = QStringLiteral(AUDIT_DIR "/saved.kif");
        QFile::remove(path);
        armDialog("file", path); click("actionSaveAs"); QVERIFY(dialogHandled);
        QVERIFY(QFileInfo(path).size() > 50);
        click("actionSave");
        click("actionNewGame");
        armDialog("file", path); click("actionOpenKifuFile");
        QTRY_VERIFY(record()->kifuView()->model()->rowCount() > 2);
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        QCOMPARE(boardSfen(), last);
    }
    void imageExport_data()
    {
        QTest::addColumn<QString>("name"); QTest::addColumn<bool>("file");
        QTest::newRow("boardClipboard") << QString("actionCopyBoardToClipboard") << false;
        QTest::newRow("graphClipboard") << QString("actionCopyEvalGraphToClipboard") << false;
        QTest::newRow("boardFile") << QString("actionSaveBoardImage") << true;
        QTest::newRow("graphFile") << QString("actionSaveEvaluationGraph") << true;
    }
    void imageExport()
    {
        QFETCH(QString, name); QFETCH(bool, file);
        QImage img;
        if (file) {
            const QString path = QStringLiteral(AUDIT_DIR "/") + name + ".png";
            QFile::remove(path); armDialog("file", path); click(name);
            QVERIFY(dialogHandled); QVERIFY(img.load(path));
        } else {
            QApplication::clipboard()->clear(); click(name);
            img = QApplication::clipboard()->image();
        }
        QVERIFY(!img.isNull()); QVERIFY(img.width() > 100); QVERIFY(img.height() > 100);
    }
    void humanGame()
    {
        armDialog("game"); click("actionStartGame"); QVERIFY(dialogHandled);
        QVERIFY(board()->blackNameLabel()->fullText().contains("Audit Black"));
        QVERIFY(board()->whiteNameLabel()->fullText().contains("Audit White"));
        QVERIFY(record()->isNavigationDisabled());
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 6));
        QTRY_COMPARE(record()->kifuView()->model()->rowCount(), 2);
        QVERIFY(boardSfen() != initial);
        QCOMPARE(copy("actionCopySFEN"), boardSfen() + " w - 2");
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 3));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(3, 4));
        QTRY_COMPARE(record()->kifuView()->model()->rowCount(), 3);
        QCOMPARE(copy("actionCopySFEN"), boardSfen() + " b - 3");
        snapshot("human-game");
        click("actionUndoMove"); QCOMPARE(boardSfen(), initial);
        armDialog("yes"); click("actionBreakOffGame");
        QTRY_VERIFY(!record()->isNavigationDisabled());
    }
    void websiteLink()
    {
        click("actionOpenWebsite"); QTRY_COMPARE_WITH_TIMEOUT(urls.size(), 1, 500);
        QCOMPARE(urls.first(), QUrl("https://hnakada123.github.io/ShogiBoardQ/"));
    }
    void usageLink()
    {
        click("actionUsage"); QTRY_COMPARE_WITH_TIMEOUT(urls.size(), 1, 500);
        QVERIFY(urls.first().path().contains("guide"));
    }
    void bodCurrentPosition_data()
    {
        QTest::addColumn<int>("ply");
        QTest::newRow("one-ply") << 1;
        QTest::newRow("four-plies") << 4;
    }
    void bodCurrentPosition()
    {
        QFETCH(int, ply);
        sampleGame(); QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        for (int i = 0; i < ply; ++i) QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        const QString last = boardSfen();
        const auto turn = board()->board()->currentPlayer();
        const auto hands = board()->board()->pieceStand();
        const auto bod = copy("actionCopyBOD");
        QVERIFY(bod.contains(QStringLiteral("手数＝%1").arg(ply)));
        qInfo() << "BOD" << bod;
        armDialog("discard");
        click("actionNewGame");
        dialogMessages.clear();
        paste(bod);
        QVERIFY2(dialogMessages.isEmpty(), qPrintable(dialogMessages.join("\n")));
        QCOMPARE(boardSfen(), last);
        QCOMPARE(board()->board()->currentPlayer(), turn);
        QCOMPARE(board()->board()->pieceStand(), hands);
    }
    void editedPositionCopy()
    {
        click("actionStartEditPosition"); click("actionSetTsumePosition");
        const auto sfen = copy("actionCopySFEN");
        qInfo() << "edited board" << boardSfen() << "copied" << sfen;
        QVERIFY(sfen.contains(boardSfen()));
        click("actionSetHiratePosition");
        QCOMPARE(copy("actionCopySFEN"), initial + " b - 1");
        click("actionChangeTurn");
        QCOMPARE(copy("actionCopySFEN"), initial + " w - 1");
    }
    void dockVisibilityAndLock()
    {
        const auto docks = window->findChildren<QDockWidget*>();
        QVERIFY(docks.size() >= 10);
        for (auto* dock : docks) {
            auto* a = dock->toggleViewAction();
            const bool checked = a->isChecked();
            clickAction(a); QCOMPARE(a->isChecked(), !checked);
            clickAction(a); QCOMPARE(a->isChecked(), checked);
        }
        if (!action("actionLockDocks")->isChecked()) click("actionLockDocks");
        for (auto* dock : docks) qInfo() << "locked dock" << dock->windowTitle() << dock->features();
        for (auto* dock : docks) QVERIFY2(!dock->features().testFlag(QDockWidget::DockWidgetMovable), qPrintable(dock->windowTitle()));
        click("actionLockDocks");
        for (auto* dock : docks) QVERIFY(dock->features().testFlag(QDockWidget::DockWidgetMovable));
        armDialog("yes"); click("actionResetDockLayout");
        snapshot("docks");
    }
    void languageSettings()
    {
        armDialog(); click("actionLanguageEnglish"); QCOMPARE(AppSettings::language(), QString("en"));
        QVERIFY(action("actionLanguageEnglish")->isChecked());
        armDialog(); click("actionLanguageJapanese"); QCOMPARE(AppSettings::language(), QString("ja_JP"));
        armDialog(); click("actionLanguageSystem"); QCOMPARE(AppSettings::language(), QString("system"));
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(APP_BUILD "/ShogiBoardQ_en.qm")));
        window.reset(); qApp->installTranslator(&translator);
        window = std::make_unique<MainWindow>(); window->show();
        QCOMPARE(action("actionOpenKifuFile")->text(), QStringLiteral("Open…"));
        snapshot("english");
        window.reset(); qApp->removeTranslator(&translator);
    }
    void sfenCollection()
    {
        selectedCollection.clear();
        armDialog("collection"); click("actionSfenCollectionViewer");
        QVERIFY(!selectedCollection.isEmpty()); QCOMPARE(boardSfen(), selectedCollection);
    }
    void josekiVisibleDuringDestruction()
    {
        auto* jw = window->findChild<JosekiWindow*>();
        QVERIFY(jw);
        auto* dock = qobject_cast<QDockWidget*>(jw->parentWidget());
        QVERIFY(dock);
        dock->show();
        dock->raise();
        QTRY_VERIFY(jw->isVisible());
        QPointer<JosekiWindow> guard(jw);
        window.reset();
        QVERIFY(guard.isNull());
    }
    void josekiLoadAndPlay()
    {
        armDialog("game"); click("actionStartGame");
        auto* jw = window->findChild<JosekiWindow*>(); QVERIFY(jw);
        auto* dock = qobject_cast<QDockWidget*>(jw->parentWidget()); QVERIFY(dock);
        if (!dock->toggleViewAction()->isChecked()) clickAction(dock->toggleViewAction());
        dock->raise();
        QFile f(QStringLiteral(AUDIT_DIR "/audit.db")); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(("#YANEURAOU-DB2016 1.00\nsfen " + initial + " b - 1\n7g7f 3c3d 30 10 1\n").toUtf8()); f.close();
        QPushButton* open = nullptr;
        for (auto* b : jw->findChildren<QPushButton*>()) if (b->text() == QStringLiteral("開く")) open = b;
        QVERIFY(open); armDialog("file", f.fileName()); QTest::mouseClick(open, Qt::LeftButton);
        auto* table = jw->findChild<QTableWidget*>(); QVERIFY(table);
        QTRY_COMPARE_WITH_TIMEOUT(table->rowCount(), 1, 2000);
        QPushButton* add = nullptr;
        QPushButton* save = nullptr;
        for (auto* button : jw->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("＋追加")) add = button;
            if (button->text() == QStringLiteral("保存")) save = button;
        }
        QVERIFY(add && save);
        armDialog("joseki-add");
        QTest::mouseClick(add, Qt::LeftButton);
        QCOMPARE(table->rowCount(), 2);
        QTRY_VERIFY(table->cellWidget(0, 4)->isVisible());
        QCoreApplication::processEvents();
        armDialog("joseki-edit");
        QTest::mouseClick(table->cellWidget(0, 4), Qt::LeftButton);
        QVERIFY(dialogHandled);
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("△８四歩(83)"));
        QVERIFY(save->isEnabled());
        QTest::mouseClick(save, Qt::LeftButton);
        QTRY_VERIFY(table->isEnabled());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray contents = f.readAll();
        QVERIFY(contents.contains("7g7f 8c8d"));
        QVERIFY(contents.contains("2g2f 8c8d"));
        f.close();
        dock->setFloating(true);
        dock->resize(1120, 620);
        QTest::qWait(30);
        QVERIFY(jw->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/joseki-window.png")));
        auto index = table->model()->index(0, 1);
        armDialog("auto");
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, table->visualRect(index).center());
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, table->visualRect(index).center());
        QTRY_VERIFY_WITH_TIMEOUT(boardSfen() != initial, 1500);
        QCOMPARE(record()->kifuView()->model()->rowCount(), 2);
    }
    void engineHumanGame()
    {
        armDialog("gameEngineWhite"); click("actionStartGame"); QVERIFY(record()->isNavigationDisabled());
        QVERIFY(board()->whiteNameLabel()->fullText().contains("Audit USI"));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 6));
        QTRY_COMPARE_WITH_TIMEOUT(record()->kifuView()->model()->rowCount(), 3, 5000);
        armDialog("auto"); click("actionBreakOffGame");
        QTRY_VERIFY(!record()->isNavigationDisabled());
        QVERIFY(copy("actionCopyUSIAll").contains("7g7f 3c3d"));
    }
    void engineVersusEngine()
    {
        armDialog("gameEngines"); click("actionStartGame");
        armDialog("auto");
        QTRY_VERIFY_WITH_TIMEOUT(record()->kifuView()->model()->rowCount() >= 6, 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!record()->isNavigationDisabled(), 3000);
        QVERIFY(copy("actionCopyKIF").contains(QStringLiteral("投了")));
        snapshot("engine-game");
    }
    void engineHumanResignNavigation_data()
    {
        QTest::addColumn<bool>("humanIsBlack");
        QTest::addColumn<bool>("realAnalysis");
        QTest::newRow("human-black") << true << false;
        QTest::newRow("human-white") << false << false;
        QTest::newRow("human-black-real-analysis") << true << true;
    }
    void engineHumanResignNavigation()
    {
        QFETCH(bool, humanIsBlack);
        QFETCH(bool, realAnalysis);
        const QString realEngine = QStringLiteral(APP_BUILD "/Hayanagi/hayanagi");
        if (realAnalysis && !QFileInfo::exists(realEngine)) QSKIP("Hayanagi is not built");
        armDialog(humanIsBlack ? "gameEngineWhite" : "gameEngineBlack");
        click("actionStartGame");
        QVERIFY(dialogHandled);
        QVERIFY(record()->isNavigationDisabled());

        auto* view = record()->kifuView();
        auto* model = qobject_cast<KifuRecordListModel*>(view->model());
        QVERIFY(model);
        if (!humanIsBlack) {
            QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), 2, 5000);
        }
        const int file = humanIsBlack ? 7 : 3;
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier,
                          squarePoint(file, humanIsBlack ? 7 : 3));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier,
                          squarePoint(file, humanIsBlack ? 6 : 4));
        const int lastMoveRow = humanIsBlack ? 2 : 3;
        QTRY_COMPARE_WITH_TIMEOUT(model->rowCount(), lastMoveRow + 1, 5000);

        armDialog("auto");
        click("actionResign");
        QTRY_VERIFY_WITH_TIMEOUT(!record()->isNavigationDisabled(), 1500);
        const int terminalRow = lastMoveRow + 1;
        QCOMPARE(model->rowCount(), terminalRow + 1);
        QVERIFY(model->index(terminalRow, 0).data().toString().contains(QStringLiteral("投了")));

        // 投了直後も、カードと手番バッジの強調を保つ。
        auto* activeCard = board()->findChild<QFrame*>(
            humanIsBlack ? QStringLiteral("blackPlayerCard") : QStringLiteral("whitePlayerCard"));
        auto* inactiveCard = board()->findChild<QFrame*>(
            humanIsBlack ? QStringLiteral("whitePlayerCard") : QStringLiteral("blackPlayerCard"));
        const auto* turnLabel = board()->findChild<QLabel*>(
            humanIsBlack ? QStringLiteral("turnLabelBlack") : QStringLiteral("turnLabelWhite"));
        QVERIFY(turnLabel && turnLabel->isVisible());
        QVERIFY(activeCard && inactiveCard);
        QCOMPARE(activeCard->palette().color(QPalette::Window), BoardColors{}.cardBackground);
        QCOMPARE(inactiveCard->palette().color(QPalette::Window), BoardColors{}.cardBackground);
        for (auto* card : {activeCard, inactiveCard}) {
            const QImage image = card->grab().toImage();
            const QColor borderColor = card == activeCard ? BoardColors{}.activeCardBorder : BoardColors{}.cardBorder;
            for (const QPoint& edge : {QPoint(0, image.height() / 2),
                                       QPoint(image.width() - 1, image.height() / 2),
                                       QPoint(image.width() / 2, 0),
                                       QPoint(image.width() / 2, image.height() - 1)}) {
                QCOMPARE(image.pixelColor(edge), borderColor);
            }
        }
        QCOMPARE(turnLabel->palette().color(QPalette::Window), BoardColors{}.turnBackground);
        snapshot(QStringLiteral("resign-highlight-") + QString::fromLatin1(QTest::currentDataTag()));

        QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        if (!humanIsBlack) moves.append(QStringLiteral("2g2f"));
        const QStringList sfens = SfenPositionTracer::buildSfenRecord(
            initial + QStringLiteral(" b - 1"), moves, /*hasTerminal=*/true);
        QCOMPARE(sfens.size(), model->rowCount());

        // 指し手列・消費時間列の両方から、終局行を含む任意の行を繰り返し選ぶ。
        view->setColumnHidden(1, false);
        const QList<int> rows = {1, 0, terminalRow, lastMoveRow, 1, terminalRow, 0};
        for (int column = 0; column < 2; ++column) {
            for (int row : rows) {
                const QModelIndex index = model->index(row, column);
                view->scrollTo(index);
                QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                                  view->visualRect(index).center());
                QTRY_COMPARE(view->currentIndex().row(), row);
                QTRY_COMPARE(boardSfen(), sfens.at(row).section(QLatin1Char(' '), 0, 0));
                QCOMPARE(turnToSfen(board()->board()->currentPlayer()),
                         sfens.at(row).section(QLatin1Char(' '), 1, 1));
                QCOMPARE(model->currentHighlightRow(), row);
                const QModelIndexList selected = view->selectionModel()->selectedRows();
                QCOMPARE(selected.size(), 1);
                QCOMPARE(selected.first().row(), row);
                for (int other = 0; other < model->rowCount(); ++other) {
                    const QColor background = model->index(other, column)
                        .data(Qt::BackgroundRole).value<QBrush>().color();
                    QCOMPARE(background == TableStyles::selectionBackground(), other == row);
                }
            }
        }
        snapshot(QStringLiteral("resign-navigation-")
                 + QString::fromLatin1(QTest::currentDataTag()));
    }
    void consideration()
    {
        const auto goCount = []() {
            QFile file(qEnvironmentVariable("AUDIT_USI_LOG"));
            return file.open(QIODevice::ReadOnly) ? file.readAll().count(" go infinite\n") : 0;
        };
        sampleGame();
        QDockWidget* dock = nullptr;
        for (auto* d : window->findChildren<QDockWidget*>()) if (d->windowTitle() == QStringLiteral("検討")) dock = d;
        QVERIFY(dock); dock->show(); dock->raise();
        QToolButton* start = nullptr;
        for (auto* b : dock->findChildren<QToolButton*>()) if (b->text() == QStringLiteral("検討開始")) start = b;
        QVERIFY(start); QVERIFY(start->isEnabled());
        auto* manager = window->findChild<ConsiderationTabManager*>(); QVERIFY(manager);
        QSignalSpy starts(manager, &ConsiderationTabManager::startConsiderationRequested);
        QSignalSpy stops(manager, &ConsiderationTabManager::stopConsiderationRequested);
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討中止"), 3000);
        bool infoFound = false;
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 3000 && !infoFound) {
            for (auto* table : dock->findChildren<QTableView*>())
                if (table->model() && table->model()->rowCount() > 0 && table->model()->columnCount() >= 5) infoFound = true;
            QTest::qWait(30);
        }
        QVERIFY(infoFound);
        QTRY_COMPARE(goCount(), 1);
        snapshot("consideration");
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討開始"), 3000);
        QCOMPARE(starts.size(), 1); QCOMPARE(stops.size(), 1);
        for (int cycle = 2; cycle <= 3; ++cycle) {
            QTest::mouseClick(start, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討中止"), 3000);
            QTRY_COMPARE(goCount(), cycle);
            QTest::mouseClick(start, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討開始"), 3000);
            QCOMPARE(starts.size(), cycle); QCOMPARE(stops.size(), cycle);
        }
        QFile commands(qEnvironmentVariable("AUDIT_USI_LOG"));
        QVERIFY(commands.open(QIODevice::ReadOnly));
        const auto log = commands.readAll();
        QVERIFY2(!log.contains("position sfen startpos"), log.constData());
        // 初期局面はstartposと同じ内容のSFENのどちらでも送信できる。
        const QByteArray initialSfen = " position sfen " + initial.toUtf8() + " b - 1\n";
        QCOMPARE(log.count(" position startpos\n") + log.count(initialSfen), 3);
        QCOMPARE(log.count(" go infinite\n"), 3);
    }
    void evaluationGraphAppearance()
    {
        sampleGame();
        auto* graph = window->evalChart();
        QVERIFY(graph);
        graph->setAutomaticRange(true);
        graph->setLabelFontSize(10);
        graph->setEngine1Name(QStringLiteral("Hayanagi 1.5.0"));
        graph->setEngine2Name(QStringLiteral("YaneuraOu"));
        for (int ply = 1; ply <= 40; ++ply) {
            const int score = (ply < 20 ? ply * 45 : (40 - ply) * 75 - 850);
            if (ply % 2 == 1) graph->appendScoreP1(ply, score);
            else graph->appendScoreP2(ply, score - 70);
        }
        graph->appendScoreP2(42, -31111, false, QStringLiteral("-7"));
        graph->setCurrentPly(42);
        for (auto* dock : window->findChildren<QDockWidget*>()) {
            if (dock->widget() == graph) { dock->show(); dock->raise(); }
        }
        QTest::qWait(100);
        QVERIFY(graph->isVisible());
        QVERIFY(graph->yAxisLimit() <= 2000);
        QVERIFY(graph->chartViewWidget()->accessibleName().contains(QStringLiteral("詰みまで7手")));
        snapshot("evaluation-graph-main");
        QVERIFY(graph->chartViewWidget()->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/evaluation-graph-export.png")));
    }

    void enginePostGameAnalysis_data()
    {
        QTest::addColumn<bool>("humanIsBlack");
        QTest::addColumn<bool>("realAnalysis");
        QTest::newRow("human-black") << true << false;
        QTest::newRow("human-white") << false << false;
        QTest::newRow("human-black-real-analysis") << true << true;
    }
    void enginePostGameAnalysis()
    {
        QFETCH(bool, humanIsBlack);
        QFETCH(bool, realAnalysis);
        const QString realEngine = QStringLiteral(APP_BUILD "/Hayanagi/hayanagi");
        if (realAnalysis && !QFileInfo::exists(realEngine)) QSKIP("Hayanagi is not built");
        armDialog(humanIsBlack ? "gameEngineWhite" : "gameEngineBlack");
        click("actionStartGame");
        auto* recordView = record()->kifuView();
        auto* recordModel = qobject_cast<KifuRecordListModel*>(recordView->model());
        QVERIFY(recordModel);
        if (!humanIsBlack) QTRY_COMPARE_WITH_TIMEOUT(recordModel->rowCount(), 2, 5000);
        const int file = humanIsBlack ? 7 : 3;
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(file, humanIsBlack ? 7 : 3));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(file, humanIsBlack ? 6 : 4));
        const int lastPly = humanIsBlack ? 2 : 3;
        QTRY_COMPARE_WITH_TIMEOUT(recordModel->rowCount(), lastPly + 1, 5000);
        armDialog("auto");
        click("actionResign");
        QTRY_VERIFY(!record()->isNavigationDisabled());
        QCOMPARE(recordModel->rowCount(), lastPly + 2);
        QVERIFY(recordModel->index(lastPly + 1, 0).data().toString().contains(QStringLiteral("投了")));
        QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        if (!humanIsBlack) moves.append(QStringLiteral("2g2f"));
        const auto sfens = SfenPositionTracer::buildSfenRecord(initial + QStringLiteral(" b - 1"), moves, false);
        auto* graph = window->evalChart();
        auto* tree = window->findChild<BranchTreeManager*>();
        QVERIFY(graph && tree);
        if (realAnalysis) {
            auto& settings = SettingsCommon::openSettings();
            settings.beginWriteArray("Engines");
            settings.setArrayIndex(0);
            settings.setValue("name", "Hayanagi");
            settings.setValue("path", realEngine);
            settings.endArray();
        }
        armDialog("startAnalysis");
        click("actionAnalyzeKifu");
        auto* progress = window->findChild<QProgressBar*>("analysisProgressBar");
        auto* results = window->findChild<QTableView*>("analysisResultsTable");
        QVERIFY(progress && results);
        QCOMPARE(progress->maximum(), lastPly + 1); // 投了は着手後の局面ではない。
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 8000);
        QCOMPARE(results->model()->rowCount(), lastPly + 1);
        QTRY_COMPARE(graph->countP1(), lastPly + 1);
        QCOMPARE(graph->countP2(), 0);
        auto* chartView = static_cast<EvaluationChartView*>(graph->chartViewWidget());
        QLineSeries* scores = nullptr;
        for (auto* series : chartView->chart()->series()) {
            if (series->objectName() == "evalSeries1") scores = qobject_cast<QLineSeries*>(series);
        }
        QVERIFY(scores);
        for (int ply = 0; ply <= lastPly; ++ply) {
            QCOMPARE(scores->at(ply).x(), double(ply));
            QCOMPARE(scores->at(ply).y(), results->model()->index(ply, 3).data().toDouble());
        }
        for (const int ply : {0, lastPly, 1, 0}) {
            results->setCurrentIndex(results->model()->index(ply, 0));
            QTest::qWait(150); // グラフの遅延更新後にも選択が戻らないこと。
            QCOMPARE(recordView->currentIndex().row(), ply);
            QCOMPARE(boardSfen(), sfens.at(ply).section(QLatin1Char(' '), 0, 0));
            QCOMPARE(turnToSfen(board()->board()->currentPlayer()), sfens.at(ply).section(QLatin1Char(' '), 1, 1));
            QCOMPARE(graph->currentPly(), ply);
            QCOMPARE(tree->lastHighlightedPly(), ply);
            QCOMPARE(tree->lastHighlightedRow(), 0);
        }
        // グラフから移動しても棋譜・盤面・ツリーが同期する。
        for (auto* dock : window->findChildren<QDockWidget*>()) {
            if (dock->isAncestorOf(graph)) { dock->show(); dock->raise(); }
        }
        QTest::qWait(50);
        const QPoint graphPoint = chartView->mapFromScene(chartView->chart()->mapToScene(
            chartView->chart()->mapToPosition(QPointF(lastPly, 0), scores)));
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, graphPoint);
        QTRY_COMPARE(recordView->currentIndex().row(), lastPly);
        QCOMPARE(graph->currentPly(), lastPly);
        QCOMPARE(tree->lastHighlightedPly(), lastPly);
        // 分岐ツリーから開始局面へ戻る。
        auto* treeView = window->findChild<QGraphicsView*>("branchTreeView");
        QVERIFY(treeView);
        for (auto* dock : window->findChildren<QDockWidget*>()) {
            if (dock->isAncestorOf(treeView)) { dock->show(); dock->raise(); }
        }
        QGraphicsItem* startNode = nullptr;
        for (auto* item : treeView->scene()->items()) {
            if (item->data(BranchTreeManager::BR_ROLE_KIND).isValid()
                && item->data(BranchTreeManager::ROLE_ROW).toInt() == 0
                && item->data(BranchTreeManager::ROLE_PLY).toInt() == 0) startNode = item;
        }
        QVERIFY(startNode);
        treeView->ensureVisible(startNode);
        QTest::qWait(50);
        QTest::mouseClick(treeView->viewport(), Qt::LeftButton, Qt::NoModifier,
                         treeView->mapFromScene(startNode->sceneBoundingRect().center()));
        QTRY_COMPARE(recordView->currentIndex().row(), 0);
        QCOMPARE(graph->currentPly(), 0);
        QCOMPARE(boardSfen(), initial);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QCOMPARE(recordView->currentIndex().row(), 1);
        QCOMPARE(graph->currentPly(), 1);
        QCOMPARE(tree->lastHighlightedPly(), 1);
        snapshot(QStringLiteral("post-game-analysis-") + QString::fromLatin1(QTest::currentDataTag()));
        // 同じ対局を最終局面だけ再解析する。古い点と終局行は残さない。
        AnalysisSettings::setKifuAnalysisFullRange(false);
        AnalysisSettings::setKifuAnalysisStartPly(lastPly);
        AnalysisSettings::setKifuAnalysisEndPly(lastPly);
        armDialog("startAnalysis");
        click("actionAnalyzeKifu");
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 5000);
        QCOMPARE(results->model()->rowCount(), 1);
        QTRY_COMPARE(graph->countP1(), 1);
        QCOMPARE(graph->countP2(), 0);
        QCOMPARE(scores->at(0).x(), double(lastPly));
        QCOMPARE(recordView->currentIndex().row(), lastPly);
        QCOMPARE(graph->currentPly(), lastPly);
        QCOMPARE(tree->lastHighlightedPly(), lastPly);
    }

    void engineAnalysisBranch()
    {
        QFile f(QStringLiteral(REPO "/tests/fixtures/test_branch.kif"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        paste(QString::fromUtf8(f.readAll()));
        auto* tree = window->findChild<BranchTreeManager*>();
        QVERIFY(tree);
        tree->branchNodeActivated(1, 3);
        QTest::qWait(50);
        QCOMPARE(tree->lastHighlightedRow(), 1);
        const QString branchBoard = boardSfen();
        const QString branchMove = record()->kifuView()->model()->index(3, 0).data().toString();
        AnalysisSettings::setKifuAnalysisFullRange(false);
        AnalysisSettings::setKifuAnalysisStartPly(3);
        AnalysisSettings::setKifuAnalysisEndPly(4);
        armDialog("startAnalysis");
        click("actionAnalyzeKifu");
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 7000);
        auto* results = window->findChild<QTableView*>("analysisResultsTable");
        QVERIFY(results);
        auto* model = qobject_cast<KifuAnalysisListModel*>(results->model());
        QVERIFY(model);
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->item(0)->sfen().section(QLatin1Char(' '), 0, 0), branchBoard);
        QCOMPARE(model->index(0, 0).data().toString(), branchMove);
        QCOMPARE(model->item(0)->lastUsiMove(), QStringLiteral("6g6f"));
        tree->branchNodeActivated(0, 4);
        QTest::qWait(50);
        results->setCurrentIndex(model->index(0, 0));
        QTest::qWait(150);
        QCOMPARE(tree->lastHighlightedRow(), 1);
        QCOMPARE(record()->kifuView()->currentIndex().row(), 3);
        QCOMPARE(tree->lastHighlightedRow(), 1);
        QCOMPARE(tree->lastHighlightedPly(), 3);
        QCOMPARE(window->evalChart()->currentPly(), 3);
        QCOMPARE(boardSfen(), branchBoard);
        // 本譜を表示して条件ダイアログをキャンセルしても、結果の分岐を保持する。
        tree->branchNodeActivated(0, 4);
        armDialog("auto");
        click("actionAnalyzeKifu");
        auto* graph = window->evalChart();
        QTRY_COMPARE(graph->countP1(), 2);
        auto* chartView = static_cast<EvaluationChartView*>(graph->chartViewWidget());
        QLineSeries* scores = nullptr;
        for (auto* series : chartView->chart()->series()) {
            if (series->objectName() == "evalSeries1") scores = qobject_cast<QLineSeries*>(series);
        }
        QVERIFY(scores);
        for (auto* dock : window->findChildren<QDockWidget*>()) {
            if (dock->isAncestorOf(graph)) { dock->show(); dock->raise(); }
        }
        QTest::qWait(50);
        const QPoint point = chartView->mapFromScene(chartView->chart()->mapToScene(
            chartView->chart()->mapToPosition(QPointF(3, 0), scores)));
        QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QTRY_COMPARE(record()->kifuView()->currentIndex().row(), 3);
        QCOMPARE(tree->lastHighlightedRow(), 1);
        QCOMPARE(tree->lastHighlightedPly(), 3);
        QCOMPARE(graph->currentPly(), 3);
        QCOMPARE(boardSfen(), branchBoard);
        snapshot("analysis-branch-synchronized");
    }

    void engineAnalysisCancelPreservesGraph()
    {
        sampleGame();
        auto* graph = window->evalChart();
        graph->appendScoreP1(1, 75);
        graph->appendScoreP2(2, -120);
        graph->setCurrentPly(2);
        armDialog("auto");
        click("actionAnalyzeKifu");
        QCOMPARE(dialogClass, QStringLiteral("KifuAnalysisDialog"));
        QCOMPARE(graph->countP1(), 1);
        QCOMPARE(graph->countP2(), 1);
        QCOMPARE(graph->currentPly(), 2);
    }

    void engineAnalysisSettings()
    {
        AnalysisSettings::setKifuAnalysisByoyomiSec(3);
        {
            KifuAnalysisDialog dialog;
            dialog.setMaxPly(14);
            dialog.show();
            auto* summary = dialog.findChild<QLabel*>("analysisSummary");
            auto* range = dialog.findChild<QRadioButton*>("radioButtonRangePosition");
            auto* start = dialog.findChild<QSpinBox*>("spinBoxStartPly");
            auto* end = dialog.findChild<QSpinBox*>("spinBoxEndPly");
            auto* time = dialog.findChild<QSpinBox*>("byoyomiSec");
            auto* buttons = dialog.findChild<QDialogButtonBox*>();
            QVERIFY(summary && range && start && end && time && buttons);
            QVERIFY(summary->text().contains(QStringLiteral("15局面")));
            QVERIFY(summary->text().contains(QStringLiteral("45秒")));
            QTest::qWait(50);
            QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/analysis-settings.png")));
            range->click();
            start->setValue(5);
            end->setValue(8);
            time->setValue(10);
            QVERIFY(summary->text().contains(QStringLiteral("4局面")));
            QVERIFY(summary->text().contains(QStringLiteral("40秒")));
            start->setValue(10);
            QCOMPARE(end->value(), 10);
            QVERIFY(summary->text().contains(QStringLiteral("1局面")));
            start->setValue(0);
            end->setValue(0);
            dialog.resize(650, 440);
            QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
            QCOMPARE(dialog.result(), int(QDialog::Accepted));
            QCOMPARE(dialog.startPly(), 0);
            QCOMPARE(dialog.endPly(), 0);
            QCOMPARE(dialog.byoyomiSec(), 10);
        }
        {
            KifuAnalysisDialog restored;
            restored.setMaxPly(120);
            restored.show();
            QCOMPARE(restored.findChild<QSpinBox*>("spinBoxEndPly")->value(), 0);
            QVERIFY(restored.findChild<QRadioButton*>("radioButtonRangePosition")->isChecked());
            QCOMPARE(restored.size(), QSize(650, 440));
            auto* increase = restored.findChild<QPushButton*>("btnFontIncrease");
            QVERIFY(increase);
            while (increase->isEnabled()) increase->click();
            QTest::qWait(50);
            auto* ok = restored.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            QVERIFY(restored.rect().contains(QRect(ok->mapTo(&restored, QPoint()), ok->size())));
            for (const QString& name : {QStringLiteral("analysisSummary"), QStringLiteral("rangeHint")}) {
                auto* label = restored.findChild<QLabel*>(name);
                QVERIFY(label);
                QVERIFY2(label->height() >= label->heightForWidth(label->width()), qPrintable(name));
            }
            QVERIFY(restored.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/analysis-settings-large.png")));
            restored.resize(restored.width() + 20, restored.height() + 20);
            const QSize canceledSize = restored.size();
            restored.reject();
            QCOMPARE(AnalysisSettings::kifuAnalysisDialogSize(), canceledSize);
        }
        SettingsCommon::openSettings().remove("Engines");
        KifuAnalysisDialog empty;
        empty.setMaxPly(0);
        QVERIFY(!empty.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        QVERIFY(!empty.findChild<QPushButton*>("engineSetting")->isEnabled());
        QVERIFY(!empty.findChild<QLabel*>("engineHint")->isHidden());
    }

    void engineAnalysisLayout()
    {
        KifuAnalysisListModel model;
        AnalysisResultsPresenter presenter;
        QWidget* container = presenter.containerWidget();
        container->setParent(window.get(), Qt::Window);
        container->resize(1100, 480);
        presenter.showWithModel(&model);
        container->show();
        presenter.beginAnalysis(24, QStringLiteral("Hayanagi 1.5.0"));
        auto* table = presenter.view();
        auto* progress = container->findChild<QProgressBar*>("analysisProgressBar");
        auto* status = container->findChild<QLabel*>("analysisStatusLabel");
        QVERIFY(table && progress && status);
        QCOMPARE(progress->maximum(), 24);
        table->setColumnWidth(0, 200);
        const QString pv = QStringLiteral("▲７六歩(77)△３四歩(33)▲２六歩(27)△８四歩(83)▲２五歩(26)△８五歩(84)▲７七角(88)");
        for (int row = 0; row < 24; ++row) {
            auto* item = new KifuAnalysisResultsDisplay(
                QStringLiteral("%1 ▲７六歩(77)").arg(row), QString::number(row * 25), QStringLiteral("25"), pv);
            item->setCandidateMove(QStringLiteral("▲７六歩(77)"));
            model.appendItem(item);
        }
        QCOMPARE(progress->value(), 24);
        presenter.showAnalysisComplete(24);
        QTest::qWait(50);
        QCOMPARE(table->columnWidth(0), 200);
        QVERIFY(status->text().contains(QStringLiteral("解析完了")));
        QVERIFY(table->model()->index(0, 7).data(Qt::ToolTipRole).toString().contains(pv));
        QVERIFY(container->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/analysis-layout.png")));
        QSignalSpy boardRequests(&presenter, &AnalysisResultsPresenter::rowDoubleClicked);
        table->setCurrentIndex(model.index(0, 6));
        table->setFocus();
        QTest::keyClick(table, Qt::Key_Return);
        QCOMPARE(boardRequests.size(), 1);
        container->resize(520, 400);
        QTest::qWait(50);
        QVERIFY(table->horizontalScrollBar()->maximum() > 0);
        table->scrollTo(model.index(0, 7));
        QVERIFY(table->visualRect(model.index(0, 7)).intersects(table->viewport()->rect()));
        QVERIFY(container->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/analysis-narrow.png")));
        auto* increase = container->findChild<QPushButton*>("analysisFontIncrease");
        QVERIFY(increase);
        const int oldHeight = table->verticalHeader()->defaultSectionSize();
        increase->click();
        QVERIFY(table->verticalHeader()->defaultSectionSize() > oldHeight);
        QCOMPARE(table->columnWidth(0), 200);
        {
            AnalysisResultsPresenter restored;
            QWidget* restoredContainer = restored.containerWidget();
            restoredContainer->setParent(window.get(), Qt::Window);
            restored.showWithModel(&model);
            QTest::qWait(50);
            QCOMPARE(restored.view()->columnWidth(0), 200);
            delete restoredContainer;
        }
        model.clearAllItems();
        QVERIFY(!progress->isVisible());
        QVERIFY(status->text().contains(QStringLiteral("開始してください")));
        delete container;
    }

    void engineAnalysis()
    {
        sampleGame();
        armDialog("startAnalysis"); click("actionAnalyzeKifu");
        QCOMPARE(dialogClass, QString("KifuAnalysisDialog"));
        armDialog("auto");
        QDockWidget* dock = nullptr;
        for (auto* d : window->findChildren<QDockWidget*>()) if (d->windowTitle() == QStringLiteral("棋譜解析")) dock = d;
        QVERIFY(dock); dock->show(); dock->raise();
        auto* table = dock->findChild<QTableView*>(); QVERIFY(table); QVERIFY(table->model());
        QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() >= 4, 7000);
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 4000);
        auto* progress = dock->findChild<QProgressBar*>("analysisProgressBar");
        auto* status = dock->findChild<QLabel*>("analysisStatusLabel");
        QVERIFY(progress && status);
        QCOMPARE(progress->value(), table->model()->rowCount());
        QCOMPARE(progress->value(), progress->maximum());
        QVERIFY(status->text().contains(QStringLiteral("解析完了")));
        QVERIFY(!QApplication::activeModalWidget());
        snapshot("analysis-results");
    }
    void engineAnalysisRange()
    {
        sampleGame();
        AnalysisSettings::setKifuAnalysisFullRange(false);
        AnalysisSettings::setKifuAnalysisStartPly(2);
        AnalysisSettings::setKifuAnalysisEndPly(3);
        armDialog("startAnalysis");
        click("actionAnalyzeKifu");
        auto* table = window->findChild<QTableView*>("analysisResultsTable");
        auto* progress = window->findChild<QProgressBar*>("analysisProgressBar");
        QVERIFY(table && progress);
        QCOMPARE(progress->maximum(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 7000);
        QCOMPARE(table->model()->rowCount(), 2);
        QCOMPARE(table->model()->index(0, 5).data().toString(), QStringLiteral("-"));
        table->setCurrentIndex(table->model()->index(0, 0));
        QTRY_COMPARE(record()->kifuView()->currentIndex().row(), 2);
        table->setCurrentIndex(table->model()->index(1, 0));
        QTRY_COMPARE(record()->kifuView()->currentIndex().row(), 3);
        snapshot("analysis-range");
    }

    void engineAnalysisRecord()
    {
        const QString engine = QStringLiteral(APP_BUILD "/Hayanagi/hayanagi");
        if (!QFileInfo::exists(engine)) QSKIP("Hayanagi is not built");
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray("Engines");
        settings.setArrayIndex(0);
        settings.setValue("name", "Hayanagi");
        settings.setValue("path", engine);
        settings.endArray();
        const QString kifu = qEnvironmentVariable("ANALYSIS_AUDIT_KIF",
            QStringLiteral(REPO "/tests/fixtures/test_kiou_comments.kif"));
        armDialog("file", kifu);
        click("actionOpenKifuFile");
        QTRY_VERIFY_WITH_TIMEOUT(record()->kifuView()->model()->rowCount() > 14, 3000);
        AnalysisSettings::setKifuAnalysisFullRange(false);
        AnalysisSettings::setKifuAnalysisStartPly(0);
        AnalysisSettings::setKifuAnalysisEndPly(14);
        armDialog("startAnalysis");
        click("actionAnalyzeKifu");
        auto* table = window->findChild<QTableView*>("analysisResultsTable");
        QVERIFY(table);
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 22000);
        QCOMPARE(table->model()->rowCount(), 15);
        bool hasScore = false;
        for (int row = 0; row < 15; ++row) {
            bool ok = false;
            table->model()->index(row, 3).data().toString().toInt(&ok);
            hasScore = hasScore || ok;
        }
        QVERIFY(hasScore);
        QDockWidget* dock = nullptr;
        for (auto* candidate : window->findChildren<QDockWidget*>()) {
            if (candidate->isAncestorOf(table)) dock = candidate;
        }
        QVERIFY(dock);
        dock->setFloating(true);
        dock->resize(1460, 700);
        dock->show();
        QTest::qWait(100);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/analysis-record.png")));
        table->setCurrentIndex(table->model()->index(7, 0));
        QTRY_COMPARE(record()->kifuView()->currentIndex().row(), 7);
        table->scrollTo(table->model()->index(7, 6));
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                         table->visualRect(table->model()->index(7, 6)).center());
        QDialog* pv = nullptr;
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (QString::fromLatin1(widget->metaObject()->className()) == "PvBoardDialog")
                pv = qobject_cast<QDialog*>(widget);
        }
        QVERIFY(pv && pv->isVisible());
        pv->close();
    }

    void engineMateNoMate()
    {
        sampleGame();
        armDialog("startAnalysis"); click("actionTsumeShogiSearch");
        QCOMPARE(dialogClass, QString("TsumeShogiSearchDialog"));
        armDialog("auto"); QTest::qWait(700);
        QVERIFY(!action("actionStopTsumeSearch")->isEnabled());
        QVERIFY(window->isVisible());
        QFile commands(qEnvironmentVariable("AUDIT_USI_LOG"));
        QVERIFY(commands.open(QIODevice::ReadOnly));
        QVERIFY(commands.readAll().contains(" go mate 1000\n"));
    }
    void engineGeneratorStartStop()
    {
        armDialog("generator"); click("actionTsumeshogiGenerator");
        QVERIFY(dialogHandled); QCOMPARE(dialogClass, QString("TsumeshogiGeneratorDialog"));
    }
    void registration()
    {
        QCOMPARE(EngineListSettings::loadEngines().size(), 0);
        armDialog("register"); click("actionEngineSettings");
        const auto engines = EngineListSettings::loadEngines();
        QCOMPARE(engines.size(), 1); QCOMPARE(engines.first().name, QString("Audit USI"));
    }
    void gameInfoPresentation()
    {
        auto* dock = window->findChild<QDockWidget*>("GameInfoDock");
        auto* controller = window->findChild<GameInfoPaneController*>();
        auto* table = window->findChild<QTableWidget*>("gameInfoTable");
        auto* apply = window->findChild<QPushButton*>("gameInfoApply");
        QVERIFY(dock && controller && table && apply);
        window->resize(1400, 1120);
        dock->show(); dock->raise();
        QTest::qWait(50);
        QVERIFY(!apply->isEnabled());
        QCOMPARE(table->rowCount(), 9);
        QVERIFY(table->item(1, 1)->text().isEmpty());
        QVERIFY(table->item(2, 1)->text().isEmpty());
        QVERIFY(table->item(5, 1)->text().isEmpty());
        QVERIFY(table->item(1, 1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("未開始")));
        snapshot("game-info-startup");
        const QString initialKif = copy("actionCopyKIF");
        QVERIFY(!initialKif.contains(QStringLiteral("未開始")));
        QVERIFY(!initialKif.contains(QStringLiteral("未設定")));

        table->setColumnWidth(0, 160);
        armDialog("file", QStringLiteral(REPO "/tests/fixtures/test_kiou_comments.kif"));
        click("actionOpenKifuFile");
        QVERIFY(dialogHandled);
        QTRY_VERIFY(record()->kifuView()->model()->rowCount() > 90);
        dock->show(); dock->raise();
        QCOMPARE(table->columnWidth(0), 160);
        QVERIFY(!apply->isEnabled());
        QTest::qWait(50);
        snapshot("game-info-loaded");

        int noteRow = -1;
        for (int row = 0; row < table->rowCount(); ++row)
            if (table->item(row, 0)->text() == QStringLiteral("備考")) noteRow = row;
        QVERIFY(noteRow >= 0);
        window->activateWindow();
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        table->setCurrentCell(noteRow, 1);
        table->scrollToItem(table->currentItem());
        table->editItem(table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        const QString note = QStringLiteral("長い備考も、ウィンドウの幅に合わせて折り返して表示します。会場や対局条件などの情報を編集できます。");
        editor->setText(note);
        QVERIFY(apply->isEnabled());
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(table->item(noteRow, 1)->text(), note);
        QVERIFY(!apply->isEnabled());
        const QString saved = copy("actionCopyKIF");
        QVERIFY(saved.contains(QStringLiteral("備考：") + note));
        armDialog("discard");
        click("actionNewGame");
        QCOMPARE(table->rowCount(), 9);
        QVERIFY(table->item(1, 1)->text().isEmpty());
        QVERIFY(table->item(8, 1)->text().isEmpty());
        QVERIFY(!controller->isDirty());
        paste(saved);
        dock->show(); dock->raise();
        QCOMPARE(table->columnWidth(0), 160);
        QVERIFY(!controller->isDirty());
        QCOMPARE(table->item(noteRow, 1)->text(), note);

        dock->setFloating(true);
        dock->resize(460, 560);
        QTest::qWait(50);
        QVERIFY(dock->width() <= 460);
        auto* toolbar = dock->findChild<QWidget*>("gameInfoToolbar");
        QVERIFY(toolbar);
        for (auto* button : toolbar->findChildren<QAbstractButton*>()) {
            const QRect rect(button->mapTo(toolbar, QPoint()), button->size());
            QVERIFY2(toolbar->rect().contains(rect), qPrintable(button->objectName()));
        }
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/game-info-narrow.png")));
        controller->setFontSize(16);
        QTest::qWait(50);
        QVERIFY(table->rowHeight(noteRow) > table->rowHeight(0));
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/game-info-large-font.png")));
    }

    void commentPresentation()
    {
        armDialog("file", QStringLiteral(REPO "/tests/fixtures/test_kiou_comments.kif"));
        click("actionOpenKifuFile");
        QVERIFY(dialogHandled);
        auto* table = record()->kifuView();
        QTRY_VERIFY(table->model()->rowCount() > 90);
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        record()->onToggleCommentColumn(true);
        record()->onToggleBookmarkColumn(false);
        auto* edit = window->findChild<QTextEdit*>("kifuCommentEdit");
        QVERIFY(edit);
        auto* dock = qobject_cast<QDockWidget*>(edit->parentWidget()->parentWidget());
        QVERIFY(dock);
        dock->show(); dock->raise();
        auto* panel = window->findChild<CommentEditorPanel*>();
        auto* apply = window->findChild<QPushButton*>("kifuCommentApply");
        auto* position = window->findChild<QLabel*>("kifuCommentPosition");
        QVERIFY(panel && apply && position);
        QTRY_COMPARE(panel->currentMoveIndex(), 0);
        QCOMPARE(position->text(), QStringLiteral("開始局面"));
        QVERIFY(!apply->isEnabled());
        QVERIFY(edit->toPlainText().contains(QStringLiteral("\n\n【棋王戦第３局｜ABEMA】")));
        QVERIFY(edit->document()->firstBlock().blockFormat().lineHeight() >= 130);
        const QString opening = edit->toPlainText();
        QCOMPARE(table->model()->data(table->model()->index(0, 3)).toString(), opening.simplified());
        QTextDocument tooltip;
        tooltip.setHtml(table->model()->data(table->model()->index(0, 3), Qt::ToolTipRole).toString());
        QCOMPARE(tooltip.toPlainText(), opening);

        // 通常クリックでは選択・編集でき、Ctrl+クリックだけがリンクを開く。
        auto cursor = edit->document()->find(QStringLiteral("https://abema.tv/"));
        QVERIFY(!cursor.isNull());
        cursor.setPosition(cursor.selectionStart() + 5);
        edit->setTextCursor(cursor);
        edit->ensureCursorVisible();
        QTest::qWait(30);
        const QPoint linkPoint = edit->cursorRect().center();
        QVERIFY(!edit->anchorAt(linkPoint).isEmpty());
        QTest::mouseClick(edit->viewport(), Qt::LeftButton, Qt::NoModifier, linkPoint);
        QVERIFY(urls.isEmpty());
        QTest::mouseClick(edit->viewport(), Qt::LeftButton, Qt::ControlModifier, linkPoint);
        QCOMPARE(urls.size(), 1);
        QCOMPARE(urls.first(), QUrl("https://abema.tv/channels/shogi/slots/CmppFMdYCe6dn7"));
        QVERIFY(!panel->hasUnsavedComment());
        edit->moveCursor(QTextCursor::Start);
        edit->verticalScrollBar()->setValue(0);
        QTest::qWait(50);
        snapshot("comment-presentation");

        // 空行は保存・再読込後も残る。
        const QString saved = copy("actionCopyKIF");
        QVERIFY(saved.contains(QStringLiteral("\n*\n*【棋王戦第３局｜ABEMA】")));
        click("actionNewGame");
        paste(saved);
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTRY_COMPARE(edit->toPlainText(), opening);
    }

    void commentEditing()
    {
        sampleGame();
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        auto* edit = window->findChild<QTextEdit*>("kifuCommentEdit");
        auto* apply = window->findChild<QPushButton*>("kifuCommentApply");
        auto* panel = window->findChild<CommentEditorPanel*>();
        auto* position = window->findChild<QLabel*>("kifuCommentPosition");
        QVERIFY(edit && apply && panel && position);
        auto* dock = qobject_cast<QDockWidget*>(edit->parentWidget()->parentWidget());
        QVERIFY(dock);
        dock->show(); dock->raise();
        QTRY_COMPARE(panel->currentMoveIndex(), 1);
        QCOMPARE(position->text(), QStringLiteral("1手目"));
        QVERIFY(edit->toPlainText().isEmpty());
        QVERIFY(!edit->placeholderText().isEmpty());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!panel->hasUnsavedComment());

        const QString comment = QStringLiteral("検討 <b>強調ではない</b> &amp; 2*3\n\n"
                                               "https://example.com/?a=1&b=2*x\n次の段落");
        auto* mime = new QMimeData;
        mime->setText(comment);
        mime->setHtml(QStringLiteral("<h1>書式付きの貼り付け</h1>"));
        QApplication::clipboard()->setMimeData(mime);
        window->activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(window.get()));
        edit->setFocus();
        QTRY_VERIFY(edit->hasFocus());
        QTest::keyClick(edit, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(edit->toPlainText(), comment);
        QVERIFY(panel->hasUnsavedComment());
        QVERIFY(apply->isEnabled());
        QSignalSpy updates(panel, &CommentEditorPanel::commentUpdated);
        QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates.first().at(0).toInt(), 1);
        QCOMPARE(updates.first().at(1).toString(), comment);
        QCOMPARE(edit->toPlainText(), comment);
        QVERIFY(!panel->hasUnsavedComment());
        QVERIFY(!apply->isEnabled());
        auto* model = record()->kifuView()->model();
        QCOMPARE(model->data(model->index(1, 3)).toString(), comment.simplified());
        QTextDocument tooltip;
        tooltip.setHtml(model->data(model->index(1, 3), Qt::ToolTipRole).toString());
        QCOMPARE(tooltip.toPlainText(), comment);
        QTest::keyClick(edit, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(updates.size(), 1);

        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTRY_VERIFY(edit->toPlainText().isEmpty());
        QTest::mouseClick(record()->prevButton(), Qt::LeftButton);
        QTRY_COMPARE(edit->toPlainText(), comment);
        const QString saved = copy("actionCopyKIF");
        QVERIFY(saved.contains(QStringLiteral("2*3\n*\n*https://example.com/?a=1&b=2*x")));
        armDialog("discard"); click("actionNewGame"); paste(saved);
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTRY_COMPARE(edit->toPlainText(), comment);
        edit->setFocus();
        QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(edit, Qt::Key_Backspace);
        QTest::mouseClick(apply, Qt::LeftButton);
        QVERIFY(edit->toPlainText().isEmpty());
        QVERIFY(!panel->hasUnsavedComment());
        QVERIFY(!apply->isEnabled());
        QVERIFY(model->data(model->index(1, 3), Qt::ToolTipRole).toString().isEmpty());
        QVERIFY(!copy("actionCopyKIF").contains(QStringLiteral("コメントなし")));
    }

    void commentsAndBookmark()
    {
        sampleGame(); QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::qWait(80);
        QDockWidget* dock = nullptr;
        for (auto* d : window->findChildren<QDockWidget*>()) if (d->windowTitle() == QStringLiteral("棋譜コメント")) dock = d;
        QVERIFY(dock); dock->show(); dock->raise();
        auto* edit = dock->findChild<QTextEdit*>(); QVERIFY(edit);
        QPushButton* update = nullptr;
        for (auto* b : dock->findChildren<QPushButton*>()) if (b->text() == QStringLiteral("コメント更新")) update = b;
        QVERIFY(update);
        auto* panel = window->findChild<CommentEditorPanel*>(); QVERIFY(panel);
        qInfo() << "comment before editing selected/index" << record()->kifuView()->currentIndex().row() << panel->currentMoveIndex();
        QSignalSpy spy(panel, &CommentEditorPanel::commentUpdated);
        edit->setFocus(); QTest::keyClick(edit, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClicks(edit, "Audit comment at ply one");
        QTest::mouseClick(update, Qt::LeftButton);
        QTest::qWait(80);
        QCOMPARE(spy.size(), 1);
        qInfo() << "comment signal" << spy.first();
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        QTest::qWait(80);
        QVERIFY(!edit->toPlainText().contains("Audit comment"));
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::qWait(80);
        qInfo() << "comment after navigating" << panel->currentMoveIndex() << edit->toPlainText() << copy("actionCopyKIF");
        QVERIFY(edit->toPlainText().contains("Audit comment at ply one"));
        QVERIFY(copy("actionCopyKIF").contains("*Audit comment at ply one"));
        armDialog("bookmark", "Audit bookmark"); QTest::mouseClick(record()->bookmarkEditButton(), Qt::LeftButton);
        QVERIFY(dialogHandled);
        const auto saved = copy("actionCopyKIF");
        QVERIFY(saved.contains("&Audit bookmark"));
        armDialog("discard");
        click("actionNewGame"); paste(saved);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::qWait(80);
        QVERIFY(edit->toPlainText().contains("Audit comment at ply one"));
        QVERIFY(copy("actionCopyKIF").contains("&Audit bookmark"));
    }
    void branchPanelCollapse()
    {
        sampleGame();
        auto* toggle = record()->findChild<QToolButton*>(QStringLiteral("kifuBranchToggle"));
        QVERIFY(toggle);
        QVERIFY(toggle->isChecked());
        const int expandedWidth = record()->width();
        const QString originalPosition = boardSfen();
        QTest::mouseClick(toggle, Qt::LeftButton);
        QVERIFY(record()->branchView()->isHidden());
        QTRY_VERIFY(record()->width() < expandedWidth);
        QCOMPARE(boardSfen(), originalPosition);
        QVERIFY(!GameSettings::kifuBranchExpanded());
        {
            RecordPane restored;
            QVERIFY(restored.branchView()->isHidden());
            QVERIFY(!restored.findChild<QToolButton*>(QStringLiteral("kifuBranchToggle"))->isChecked());
        }
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTRY_COMPARE(record()->kifuView()->currentIndex().row(), 1);
        const QString movedPosition = boardSfen();
        QVERIFY(movedPosition != originalPosition);
        board()->setBlackPlayerName(QStringLiteral("Hayanagi 1.5.0"));
        board()->setWhitePlayerName(QStringLiteral("将棋盤Q テスト対局者"));
        snapshot("branch-panel-collapsed");
        QTest::mouseClick(toggle, Qt::LeftButton);
        QVERIFY(record()->branchView()->isVisible());
        QTRY_COMPARE(record()->width(), expandedWidth);
        QCOMPARE(boardSfen(), movedPosition);
        QVERIFY(GameSettings::kifuBranchExpanded());
    }
    void engineConsiderationLayout()
    {
        sampleGame();
        auto* manager = window->findChild<ConsiderationTabManager*>(); QVERIFY(manager);
        auto* view = manager->considerationView(); QVERIFY(view);
        auto* model = manager->considerationModel(); QVERIFY(model);
        QDockWidget* dock = nullptr;
        for (auto* d : window->findChildren<QDockWidget*>())
            if (d->windowTitle() == QStringLiteral("検討")) dock = d;
        QVERIFY(dock); dock->show(); dock->raise();
        auto* unlimited = dock->findChild<QRadioButton*>("considerationUnlimited");
        auto* timed = dock->findChild<QRadioButton*>("considerationTimed");
        auto* seconds = dock->findChild<QSpinBox*>("considerationSeconds");
        auto* toolbar = dock->findChild<QWidget*>("considerationToolbar");
        auto* increase = dock->findChild<QToolButton*>("considerationFontIncrease");
        QVERIFY(unlimited && timed && seconds && toolbar && increase);
        manager->setConsiderationTimeLimit(true, 20);
        QVERIFY(!seconds->isEnabled());
        QTest::mouseClick(timed, Qt::LeftButton);
        QVERIFY(!unlimited->isChecked()); QVERIFY(seconds->isEnabled());
        QTest::mouseClick(unlimited, Qt::LeftButton);
        QVERIFY(!timed->isChecked()); QVERIFY(!seconds->isEnabled());
        manager->setConsiderationTimeLimit(false, 20);
        manager->setConsiderationRunning(true);
        QVERIFY(!seconds->isEnabled()); QVERIFY(!timed->isEnabled());
        manager->setConsiderationRunning(false);
        QVERIFY(seconds->isEnabled()); QVERIFY(timed->isEnabled());

        const QString pv = QStringLiteral("▲７六歩(77)△３四歩(33)▲２六歩(27)△８四歩(83)▲２五歩(26)△８五歩(84)▲７七角(88)△３二金(41)");
        for (int rank = 1; rank <= 6; ++rank) {
            auto* info = new ShogiInfoRecord(QStringLiteral("11006"), QStringLiteral("22"),
                QStringLiteral("29113787"), QString::number(150 - rank * 10), pv + pv);
            info->setMultipv(rank);
            model->updateByMultipv(info, 6);
        }
        manager->setConsiderationMultiPV(6);
        manager->setConsiderationEngineName(QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2"));
        auto* summary = new UsiCommLogModel(window.get());
        summary->setEngineName(QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2"));
        summary->setSearchedMove(QStringLiteral("▲７六歩(77)"));
        summary->setSearchDepth(QStringLiteral("22/40"));
        summary->setNodeCount(QStringLiteral("29,113,787"));
        summary->setNodesPerSecond(QStringLiteral("2,645,265"));
        summary->setHashUsage(QStringLiteral("14%"));
        manager->considerationInfo()->setModel(summary);
        QTest::qWait(80);
        QCOMPARE(model->headerData(0, Qt::Horizontal).toString(), QStringLiteral("時間(ms)"));
        QVERIFY(model->index(0, 5).data(Qt::ToolTipRole).toString().contains(pv + pv));
        QVERIFY(view->columnWidth(1) < 100);
        QVERIFY(view->columnWidth(4) < 100);
        QVERIFY(view->columnWidth(5) > view->viewport()->width() / 2);
        QSignalSpy pvClicked(manager, &ConsiderationTabManager::pvRowClicked);
        const auto index = model->index(2, 4);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, view->visualRect(index).center());
        QCOMPARE(pvClicked.size(), 1);
        QCOMPARE(pvClicked.first().at(1).toInt(), 2);
        // 盤面ダイアログを閉じてからレイアウトを確認する。
        for (auto* dialog : window->findChildren<QDialog*>()) dialog->close();
        snapshot("consideration-layout");

        view->setColumnWidth(2, 180);
        auto* infoTable = manager->considerationInfo()->findChild<QTableWidget*>(); QVERIFY(infoTable);
        infoTable->setColumnWidth(0, 290);
        if (action("actionLockDocks")->isChecked()) click("actionLockDocks");
        dock->setFloating(true);
        dock->resize(660, 460);
        QTest::qWait(80);
        for (auto* control : toolbar->findChildren<QWidget*>()) {
            if (control->isVisible())
                QVERIFY2(toolbar->rect().contains(QRect(control->mapTo(toolbar, QPoint()), control->size())),
                         qPrintable(control->objectName()));
        }
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/consideration-narrow.png")));
        for (int i = 0; i < 4; ++i) QTest::mouseClick(increase, Qt::LeftButton);
        QTest::qWait(80);
        QCOMPARE(manager->considerationFontSize(), AnalysisSettings::considerationFontSize());
        for (int col = 0; col < 5; ++col)
            QVERIFY(view->columnWidth(col) >= view->fontMetrics().horizontalAdvance(model->headerData(col, Qt::Horizontal).toString()));
        QCOMPARE(AnalysisSettings::thinkingViewColumnWidths(2).at(2), 180);

        QWidget restoredPanel;
        new QVBoxLayout(&restoredPanel);
        ConsiderationTabManager restored;
        restored.setConsiderationThinkingModel(model);
        restored.buildConsiderationUi(&restoredPanel);
        QCOMPARE(restored.considerationView()->columnWidth(2), 180);
        QCOMPARE(restored.considerationInfo()->columnWidths().at(0), 290);
        restored.setConsiderationTimeLimit(true, 20);
        QVERIFY(!restoredPanel.findChild<QSpinBox*>("considerationSeconds")->isEnabled());
    }
    void engineInfoLayout()
    {
        UsiCommLogModel model;
        EngineInfoWidget info(nullptr, true);
        model.setEngineName(QStringLiteral("Hayanagi 1.5.0"));
        model.setPredictiveMove(QStringLiteral("▲７六歩(77)"));
        model.setSearchedMove(QStringLiteral("△３四歩(33)"));
        model.setSearchDepth(QStringLiteral("12/18"));
        model.setNodeCount(QStringLiteral("1,234,567"));
        model.setNodesPerSecond(QStringLiteral("456,789"));
        model.setHashUsage(QStringLiteral("12%"));
        info.setModel(&model);
        info.resize(1200, info.height());
        info.show();
        auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
        QVERIFY(table);
        QTRY_COMPARE(table->horizontalScrollBar()->maximum(), 0);
        QVERIFY(table->columnWidth(0) < info.width() / 3);
        const auto widths = info.columnWidths();
        info.resize(400, info.height());
        QTRY_VERIFY(table->horizontalScrollBar()->maximum() > 0);
        QCOMPARE(info.columnWidths(), widths);
        table->horizontalScrollBar()->setValue(table->horizontalScrollBar()->maximum());
        QVERIFY(table->visualRect(table->model()->index(0, 6)).right() < table->viewport()->width());
        info.resize(1200, info.height());
        QTRY_COMPARE(table->horizontalScrollBar()->maximum(), 0);
        info.setFontSize(16);
        QTRY_VERIFY(table->columnWidth(4) >= table->fontMetrics().horizontalAdvance(table->item(0, 4)->text()) + 6);
        auto customWidths = info.columnWidths();
        customWidths[0] += 40;
        info.setColumnWidths(customWidths);
        info.resize(500, info.height());
        info.resize(1300, info.height());
        QCOMPARE(info.columnWidths(), customWidths);
        model.setEngineName(QStringLiteral("An engine with a very long display name"));
        QCOMPARE(info.columnWidths(), customWidths);
        QCOMPARE(table->item(0, 0)->toolTip(), model.engineName());
    }
    void branchNavigation()
    {
        QFile f(QStringLiteral(REPO "/tests/fixtures/test_branch.kif")); QVERIFY(f.open(QIODevice::ReadOnly));
        paste(QString::fromUtf8(f.readAll()));
        QTest::mouseClick(record()->firstButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::qWait(80);
        auto* branches = record()->branchView(); QVERIFY(branches->model());
        QVERIFY(branches->model()->rowCount() >= 2);
        const auto idx = branches->model()->index(1, 0);
        QTest::mouseClick(branches->viewport(), Qt::LeftButton, Qt::NoModifier, branches->visualRect(idx).center());
        QTest::qWait(30);
        qInfo() << "selected branch board" << boardSfen();
        QCOMPARE(boardSfen(), QString("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2PP5/PP2PPPPP/1B5R1/LNSGKGSNL"));
        QCOMPARE(copy("actionCopySFEN"), boardSfen() + " w - 4");
        QModelIndex back;
        for (int r = 0; r < branches->model()->rowCount(); ++r) {
            const auto candidate = branches->model()->index(r, 0);
            if (candidate.data().toString() == QStringLiteral("本譜へ戻る")) back = candidate;
        }
        QVERIFY(back.isValid());
        QTest::mouseClick(branches->viewport(), Qt::LeftButton, Qt::NoModifier, branches->visualRect(back).center());
        QTest::qWait(50);
        QCOMPARE(boardSfen(), QString("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL"));
        QCOMPARE(copy("actionCopySFEN"), boardSfen() + " w - 4");
    }
    void menuDockAction()
    {
        auto* menu = window->findChild<MenuWindow*>(); QVERIFY(menu);
        auto* dock = qobject_cast<QDockWidget*>(menu->parentWidget()); QVERIFY(dock);
        if (!dock->toggleViewAction()->isChecked()) clickAction(dock->toggleViewAction());
        dock->raise();
        auto* tabs = menu->findChild<QTabWidget*>(); QVERIFY(tabs); QCOMPARE(tabs->count(), 7);
        MenuButtonWidget* flip = nullptr;
        for (auto* b : menu->findChildren<MenuButtonWidget*>()) if (b->actionName() == "actionFlipBoard") flip = b;
        QVERIFY(flip);
        for (int i = 0; i < tabs->count(); ++i) if (tabs->tabText(i).contains(QStringLiteral("表示"))) tabs->setCurrentIndex(i);
        auto* button = flip->findChild<QPushButton*>(); QVERIFY(button);
        const auto before = board()->flipMode();
        QTest::mouseClick(button, Qt::LeftButton); QCOMPARE(board()->flipMode(), !before);
        snapshot("menu-dock");
    }
    void menuPresentation()
    {
        auto* menu = window->findChild<MenuWindow*>(); QVERIFY(menu);
        auto* dock = qobject_cast<QDockWidget*>(menu->parentWidget()); QVERIFY(dock);
        dock->show();
        dock->raise();
        auto* tabs = menu->findChild<QTabWidget*>(); QVERIFY(tabs);
        int editTab = -1;
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i) == QStringLiteral("編集")) editTab = i;
        QVERIFY(editTab >= 0);
        tabs->setCurrentIndex(editTab);
        auto* area = qobject_cast<QScrollArea*>(tabs->currentWidget()); QVERIFY(area);
        QTest::qWait(100);
        snapshot("menu-presentation");
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/menu-panel.png")));
        auto* search = menu->findChild<QLineEdit*>("menuSearch"); QVERIFY(search);
        search->setText(QStringLiteral("USI"));
        QTest::qWait(50);
        int visible = 0;
        for (auto* card : area->findChildren<MenuButtonWidget*>()) {
            if (!card->isVisible()) continue;
            ++visible;
            auto* label = card->findChild<QLabel*>("menuActionText"); QVERIFY(label);
            QVERIFY(label->text().contains(QStringLiteral("USI")));
            QVERIFY(!label->text().contains(QStringLiteral("...")));
            QVERIFY(label->height() >= label->heightForWidth(label->width()));
        }
        QCOMPARE(visible, 2);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/menu-search.png")));
        search->clear();
        dock->setFloating(true);
        dock->resize(350, 600);
        QTest::qWait(100);
        for (auto* card : area->findChildren<MenuButtonWidget*>()) {
            if (card->isHidden()) continue;
            const auto topLeft = card->mapTo(area->viewport(), QPoint(0, 0));
            QVERIFY(topLeft.x() >= 0);
            QVERIFY(topLeft.x() + card->width() <= area->viewport()->width());
        }
        QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
        QVERIFY(area->verticalScrollBar()->maximum() > 0);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/menu-narrow.png")));
        auto* customize = menu->findChild<QToolButton*>("menuCustomize"); QVERIFY(customize);
        QTest::mouseClick(customize, Qt::LeftButton);
        for (auto* card : area->findChildren<MenuButtonWidget*>()) {
            if (card->isVisible() && card->action()->isEnabled()) {
                QTest::mouseClick(card->findChild<QPushButton*>("menuAddFavorite"), Qt::LeftButton);
                break;
            }
        }
        QCOMPARE(menu->favorites().size(), 1);
        tabs->setCurrentIndex(0);
        QTest::qWait(100);
        QVERIFY(dock->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/menu-favorites.png")));
        QTest::mouseClick(customize, Qt::LeftButton);
        QCOMPARE(AppSettings::menuWindowFavorites(), menu->favorites());
    }
    void pieceStyleMenuDock()
    {
        auto* menu = window->findChild<MenuWindow*>();
        QVERIFY(menu);
        auto* dock = qobject_cast<QDockWidget*>(menu->parentWidget());
        QVERIFY(dock);
        if (!dock->toggleViewAction()->isChecked()) clickAction(dock->toggleViewAction());
        dock->raise();
        auto* tabs = menu->findChild<QTabWidget*>();
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i).contains(QStringLiteral("表示"))) tabs->setCurrentIndex(i);
        MenuButtonWidget* appearance = nullptr;
        for (auto* button : menu->findChildren<MenuButtonWidget*>()) {
            QVERIFY(!button->actionName().startsWith(QStringLiteral("actionPieceStyle")));
            QVERIFY(button->actionName() != QStringLiteral("actionBoardColors"));
            if (button->actionName() == "actionBoardAppearance") appearance = button;
        }
        QVERIFY(appearance);
        auto* button = appearance->findChild<QPushButton*>();
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog && dialog->isVisible());
        QCOMPARE(dialog->findChild<QListWidget*>("appearancePieces")->count(), 21);
    }
    void pieceStyleMenuBar()
    {
        auto* display = window->findChild<QMenu*>("Display");
        QVERIFY(display);
        QVERIFY(!window->findChild<QMenu*>("menuPieceStyle"));
        QVERIFY(!action("actionBoardColors"));
        QVERIFY(!action("actionPieceStyleStandard"));
        QTest::mouseClick(window->menuBar(), Qt::LeftButton, Qt::NoModifier,
                          window->menuBar()->actionGeometry(display->menuAction()).center());
        QTRY_VERIFY(display->isVisible());
        QTest::mouseClick(display, Qt::LeftButton, Qt::NoModifier,
                          display->actionGeometry(action("actionBoardAppearance")).center());
        auto* dialog = window->findChild<BoardColorDialog*>();
        QVERIFY(dialog && dialog->isVisible());
        QCOMPARE(dialog->windowTitle(), QStringLiteral("対局画面の外観"));
        QCOMPARE(dialog->findChild<QTabWidget*>("appearanceSections")->count(), 6);
        QVERIFY(!hasKifuPasteDialog());
    }
    void humanResignAndDeclaration()
    {
        armDialog("game"); click("actionStartGame");
        QVERIFY(action("actionNyugyokuDeclaration")->isEnabled());
        armDialog(); click("actionNyugyokuDeclaration"); QVERIFY(dialogHandled);
        QVERIFY(record()->isNavigationDisabled());
        armDialog("auto"); click("actionResign");
        QTRY_VERIFY_WITH_TIMEOUT(!record()->isNavigationDisabled(), 1500);
        QVERIFY(copy("actionCopyKIF").contains(QStringLiteral("投了")));
    }
    void engineConsiderationPosition()
    {
        sampleGame(); QTest::mouseClick(record()->lastButton(), Qt::LeftButton); QTest::qWait(80);
        const auto expected = boardSfen();
        QDockWidget* dock = nullptr;
        for (auto* d : window->findChildren<QDockWidget*>()) if (d->windowTitle() == QStringLiteral("検討")) dock = d;
        QVERIFY(dock); dock->show(); dock->raise();
        QToolButton* start = nullptr;
        for (auto* b : dock->findChildren<QToolButton*>()) if (b->text() == QStringLiteral("検討開始")) start = b;
        QVERIFY(start); QTest::mouseClick(start, Qt::LeftButton); QTest::qWait(300);
        QFile log(QString::fromLocal8Bit(qgetenv("AUDIT_USI_LOG"))); QVERIFY(log.open(QIODevice::ReadOnly));
        const auto commands = QString::fromUtf8(log.readAll());
        qInfo() << "displayed position" << expected << "commands" << commands;
        QVERIFY(commands.contains("position startpos moves 7g7f 3c3d 2g2f 8c8d") || commands.contains("position sfen " + expected));
    }
    void bookmarkDialog()
    {
        sampleGame();
        armDialog(); QTest::mouseClick(record()->bookmarkEditButton(), Qt::LeftButton);
        QVERIFY(dialogHandled);
    }
    void dockReset()
    {
        auto* dock = qobject_cast<QDockWidget*>(record()->parentWidget()); QVERIFY(dock);
        if (action("actionLockDocks")->isChecked()) click("actionLockDocks");
        dock->setFloating(true); QVERIFY(dock->isFloating());
        click("actionResetDockLayout"); QVERIFY(!dock->isFloating()); QVERIFY(dock->isVisible());
    }
    void quitAction()
    {
        QSignalSpy quit(qApp, &QCoreApplication::aboutToQuit);
        QSignalSpy triggered(action("actionQuit"), &QAction::triggered);
        QTimer trigger, watchdog;
        trigger.setSingleShot(true); watchdog.setSingleShot(true);
        connect(&trigger, &QTimer::timeout, this, &GuiAudit::quitFromMenu);
        connect(&watchdog, &QTimer::timeout, qApp, &QCoreApplication::quit);
        trigger.start(50); watchdog.start(1500);
        QElapsedTimer timer; timer.start();
        QCOMPARE(qApp->exec(), 0);
        QCOMPARE(triggered.size(), 1); QCOMPARE(quit.size(), 1); QVERIFY(timer.elapsed() < 1000);
    }
    void engineCancelAnalysis()
    {
        sampleGame(); armDialog("startAnalysis"); click("actionAnalyzeKifu");
        QTRY_VERIFY_WITH_TIMEOUT(action("actionCancelAnalyzeKifu")->isEnabled(), 1500);
        auto* progress = window->findChild<QProgressBar*>("analysisProgressBar");
        QVERIFY(progress);
        QVERIFY(progress->value() < progress->maximum());
        snapshot("analysis-in-progress");
        armDialog("auto"); click("actionCancelAnalyzeKifu");
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 2500);
        auto* status = window->findChild<QLabel*>("analysisStatusLabel");
        QVERIFY(status && status->text().contains(QStringLiteral("解析中止")));
    }
    void engineStopMate()
    {
        AnalysisSettings::setTsumeSearchUnlimitedTime(true);
        sampleGame(); armDialog("startAnalysis"); click("actionTsumeShogiSearch");
        QTRY_VERIFY_WITH_TIMEOUT(action("actionStopTsumeSearch")->isEnabled(), 1500);
        const auto searchStarted = [] {
            QFile commands(qEnvironmentVariable("AUDIT_USI_LOG"));
            return commands.open(QIODevice::ReadOnly)
                && commands.readAll().contains(" go mate infinite\n");
        };
        QTRY_VERIFY_WITH_TIMEOUT(searchStarted(), 1500);
        armDialog("auto"); click("actionStopTsumeSearch");
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionStopTsumeSearch")->isEnabled(), 2500);
    }
    void engineImmediateMove()
    {
        armDialog("gameEngineWhite"); click("actionStartGame");
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 7));
        QTest::mouseClick(board(), Qt::LeftButton, Qt::NoModifier, squarePoint(7, 6));
        QTRY_VERIFY_WITH_TIMEOUT(action("actionMakeImmediateMove")->isEnabled(), 1000);
        click("actionMakeImmediateMove");
        QTRY_COMPARE_WITH_TIMEOUT(record()->kifuView()->model()->rowCount(), 3, 3000);
        armDialog("auto"); click("actionBreakOffGame");
    }
};

int main(int argc, char** argv)
{
    // 単体で起動した場合も通常のアプリ設定を読み書きしない。
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", (settingsDir.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (settingsDir.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (settingsDir.path() + "/cache").toUtf8());
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setApplicationName("ShogiBoardQ-GuiAudit");
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");
    ApplicationFonts::initialize();
    GuiAudit audit;
    return QTest::qExec(&audit, argc, argv);
}

#include "tst_gui_functional.moc"
