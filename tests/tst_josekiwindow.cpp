/// @file tst_josekiwindow.cpp
/// @brief 定跡ウィンドウの設定永続化テスト

#include <QTest>
#include <QSettings>
#include <QApplication>
#include <QHeaderView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QPushButton>
#include <QCheckBox>
#include <QtWidgets>
#include <QFutureWatcher>
#include <QSignalSpy>
#include <memory>

#include "settingscommon.h"
#include "josekisettings.h"
// private メンバへは JosekiWindow の friend 宣言（SHOGIBOARDQ_TESTING 時のみ有効）経由でアクセスする
#include "josekiwindow.h"
#include "josekirepository.h"
#include "josekipresenter.h"
#include "josekimergedialog.h"
#include "josekimovedialog.h"
#include "josekimoveinputwidget.h"
#include "sfenpositiontracer.h"

class TestJosekiWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // SettingsService 永続化テスト
    void testFontSizePersistence();
    void testAutoLoadPersistence();
    void testDisplayEnabledPersistence();
    void testSfenDetailVisiblePersistence();
    void testLastFilePathClearOnHistoryClear();

    // JosekiWindow フォント適用テスト
    void testApplyFontSizeToWindow();
    void testApplyFontSizeToTableHeader();
    void testFontSizePreservedAfterHideShow();

    // setCurrentSfen テスト
    void testSetCurrentSfenWithInitialPosition();
    void testSetCurrentSfenWithStartpos();
    void testSetCurrentSfenWithEmpty();
    void testSetCurrentSfenWithMidgamePosition();
    void testSetCurrentSfenShowsPositionSummary();
    void testBusyBlocksEditsAndMerge();
    void testFileSwitchClosesMerge_data();
    void testFileSwitchClosesMerge();
    void testMergeSaveFailureAndRetry();
    void testMergeRegisterAllSkipsRegistered();
    void testDirtyStatusSurvivesRefresh();
    void testConfirmClose_data();
    void testConfirmClose();
    void testConfirmCloseSavesNewFile();
    void testConfirmCloseWhileSaving();
    void testMoveValidation_data();
    void testMoveValidation();
    void testReplyValidationAndPreview();
    void testEditReplyAndLargeFrequency();
    void testColumnWidthsSurviveRefresh();
    void testEmptyAndStoppedDisplay();
    void testDialogCancelPersistsSize();
    void testAddKeepsTargetPosition();

private:
    static QString initialSfen();
    bool seedWindow(JosekiWindow &window, const QString &path);
    void respondToMessageBox();
    void chooseSaveFile();
    void acceptMoveEdit();
    void acceptMoveAfterPositionChange();
    QMessageBox::ButtonRole m_responseRole = QMessageBox::AcceptRole;
    bool m_messageSeen = false;
    QString m_newSavePath;
    QTemporaryDir m_tempDir;
    QString m_originalSettingsPath;
};

void TestJosekiWindow::initTestCase()
{
    // テスト用の一時設定ファイルを使用
    // SettingsServiceは settingsFilePath() でキャッシュするため、
    // テスト前に既存設定をバックアップし、テスト後に復元する
    m_originalSettingsPath = SettingsCommon::settingsFilePath();
    // テスト用に初期値をクリア
    QSettings s(m_originalSettingsPath, QSettings::IniFormat);
    s.remove("JosekiWindow");
}

void TestJosekiWindow::cleanupTestCase()
{
    // テスト後にクリーンアップ
    QSettings s(m_originalSettingsPath, QSettings::IniFormat);
    s.remove("JosekiWindow");
}

void TestJosekiWindow::testFontSizePersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 10);

    // 保存して読み出し
    JosekiSettings::setJosekiWindowFontSize(16);
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 16);

    // 別の値に変更
    JosekiSettings::setJosekiWindowFontSize(8);
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 8);

    // クリーンアップ
    JosekiSettings::setJosekiWindowFontSize(10);
}

void TestJosekiWindow::testAutoLoadPersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);

    // false に変更して読み出し
    JosekiSettings::setJosekiWindowAutoLoadEnabled(false);
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), false);

    // true に戻す
    JosekiSettings::setJosekiWindowAutoLoadEnabled(true);
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);
}

void TestJosekiWindow::testDisplayEnabledPersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), true);

    // false に変更して読み出し
    JosekiSettings::setJosekiWindowDisplayEnabled(false);
    QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), false);

    // true に戻す
    JosekiSettings::setJosekiWindowDisplayEnabled(true);
}

void TestJosekiWindow::testSfenDetailVisiblePersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), false);

    // true に変更して読み出し
    JosekiSettings::setJosekiWindowSfenDetailVisible(true);
    QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), true);

    // false に戻す
    JosekiSettings::setJosekiWindowSfenDetailVisible(false);
}

void TestJosekiWindow::testLastFilePathClearOnHistoryClear()
{
    // パスを保存
    JosekiSettings::setJosekiWindowLastFilePath("/tmp/test.db");
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), QString("/tmp/test.db"));

    // クリア
    JosekiSettings::setJosekiWindowLastFilePath(QString());
    QVERIFY(JosekiSettings::josekiWindowLastFilePath().isEmpty());
}

void TestJosekiWindow::testApplyFontSizeToWindow()
{
    // フォントサイズ 14 を設定しておく
    JosekiSettings::setJosekiWindowFontSize(14);

    JosekiWindow window;

    // ウィンドウのフォントサイズが 14 に設定されているか
    QCOMPARE(window.font().pointSize(), 14);

    // クリーンアップ
    JosekiSettings::setJosekiWindowFontSize(10);
}

void TestJosekiWindow::testApplyFontSizeToTableHeader()
{
    // フォントサイズ 16 を設定しておく
    JosekiSettings::setJosekiWindowFontSize(16);

    JosekiWindow window;

    // テーブルを探す
    QTableWidget *table = window.findChild<QTableWidget *>();
    QVERIFY(table != nullptr);

    // テーブルヘッダーのフォントサイズが 16 に設定されているか
    QHeaderView *header = table->horizontalHeader();
    QVERIFY(header != nullptr);
    QCOMPARE(header->font().pointSize(), 16);

    // クリーンアップ
    JosekiSettings::setJosekiWindowFontSize(10);
}

void TestJosekiWindow::testFontSizePreservedAfterHideShow()
{
    // フォントサイズ 10（デフォルト）を設定
    JosekiSettings::setJosekiWindowFontSize(10);

    JosekiWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // A+ を3回押してフォントサイズを 13 にする
    // onFontSizeIncrease() を直接呼び出す
    window.onFontSizeIncrease();
    window.onFontSizeIncrease();
    window.onFontSizeIncrease();

    // フォントサイズが 13 に変わっているか
    QCOMPARE(window.font().pointSize(), 13);

    // SettingsService にも保存されているか
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 13);

    // テーブルヘッダーのフォントサイズも 13 か
    QTableWidget *table = window.findChild<QTableWidget *>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->horizontalHeader()->font().pointSize(), 13);

    // ウィンドウを非表示にして再表示
    window.hide();
    QTest::qWait(100);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // フォントサイズが保持されているか
    QCOMPARE(window.font().pointSize(), 13);
    QCOMPARE(table->horizontalHeader()->font().pointSize(), 13);

    // === 「再起動」をシミュレーション: 新しいJosekiWindowを作成 ===
    // SettingsServiceには 13 が保存済み
    {
        JosekiWindow window2;
        window2.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window2));

        // 再起動後もフォントサイズが 13 か
        QCOMPARE(window2.font().pointSize(), 13);

        // テーブルヘッダーのフォントサイズも 13 か
        QTableWidget *table2 = window2.findChild<QTableWidget *>();
        QVERIFY(table2 != nullptr);
        QCOMPARE(table2->horizontalHeader()->font().pointSize(), 13);
    }

    // クリーンアップ
    JosekiSettings::setJosekiWindowFontSize(10);
}

// ============================================================
// setCurrentSfen テスト
// ============================================================

void TestJosekiWindow::testSetCurrentSfenWithInitialPosition()
{
    JosekiWindow window;
    const QString initialSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    window.setCurrentSfen(initialSfen);

    // 初期局面は「初期配置 (先手番)」と表示される
    QLabel *summaryLabel = nullptr;
    const auto labels = window.findChildren<QLabel *>();
    for (auto *label : labels) {
        if (label->text().contains(tr("先手")) || label->text().contains(tr("後手"))
            || label->text().contains(tr("(未設定)"))) {
            summaryLabel = label;
            break;
        }
    }
    qWarning() << "[TEST] testSetCurrentSfenWithInitialPosition: summaryLabel="
               << (summaryLabel ? summaryLabel->text() : QStringLiteral("(not found)"));
    QVERIFY(summaryLabel != nullptr);
    QVERIFY(!summaryLabel->text().contains(tr("(未設定)")));
    QVERIFY(summaryLabel->text().contains(tr("初期配置")));
}

void TestJosekiWindow::testSetCurrentSfenWithStartpos()
{
    JosekiWindow window;
    window.setCurrentSfen(QStringLiteral("startpos"));

    // "startpos" は内部で平手初期局面SFENに変換されるので「初期配置 (先手番)」
    QLabel *summaryLabel = nullptr;
    const auto labels = window.findChildren<QLabel *>();
    for (auto *label : labels) {
        if (label->text().contains(tr("先手")) || label->text().contains(tr("後手"))
            || label->text().contains(tr("(未設定)"))) {
            summaryLabel = label;
            break;
        }
    }
    qWarning() << "[TEST] testSetCurrentSfenWithStartpos: summaryLabel="
               << (summaryLabel ? summaryLabel->text() : QStringLiteral("(not found)"));
    QVERIFY(summaryLabel != nullptr);
    QVERIFY(!summaryLabel->text().contains(tr("(未設定)")));
}

void TestJosekiWindow::testSetCurrentSfenWithEmpty()
{
    JosekiWindow window;
    window.setCurrentSfen(QString());

    // 空SFENの場合は「(未設定)」が表示される
    QLabel *summaryLabel = nullptr;
    const auto labels = window.findChildren<QLabel *>();
    for (auto *label : labels) {
        if (label->text().contains(tr("(未設定)"))) {
            summaryLabel = label;
            break;
        }
    }
    qWarning() << "[TEST] testSetCurrentSfenWithEmpty: summaryLabel="
               << (summaryLabel ? summaryLabel->text() : QStringLiteral("(not found)"));
    QVERIFY(summaryLabel != nullptr);
    QCOMPARE(summaryLabel->text(), tr("(未設定)"));
}

void TestJosekiWindow::testSetCurrentSfenWithMidgamePosition()
{
    JosekiWindow window;
    // 3手目先手番の局面
    const QString sfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");

    window.setCurrentSfen(sfen);

    QLabel *summaryLabel = nullptr;
    const auto labels = window.findChildren<QLabel *>();
    for (auto *label : labels) {
        if (label->text().contains(tr("手目"))) {
            summaryLabel = label;
            break;
        }
    }
    qWarning() << "[TEST] testSetCurrentSfenWithMidgamePosition: summaryLabel="
               << (summaryLabel ? summaryLabel->text() : QStringLiteral("(not found)"));
    QVERIFY(summaryLabel != nullptr);
    QVERIFY(summaryLabel->text().contains(QStringLiteral("3")));
    QVERIFY(summaryLabel->text().contains(tr("先手")));
}

void TestJosekiWindow::testSetCurrentSfenShowsPositionSummary()
{
    JosekiWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // 初期状態は「(未設定)」
    QLabel *summaryLabel = nullptr;
    const auto labels = window.findChildren<QLabel *>();
    for (auto *label : labels) {
        if (label->text() == tr("(未設定)")) {
            summaryLabel = label;
            break;
        }
    }
    qWarning() << "[TEST] testSetCurrentSfenShowsPositionSummary: initial summaryLabel="
               << (summaryLabel ? summaryLabel->text() : QStringLiteral("(not found)"));
    QVERIFY2(summaryLabel != nullptr, "Expected (未設定) label on initial state");

    // 平手初期局面SFENを設定
    const QString initialSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    window.setCurrentSfen(initialSfen);

    // サマリーが更新されているか
    qWarning() << "[TEST] testSetCurrentSfenShowsPositionSummary: after setCurrentSfen, summaryLabel="
               << summaryLabel->text();
    QVERIFY(!summaryLabel->text().contains(tr("(未設定)")));
    // 初期局面は「初期配置 (先手番)」
    QVERIFY(summaryLabel->text().contains(tr("先手")));
}

QString TestJosekiWindow::initialSfen()
{
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

bool TestJosekiWindow::seedWindow(JosekiWindow &window, const QString &path)
{
    window.m_pendingAutoLoad = false;
    const QString key = JosekiPresenter::normalizeSfen(initialSfen());
    window.m_repository->addMove(key, {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    window.m_repository->ensureSfenWithPly(key, initialSfen());
    if (!window.saveToFile(path)) return false;
    window.applySavedFilePath(path);
    window.setCurrentSfen(initialSfen());
    return true;
}

void TestJosekiWindow::respondToMessageBox()
{
    auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box) return;
    m_messageSeen = true;
    for (QAbstractButton *button : box->buttons()) {
        if (box->buttonRole(button) != m_responseRole) continue;
        if (!m_newSavePath.isEmpty()) QTimer::singleShot(0, this, &TestJosekiWindow::chooseSaveFile);
        button->click();
        return;
    }
}

void TestJosekiWindow::chooseSaveFile()
{
    auto *dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
    if (!dialog) return;
    dialog->selectFile(m_newSavePath);
    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
}

void TestJosekiWindow::testBusyBlocksEditsAndMerge()
{
    JosekiSettings::setJosekiWindowAutoLoadEnabled(false);
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("busy.db"));
    QVERIFY(seedWindow(window, path));
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("2g2f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    QVERIFY(merge);
    QSignalSpy registrations(&window, &JosekiWindow::mergeRegistrationFinished);
    window.setModified(true);
    window.saveToFileAsync(path);
    QVERIFY(window.isIoBusy());
    window.setCurrentSfen(initialSfen()); // 保存中に局面更新で行が作り直される場合も禁止
    QVERIFY(!window.m_tableWidget->cellWidget(0, 4)->isEnabled());
    QVERIFY(!window.m_tableWidget->cellWidget(0, 5)->isEnabled());
    QVERIFY(!window.m_actionEdit->isEnabled());
    QVERIFY(!window.m_actionDelete->isEnabled());
    QVERIFY(!merge->isEnabled());
    window.setModified(true);
    QVERIFY(!window.m_saveButton->isEnabled());
    window.editMoveAt(0);
    window.deleteMoveAt(0);
    window.onAddMoveButtonClicked();
    window.onMergeRegisterMove(JosekiPresenter::normalizeSfen(initialSfen()), initialSfen(), QStringLiteral("2g2f"));
    QCOMPARE(registrations.count(), 0);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(window.m_tableWidget->isEnabled());
    QVERIFY(merge->isEnabled());
    QVERIFY(!window.m_modified);
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    const auto moves = saved.josekiData.value(JosekiPresenter::normalizeSfen(initialSfen()));
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().value, 10);
}

void TestJosekiWindow::testFileSwitchClosesMerge_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("new") << QStringLiteral("new");
    QTest::newRow("load") << QStringLiteral("load");
    QTest::newRow("save-as") << QStringLiteral("save-as");
}

void TestJosekiWindow::testFileSwitchClosesMerge()
{
    QFETCH(QString, operation);
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("source.db"));
    const QString target = m_tempDir.filePath(QStringLiteral("target.db"));
    QVERIFY(seedWindow(window, path));
    QVERIFY(window.m_repository->saveToFile(target));
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("2g2f")}, {}, 1);
    QPointer<JosekiMergeDialog> merge = window.findChild<JosekiMergeDialog*>();
    QVERIFY(merge && merge->isVisible());
    if (operation == QStringLiteral("new")) window.onNewButtonClicked();
    else if (operation == QStringLiteral("load")) window.loadAndApplyFileAsync(target);
    else window.saveToFileAsync(target);
    QVERIFY(!merge || !merge->isVisible());
    if (merge) {
        // 遅延破棄待ちの古いダイアログからも登録できないこと
        merge->registerMove(JosekiPresenter::normalizeSfen(initialSfen()), initialSfen(), QStringLiteral("2g2f"));
    }
    QTRY_VERIFY(!window.isIoBusy());
    const auto saved = JosekiRepository::parseFromFile(target);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(JosekiPresenter::normalizeSfen(initialSfen())).size(), 1);
}

void TestJosekiWindow::testMergeSaveFailureAndRetry()
{
    JosekiWindow window;
    const QString directory = m_tempDir.filePath(QStringLiteral("merge-retry"));
    QVERIFY(QDir().mkpath(directory));
    const QString path = directory + QStringLiteral("/book.db");
    QVERIFY(seedWindow(window, path));
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().rmdir(directory));
    window.setModified(true);
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("7g7f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();
    QSignalSpy results(&window, &JosekiWindow::mergeRegistrationFinished);
    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(m_messageSeen);
    QCOMPARE(results.count(), 1);
    QCOMPARE(results.last().last().toBool(), false);
    QVERIFY(table->item(0, 3)->text().isEmpty());
    QVERIFY(table->cellWidget(0, 2)->isEnabled());
    QVERIFY(window.m_modified);
    QCOMPARE(window.m_currentMoves.first().frequency, 1);
    QVERIFY(QDir().mkpath(directory));
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QCOMPARE(results.last().last().toBool(), true);
    QVERIFY(!table->cellWidget(0, 2)->isEnabled());
    QVERIFY(!window.m_modified);
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(JosekiPresenter::normalizeSfen(initialSfen())).first().frequency, 2);
}

void TestJosekiWindow::testMergeRegisterAllSkipsRegistered()
{
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("all.db"))));
    window.setKifuDataForMerge({initialSfen(), initialSfen(), initialSfen()},
                              {QStringLiteral("7g7f"), QStringLiteral("2g2f"), QStringLiteral("7g7f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();
    QSignalSpy results(&window, &JosekiWindow::mergeRegistrationFinished);
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(!table->cellWidget(2, 2)->isEnabled());
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QCOMPARE(results.count(), 2);
    QCOMPARE(window.m_currentMoves.size(), 2);
    QCOMPARE(window.m_currentMoves.first().frequency, 2);
    QCOMPARE(window.m_currentMoves.last().frequency, 1);
}

void TestJosekiWindow::testDirtyStatusSurvivesRefresh()
{
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("dirty.db"))));
    window.setModified(true);
    window.updateJosekiDisplay();
    QCOMPARE(window.m_fileStatusLabel->text(), QStringLiteral("未保存"));
    window.m_repository->clear();
    window.updateJosekiDisplay();
    QCOMPARE(window.m_fileStatusLabel->text(), QStringLiteral("未保存"));
    window.setModified(false);
    QVERIFY(window.m_fileStatusLabel->text().isEmpty());
}

void TestJosekiWindow::testConfirmClose_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<bool>("accepted");
    QTest::addColumn<bool>("saved");
    QTest::newRow("save") << int(QMessageBox::AcceptRole) << true << true;
    QTest::newRow("discard") << int(QMessageBox::DestructiveRole) << true << false;
    QTest::newRow("cancel") << int(QMessageBox::RejectRole) << false << false;
}

void TestJosekiWindow::testConfirmClose()
{
    QFETCH(int, role);
    QFETCH(bool, accepted);
    QFETCH(bool, saved);
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("close.db"));
    QVERIFY(seedWindow(window, path));
    const QString key = JosekiPresenter::normalizeSfen(initialSfen());
    window.m_presenter->editMove(key, QStringLiteral("7g7f"), 99, 32, 1, {});
    m_responseRole = static_cast<QMessageBox::ButtonRole>(role);
    m_messageSeen = false;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QCOMPARE(window.confirmClose(), accepted);
    QVERIFY(m_messageSeen);
    const auto disk = JosekiRepository::parseFromFile(path);
    QVERIFY(disk.success);
    QCOMPARE(disk.josekiData.value(key).first().value, saved ? 99 : 10);
    QCOMPARE(window.m_modified, !saved);
}

void TestJosekiWindow::testConfirmCloseSavesNewFile()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    JosekiWindow window;
    window.m_presenter->addMove(JosekiPresenter::normalizeSfen(initialSfen()), initialSfen(),
                                {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    QVERIFY(window.m_saveButton->isEnabled());
    m_newSavePath = m_tempDir.filePath(QStringLiteral("close-new.db"));
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(window.confirmClose());
    QVERIFY(!window.isIoBusy());
    QVERIFY(!window.m_modified);
    QVERIFY(JosekiRepository::parseFromFile(m_newSavePath).success);
    m_newSavePath.clear();
}

void TestJosekiWindow::testConfirmCloseWhileSaving()
{
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("close-busy.db"));
    QVERIFY(seedWindow(window, path));
    window.saveToFileAsync(path);
    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(!window.confirmClose());
    QVERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(window.confirmClose());
}

void TestJosekiWindow::testMoveValidation_data()
{
    QTest::addColumn<QString>("sfen");
    QTest::addColumn<QString>("usi");
    QTest::addColumn<bool>("valid");
    QTest::newRow("pawn") << initialSfen() << QStringLiteral("7g7f") << true;
    QTest::newRow("same-square") << initialSfen() << QStringLiteral("7g7g") << false;
    QTest::newRow("opponent-piece") << initialSfen() << QStringLiteral("3c3d") << false;
    QTest::newRow("empty-square") << initialSfen() << QStringLiteral("7f7e") << false;
    QTest::newRow("pawn-jump") << initialSfen() << QStringLiteral("7g7e") << false;
    QTest::newRow("promotion-outside-zone") << initialSfen() << QStringLiteral("7g7f+") << false;
    QTest::newRow("drop-without-hand") << initialSfen() << QStringLiteral("P*5e") << false;
    const QString drop = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b P 1");
    QTest::newRow("drop") << drop << QStringLiteral("P*5e") << true;
    QTest::newRow("dead-drop") << drop << QStringLiteral("P*4a") << false;
    QTest::newRow("double-pawn") << QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1")
                                     << QStringLiteral("P*5e") << false;
    QTest::newRow("promote") << QStringLiteral("4k4/9/P8/9/9/9/9/9/4K4 b - 1")
                              << QStringLiteral("9c9b+") << true;
    QTest::newRow("must-promote") << QStringLiteral("4k4/P8/9/9/9/9/9/9/4K4 b - 1")
                                   << QStringLiteral("9b9a") << false;
    QTest::newRow("self-check") << QStringLiteral("4r3k/9/9/9/9/9/9/4G4/4K4 b - 1")
                                 << QStringLiteral("5h4h") << false;
    QTest::newRow("white") << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")
                            << QStringLiteral("3c3d") << true;
}

void TestJosekiWindow::testMoveValidation()
{
    QFETCH(QString, sfen);
    QFETCH(QString, usi);
    QFETCH(bool, valid);
    JosekiMoveDialog dialog;
    dialog.setCurrentSfen(sfen);
    dialog.setMove(usi);
    auto *buttons = dialog.findChild<QDialogButtonBox*>();
    QVERIFY(buttons);
    QCOMPARE(buttons->button(QDialogButtonBox::Ok)->isEnabled(), valid);
    dialog.accept(); // Enter や直接 accept() でも不正な手を登録できない
    QCOMPARE(dialog.result(), valid ? int(QDialog::Accepted) : int(QDialog::Rejected));
}

void TestJosekiWindow::testReplyValidationAndPreview()
{
    JosekiMoveDialog dialog;
    dialog.setCurrentSfen(initialSfen());
    dialog.setMove(QStringLiteral("7g7f"));
    auto *buttons = dialog.findChild<QDialogButtonBox*>();
    auto *input = dialog.findChild<JosekiMoveInputWidget*>(QStringLiteral("moveInput"));
    auto *reply = dialog.findChild<JosekiMoveInputWidget*>(QStringLiteral("nextMoveInput"));
    QVERIFY(input && reply && buttons);
    QCOMPARE(input->findChild<QLabel*>(QStringLiteral("movePreview"))->text(), QStringLiteral("▲７六歩(77)"));
    dialog.setNextMove(QStringLiteral("8c8d"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QCOMPARE(reply->findChild<QLabel*>(QStringLiteral("movePreview"))->text(), QStringLiteral("△８四歩(83)"));
    dialog.setNextMove(QStringLiteral("7f7e"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.setNextMove(QStringLiteral("8c8c"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.setNextMove(QStringLiteral("none"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    dialog.setMove(QStringLiteral("7g7e"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QVERIFY(!reply->isEnabled());
    dialog.setMove(QStringLiteral("7g7f"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QVERIFY(reply->isEnabled());
    dialog.setNextMove(QStringLiteral("8c8d"));
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    const QString shots = qEnvironmentVariable("JOSEKI_SCREENSHOT_DIR");
    if (!shots.isEmpty()) QVERIFY(dialog.grab().save(shots + QStringLiteral("/joseki-add.png")));
}

void TestJosekiWindow::acceptMoveEdit()
{
    auto *dialog = qobject_cast<JosekiMoveDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    QCOMPARE(dialog->move(), QStringLiteral("7g7f"));
    QCOMPARE(dialog->frequency(), 2000000);
    auto *reply = dialog->findChild<JosekiMoveInputWidget*>(QStringLiteral("nextMoveInput"));
    QVERIFY(reply && reply->isVisible());
    QCOMPARE(dialog->nextMove(), QStringLiteral("8c8d"));
    dialog->setNextMove(QStringLiteral("3c3d"));
    dialog->accept();
}

void TestJosekiWindow::testEditReplyAndLargeFrequency()
{
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("edit-reply.db"));
    QVERIFY(seedWindow(window, path));
    const QString key = JosekiPresenter::normalizeSfen(initialSfen());
    window.m_presenter->editMove(key, QStringLiteral("7g7f"), 10, 32, 2000000, {}, QStringLiteral("8c8d"));
    window.updateJosekiDisplay();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptMoveEdit);
    window.editMoveAt(0);
    QCOMPARE(window.m_currentMoves.first().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(window.m_currentMoves.first().frequency, 2000000);
    QVERIFY(window.saveToFile(path));
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(key).first().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(saved.josekiData.value(key).first().frequency, 2000000);
}

void TestJosekiWindow::testColumnWidthsSurviveRefresh()
{
    JosekiSettings::setJosekiWindowAutoLoadEnabled(false);
    {
        JosekiWindow window;
        QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("widths.db"))));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.m_tableWidget->setColumnWidth(2, 280);
        window.m_tableWidget->setColumnWidth(3, 270);
        window.setCurrentSfen(initialSfen());
        QCOMPARE(window.m_tableWidget->columnWidth(2), 280);
        window.hide();
        window.show();
        QCOMPARE(window.m_tableWidget->columnWidth(3), 270);
        window.saveSettings();
        const QString shots = qEnvironmentVariable("JOSEKI_SCREENSHOT_DIR");
        if (!shots.isEmpty()) {
            window.resize(1150, 600);
            QTest::qWait(30);
            QVERIFY(window.grab().save(shots + QStringLiteral("/joseki-window.png")));
        }
    }
    JosekiWindow restored;
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored));
    QCOMPARE(restored.m_tableWidget->columnWidth(2), 280);
    QCOMPARE(restored.m_tableWidget->columnWidth(3), 270);
}

void TestJosekiWindow::testEmptyAndStoppedDisplay()
{
    JosekiWindow window;
    window.setCurrentSfen(initialSfen());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.m_emptyGuideLabel->isVisible());
    window.resize(1100, 700);
    QCoreApplication::processEvents();
    QVERIFY(window.m_tableWidget->viewport()->rect().contains(window.m_emptyGuideLabel->geometry()));
    window.m_stopButton->setChecked(true);
    window.onStopButtonClicked();
    QVERIFY(window.m_stopButton->isVisible()); // 空の定跡でも再開できる
    QVERIFY(window.m_emptyGuideLabel->text().contains(QStringLiteral("停止")));
    window.setCurrentSfen(QString());
    QCOMPARE(window.m_positionSummaryLabel->text(), QStringLiteral("(未設定)"));
    QVERIFY(window.m_sfenLineLabel->text().isEmpty());
    QVERIFY(!window.m_addMoveButton->isEnabled());
    window.m_stopButton->setChecked(false);
    window.onStopButtonClicked();
}

void TestJosekiWindow::testDialogCancelPersistsSize()
{
    JosekiMoveDialog dialog;
    dialog.resize(780, 650);
    dialog.reject();
    QCOMPARE(JosekiSettings::josekiMoveDialogSize(), dialog.size());
    JosekiMoveDialog restored;
    QCOMPARE(restored.size(), dialog.size());
}

void TestJosekiWindow::acceptMoveAfterPositionChange()
{
    auto *dialog = qobject_cast<JosekiMoveDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    auto *window = qobject_cast<JosekiWindow*>(dialog->parentWidget());
    QVERIFY(window);
    SfenPositionTracer tracer;
    QVERIFY(tracer.applyUsiMove(QStringLiteral("2g2f")));
    window->setCurrentSfen(tracer.toSfenString());
    dialog->setMove(QStringLiteral("7g7f"));
    dialog->accept();
}

void TestJosekiWindow::testAddKeepsTargetPosition()
{
    JosekiWindow window;
    window.setCurrentSfen(initialSfen());
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptMoveAfterPositionChange);
    window.onAddMoveButtonClicked();
    const auto moves = window.m_repository->movesForPosition(JosekiPresenter::normalizeSfen(initialSfen()));
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().move, QStringLiteral("7g7f"));
    QVERIFY(!window.m_repository->containsPosition(JosekiPresenter::normalizeSfen(window.m_currentSfen)));
}

QTEST_MAIN(TestJosekiWindow)
#include "tst_josekiwindow.moc"
