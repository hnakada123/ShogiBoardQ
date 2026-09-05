#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTableWidget>
#include <QTranslator>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "recordpane.h"
#include "settingscommon.h"
#include "appsettings.h"
#include "engineanalysistab.h"
#include "sfencollectiondialog.h"
#include "kifupastedialog.h"
#include "josekiwindow.h"
#include "menuwindow.h"
#include "menubuttonwidget.h"
#include "enginelistsettings.h"
#include "commenteditorpanel.h"
#include "considerationtabmanager.h"

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
    void armDialog(const QString& mode = "close", const QString& path = {})
    {
        dialogMode = mode; dialogPath = path;
        dialogHandled = false; dialogClass.clear(); dialogTitle.clear();
        dialogTimer.start(30);
    }
    void paste(const QString& text)
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
        QCoreApplication::processEvents();
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
        if (window) window->close();
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
    void pieceStyles()
    {
        const auto standardPawn = board()->piece('P').pixmap(90).toImage();
        ShogiView secondary;
        secondary.setPieces();
        QVERIFY(action("actionPieceStyleStandard")->isChecked());
        snapshot("pieces-standard");

        click("actionPieceStyleClear");
        QVERIFY(action("actionPieceStyleClear")->isChecked());
        QVERIFY(!action("actionPieceStyleStandard")->isChecked());
        QVERIFY2(!hasKifuPasteDialog(),
                 "Changing piece style must not open the kifu paste dialog");
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("clear"));
        const auto clearPawn = board()->piece('P').pixmap(90).toImage();
        QVERIFY(clearPawn != standardPawn);
        QCOMPARE(secondary.piece('P').pixmap(90).toImage(), clearPawn);
        QCOMPARE(boardSfen(), initial);
        snapshot("pieces-clear");

        click("actionFlipBoard");
        QVERIFY(board()->flipMode());
        QCOMPARE(board()->piece('K').pixmap(90).toImage(),
                 QIcon(":/pieces/clear/Gote_ou45.svg").pixmap(90).toImage());
        click("actionPieceStyleStandard");
        QVERIFY(board()->flipMode());
        QCOMPARE(board()->piece('K').pixmap(90).toImage(),
                 QIcon(":/pieces/Gote_ou45.svg").pixmap(90).toImage());
        QCOMPARE(secondary.piece('P').pixmap(90).toImage(), standardPawn);
        click("actionPieceStyleClear");
        snapshot("pieces-clear-flipped");
        click("actionFlipBoard");
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), clearPawn);

        ShogiView createdAfterChange;
        QCOMPARE(createdAfterChange.piece('P').pixmap(90).toImage(), clearPawn);
        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(action("actionPieceStyleClear")->isChecked());
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), clearPawn);

        // 成駒・持ち駒・駒打ち矢印を表示してキャッシュを作り、切替後の描画を比較する。
        board()->board()->setSfen(QStringLiteral(
            "4k4/9/3+r+b+s+n+l+p/9/9/9/+P+L+N+S+B+R3/9/4K4 b 2GSNL8P2gsnl8p 1"));
        ShogiView::Arrow drop;
        drop.toFile = 5;
        drop.toRank = 5;
        drop.dropPiece = 'P';
        board()->setArrows({drop});
        click("actionPieceStyleStandard");
        const auto standardPosition = board()->toImage();
        click("actionPieceStyleClear");
        const auto clearPosition = board()->toImage();
        QVERIFY(clearPosition != standardPosition);
        board()->setPieces();
        QCOMPARE(board()->toImage(), clearPosition);
        snapshot("pieces-clear-promoted-and-hand");
    }
    void pieceVariants_data()
    {
        QTest::addColumn<QString>("style");
        QTest::addColumn<QString>("actionName");
        QTest::newRow("wood") << QStringLiteral("wood") << QStringLiteral("actionPieceStyleWood");
        QTest::newRow("ivory") << QStringLiteral("ivory") << QStringLiteral("actionPieceStyleIvory");
        QTest::newRow("dark") << QStringLiteral("dark") << QStringLiteral("actionPieceStyleDark");
    }
    void pieceVariants()
    {
        QFETCH(QString, style);
        QFETCH(QString, actionName);
        ShogiView secondary;
        const auto standardPawn = board()->piece('P').pixmap(90).toImage();
        click(actionName);
        QCOMPARE(AppSettings::pieceStyle(), style);
        QVERIFY(action(actionName)->isChecked());
        QVERIFY(!action("actionPieceStyleStandard")->isChecked());
        QVERIFY(!hasKifuPasteDialog());
        const QString prefix = QStringLiteral(":/pieces/%1/").arg(style);
        const auto pawn = QIcon(prefix + "Sente_fu45.svg").pixmap(90).toImage();
        QVERIFY(pawn != standardPawn);
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);
        QCOMPARE(secondary.piece('P').pixmap(90).toImage(), pawn);
        QCOMPARE(boardSfen(), initial);
        snapshot("pieces-" + style);

        click("actionFlipBoard");
        QCOMPARE(board()->piece('K').pixmap(90).toImage(),
                 QIcon(prefix + "Gote_ou45.svg").pixmap(90).toImage());
        click("actionPieceStyleClear");
        QVERIFY(!action(actionName)->isChecked());
        click(actionName);
        QVERIFY(!action("actionPieceStyleClear")->isChecked());
        QCOMPARE(board()->piece('k').pixmap(90).toImage(),
                 QIcon(prefix + "Sente_gyoku45.svg").pixmap(90).toImage());

        window->close();
        window.reset();
        window = std::make_unique<MainWindow>();
        window->show();
        QVERIFY(action(actionName)->isChecked());
        QCOMPARE(board()->piece('P').pixmap(90).toImage(), pawn);
        QVERIFY(!hasKifuPasteDialog());
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
            {"actionJishogiScore", "QDialog"},
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
        QVERIFY(board()->board()->convertStandToSfen() != "-");
        click("actionSetTsumePosition"); QVERIFY(boardSfen().contains('k'));
        click("actionSetHiratePosition"); QCOMPARE(boardSfen(), initial);
        auto* gc = window->findChild<ShogiGameController*>(); QVERIFY(gc);
        const auto before = gc->currentPlayer();
        click("actionChangeTurn");
        QVERIFY(gc->currentPlayer() != before);
        click("actionChangeTurn"); QCOMPARE(gc->currentPlayer(), before);
        click("actionEndEditPosition"); QVERIFY(!board()->positionEditMode());
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
        QVERIFY(record()->kifuView()->model()->rowCount() > 2);
        QTest::mouseClick(record()->lastButton(), Qt::LeftButton);
        const auto last = boardSfen();
        const QString path = QStringLiteral(AUDIT_DIR "/saved.kif");
        QFile::remove(path);
        armDialog("file", path); click("actionSaveAs"); QVERIFY(dialogHandled);
        QVERIFY(QFileInfo(path).size() > 50);
        click("actionSave");
        click("actionNewGame");
        armDialog("file", path); click("actionOpenKifuFile");
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
        click("actionNewGame"); armDialog("auto"); paste(bod);
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
        QCOMPARE(action("actionOpenKifuFile")->text(), QString("Open"));
        snapshot("english");
        window.reset(); qApp->removeTranslator(&translator);
    }
    void sfenCollection()
    {
        selectedCollection.clear();
        armDialog("collection"); click("actionSfenCollectionViewer");
        QVERIFY(!selectedCollection.isEmpty()); QCOMPARE(boardSfen(), selectedCollection);
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
        QVERIFY(copy("actionCopyUSIAll").contains("7g7f 3c3d"));
        armDialog("auto"); click("actionBreakOffGame");
        QTRY_VERIFY(!record()->isNavigationDisabled());
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
    void consideration()
    {
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
        QVERIFY(infoFound); snapshot("consideration");
        QTest::mouseClick(start, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討開始"), 3000);
        QCOMPARE(starts.size(), 1); QCOMPARE(stops.size(), 1);
        for (int cycle = 2; cycle <= 3; ++cycle) {
            QTest::mouseClick(start, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討中止"), 3000);
            QTest::qWait(100);
            QTest::mouseClick(start, Qt::LeftButton);
            QTRY_COMPARE_WITH_TIMEOUT(start->text(), QStringLiteral("検討開始"), 3000);
            QCOMPARE(starts.size(), cycle); QCOMPARE(stops.size(), cycle);
        }
        QFile commands(qEnvironmentVariable("AUDIT_USI_LOG"));
        QVERIFY(commands.open(QIODevice::ReadOnly));
        const auto log = commands.readAll();
        QVERIFY2(!log.contains("position sfen startpos"), log.constData());
        QCOMPARE(log.count(" position startpos\n"), 3);
        QCOMPARE(log.count(" go infinite\n"), 3);
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
        snapshot("analysis-results");
    }
    void engineMateNoMate()
    {
        sampleGame();
        armDialog("startAnalysis"); click("actionTsumeShogiSearch");
        QCOMPARE(dialogClass, QString("TsumeShogiSearchDialog"));
        armDialog("auto"); QTest::qWait(700);
        QVERIFY(!action("actionStopTsumeSearch")->isEnabled());
        QVERIFY(window->isVisible());
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
        click("actionNewGame"); paste(saved);
        QTest::mouseClick(record()->nextButton(), Qt::LeftButton);
        QTest::qWait(80);
        QVERIFY(edit->toPlainText().contains("Audit comment at ply one"));
        QVERIFY(copy("actionCopyKIF").contains("&Audit bookmark"));
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
    void pieceStyleMenuDock()
    {
        auto* menu = window->findChild<MenuWindow*>();
        QVERIFY(menu);
        auto* dock = qobject_cast<QDockWidget*>(menu->parentWidget());
        QVERIFY(dock);
        if (!dock->toggleViewAction()->isChecked()) clickAction(dock->toggleViewAction());
        dock->raise();
        auto* tabs = menu->findChild<QTabWidget*>();
        QVERIFY(tabs);
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i).contains(QStringLiteral("表示"))) tabs->setCurrentIndex(i);
        }
        MenuButtonWidget* clear = nullptr;
        for (auto* button : menu->findChildren<MenuButtonWidget*>()) {
            if (button->actionName() == "actionPieceStyleClear") clear = button;
        }
        QVERIFY(clear);
        auto* button = clear->findChild<QPushButton*>();
        QVERIFY(button);
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("clear"));
        QVERIFY2(!hasKifuPasteDialog(),
                 "The piece style button must not open the kifu paste dialog");
    }
    void pieceStyleMenuBar()
    {
        auto* display = window->findChild<QMenu*>("Display");
        auto* styles = window->findChild<QMenu*>("menuPieceStyle");
        QVERIFY(display);
        QVERIFY(styles);
        QTest::mouseClick(window->menuBar(), Qt::LeftButton, Qt::NoModifier,
                          window->menuBar()->actionGeometry(display->menuAction()).center());
        QTRY_VERIFY(display->isVisible());
        QTest::mouseMove(display, display->actionGeometry(styles->menuAction()).center());
        QTRY_VERIFY(styles->isVisible());
        QTest::mouseClick(styles, Qt::LeftButton, Qt::NoModifier,
                          styles->actionGeometry(action("actionPieceStyleClear")).center());
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("clear"));
        QVERIFY2(!hasKifuPasteDialog(),
                 "The piece style submenu must not open the kifu paste dialog");
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
        armDialog("auto"); click("actionCancelAnalyzeKifu");
        QTRY_VERIFY_WITH_TIMEOUT(!action("actionCancelAnalyzeKifu")->isEnabled(), 2500);
    }
    void engineStopMate()
    {
        sampleGame(); armDialog("startAnalysis"); click("actionTsumeShogiSearch");
        QTRY_VERIFY_WITH_TIMEOUT(action("actionStopTsumeSearch")->isEnabled(), 1500);
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
    GuiAudit audit;
    return QTest::qExec(&audit, argc, argv);
}

#include "tst_gui_functional.moc"
