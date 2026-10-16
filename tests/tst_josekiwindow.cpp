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
#include "josekibooksession.h"
#include "josekirepository.h"
#include "josekifilecontroller.h"
#include "josekitablemodel.h"
#include "josekimergedialog.h"
#include "josekimovedialog.h"
#include "josekimoveinputwidget.h"
#include "sfenreplay.h"
#include "sfenutils.h"

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
    void testMergeRegisterAllSavesAtOnce();
    void testMergeTableFitsMovesAndButtons();
    void testPositionSummaryNamesHandicapOrPly();
    void testHistoryKeepsOnlyLoadedFiles();
    void testClearHistoryStopsAutoLoad();
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
    void testSameSfenKeepsTableRows();
    void testButtonColumnsActOnRow();

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
    JosekiSettings::josekiWindowFontSize.set(16);
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 16);

    // 別の値に変更
    JosekiSettings::josekiWindowFontSize.set(8);
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 8);

    // クリーンアップ
    JosekiSettings::josekiWindowFontSize.set(10);
}

void TestJosekiWindow::testAutoLoadPersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);

    // false に変更して読み出し
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), false);

    // true に戻す
    JosekiSettings::josekiWindowAutoLoadEnabled.set(true);
    QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);
}

void TestJosekiWindow::testDisplayEnabledPersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), true);

    // false に変更して読み出し
    JosekiSettings::josekiWindowDisplayEnabled.set(false);
    QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), false);

    // true に戻す
    JosekiSettings::josekiWindowDisplayEnabled.set(true);
}

void TestJosekiWindow::testSfenDetailVisiblePersistence()
{
    // デフォルト値
    QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), false);

    // true に変更して読み出し
    JosekiSettings::josekiWindowSfenDetailVisible.set(true);
    QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), true);

    // false に戻す
    JosekiSettings::josekiWindowSfenDetailVisible.set(false);
}

void TestJosekiWindow::testLastFilePathClearOnHistoryClear()
{
    // パスを保存
    JosekiSettings::josekiWindowLastFilePath.set("/tmp/test.db");
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), QString("/tmp/test.db"));

    // クリア
    JosekiSettings::josekiWindowLastFilePath.set(QString());
    QVERIFY(JosekiSettings::josekiWindowLastFilePath().isEmpty());
}

void TestJosekiWindow::testApplyFontSizeToWindow()
{
    // フォントサイズ 14 を設定しておく
    JosekiSettings::josekiWindowFontSize.set(14);

    JosekiWindow window;

    // ウィンドウのフォントサイズが 14 に設定されているか
    QCOMPARE(window.font().pointSize(), 14);

    // クリーンアップ
    JosekiSettings::josekiWindowFontSize.set(10);
}

void TestJosekiWindow::testApplyFontSizeToTableHeader()
{
    // フォントサイズ 16 を設定しておく
    JosekiSettings::josekiWindowFontSize.set(16);

    JosekiWindow window;

    // テーブルを探す
    QTableView *table = window.findChild<QTableView *>();
    QVERIFY(table != nullptr);

    // テーブルヘッダーのフォントサイズが 16 に設定されているか
    QHeaderView *header = table->horizontalHeader();
    QVERIFY(header != nullptr);
    QCOMPARE(header->font().pointSize(), 16);

    // クリーンアップ
    JosekiSettings::josekiWindowFontSize.set(10);
}

void TestJosekiWindow::testFontSizePreservedAfterHideShow()
{
    // フォントサイズ 10（デフォルト）を設定
    JosekiSettings::josekiWindowFontSize.set(10);

    JosekiWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // A+ を3回押してフォントサイズを 13 にする
    // A+ と同じ m_fontHelper.increase() を直接呼び出す
    window.m_fontHelper.increase();
    window.m_fontHelper.increase();
    window.m_fontHelper.increase();

    // フォントサイズが 13 に変わっているか
    QCOMPARE(window.font().pointSize(), 13);

    // SettingsService にも保存されているか
    QCOMPARE(JosekiSettings::josekiWindowFontSize(), 13);

    // テーブルヘッダーのフォントサイズも 13 か
    QTableView *table = window.findChild<QTableView *>();
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
        QTableView *table2 = window2.findChild<QTableView *>();
        QVERIFY(table2 != nullptr);
        QCOMPARE(table2->horizontalHeader()->font().pointSize(), 13);
    }

    // クリーンアップ
    JosekiSettings::josekiWindowFontSize.set(10);
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
    window.m_pendingAutoLoadPath.clear();
    window.m_session->addMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    if (!window.m_session->save(path)) return false;
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
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("busy.db"));
    QVERIFY(seedWindow(window, path));
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("2g2f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    QVERIFY(merge);
    window.m_session->setModified(true);
    window.m_files->saveTo(path);
    QVERIFY(window.isIoBusy());
    window.setCurrentSfen(initialSfen()); // 保存中に局面更新で行が作り直される場合も禁止
    QVERIFY(!window.m_tableView->isEnabled());
    QVERIFY(!window.m_actionEdit->isEnabled());
    QVERIFY(!window.m_actionDelete->isEnabled());
    QVERIFY(!merge->isEnabled());
    window.m_session->setModified(true);
    QVERIFY(!window.m_saveButton->isEnabled());
    window.editMoveAt(0);
    window.deleteMoveAt(0);
    window.onAddMoveButtonClicked();
    QVERIFY(!window.registerMergeEntries({KifuMergeEntry{1, initialSfen(), QStringLiteral("2g2f"), {}, false}}));
    QVERIFY(!window.m_session->hasMove(initialSfen(), QStringLiteral("2g2f")));
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(window.m_tableView->isEnabled());
    QVERIFY(merge->isEnabled());
    QVERIFY(!window.m_session->isModified());
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    const auto moves = saved.josekiData.value(JosekiBookSession::positionKey(initialSfen()));
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
    QFile::remove(target);
    QVERIFY(QFile::copy(path, target));
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("2g2f")}, {}, 1);
    QPointer<JosekiMergeDialog> merge = window.findChild<JosekiMergeDialog*>();
    QVERIFY(merge && merge->isVisible());
    if (operation == QStringLiteral("new")) window.m_files->newBook();
    else if (operation == QStringLiteral("load")) window.m_files->openFile(target);
    else window.m_files->saveTo(target);
    QVERIFY(!merge || !merge->isVisible());
    if (merge) {
        // 遅延破棄待ちの古いダイアログからも登録できないこと
        QMetaObject::invokeMethod(merge, "onRegisterButtonClicked", Qt::DirectConnection, Q_ARG(int, 0));
    }
    QTRY_VERIFY(!window.isIoBusy());
    const auto saved = JosekiRepository::parseFromFile(target);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(JosekiBookSession::positionKey(initialSfen())).size(), 1);
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
    window.m_session->setModified(true);
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("7g7f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();
    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(m_messageSeen);
    QVERIFY(table->item(0, 3)->text().isEmpty());
    QVERIFY(table->cellWidget(0, 2)->isEnabled());
    QVERIFY(window.m_session->isModified());
    QCOMPARE(window.m_model->moves().first().frequency, 1);
    QVERIFY(QDir().mkpath(directory));
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(!table->cellWidget(0, 2)->isEnabled());
    QVERIFY(!window.m_session->isModified());
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(JosekiBookSession::positionKey(initialSfen())).first().frequency, 2);
}

void TestJosekiWindow::testMergeRegisterAllSkipsRegistered()
{
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("all.db"))));
    window.setKifuDataForMerge({initialSfen(), initialSfen(), initialSfen()},
                              {QStringLiteral("7g7f"), QStringLiteral("2g2f"), QStringLiteral("7g7f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(!table->cellWidget(2, 2)->isEnabled());
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QVERIFY(!table->cellWidget(1, 2)->isEnabled());
    QCOMPARE(window.m_model->moves().size(), 2);
    QCOMPARE(window.m_model->moves().first().frequency, 2);
    QCOMPARE(window.m_model->moves().last().frequency, 1);
}

/// 「全て登録」はまとめて保存する。保存に失敗したら1手も登録せず、再試行で全て登録できる
void TestJosekiWindow::testMergeRegisterAllSavesAtOnce()
{
    JosekiWindow window;
    const QString directory = m_tempDir.filePath(QStringLiteral("merge-all"));
    QVERIFY(QDir().mkpath(directory));
    const QString path = directory + QStringLiteral("/book.db");
    QVERIFY(seedWindow(window, path));
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().rmdir(directory));
    window.setKifuDataForMerge({initialSfen(), initialSfen()},
                              {QStringLiteral("7g7f"), QStringLiteral("2g2f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();

    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QVERIFY(m_messageSeen);
    QVERIFY(table->cellWidget(0, 2)->isEnabled());
    QVERIFY(table->cellWidget(1, 2)->isEnabled());

    QVERIFY(QDir().mkpath(directory));
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QVERIFY(!table->cellWidget(0, 2)->isEnabled());
    QVERIFY(!table->cellWidget(1, 2)->isEnabled());
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    const auto moves = saved.josekiData.value(JosekiBookSession::positionKey(initialSfen()));
    QCOMPARE(moves.size(), 2);
    QCOMPARE(moves.first().frequency, 2);
}

/// マージの表は指し手と登録ボタンが収まる幅になる（指し手が縦に折り返さない）
void TestJosekiWindow::testMergeTableFitsMovesAndButtons()
{
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("fit.db"))));
    window.setKifuDataForMerge({initialSfen()}, {QStringLiteral("7g7f")}, {QStringLiteral("▲７六歩(77)")}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    QVERIFY(merge);
    auto *table = merge->findChild<QTableWidget*>();
    QVERIFY(table);
    // 表記の一覧は [i] が i+1 手目（棋譜欄の「開始局面」行は含まない）
    QVERIFY2(table->item(0, 1)->text().contains(QStringLiteral("７六歩")), qPrintable(table->item(0, 1)->text()));
    QVERIFY(table->columnWidth(1) >= table->fontMetrics().horizontalAdvance(table->item(0, 1)->text()));
    QVERIFY(table->columnWidth(2) >= table->cellWidget(0, 2)->sizeHint().width());
    QVERIFY(table->rowHeight(0) < 2 * table->fontMetrics().height() + 20);
}

/// 1手目の局面は平手・駒落ちなら手合の名前、それ以外は「1手目」と表示する
void TestJosekiWindow::testPositionSummaryNamesHandicapOrPly()
{
    JosekiWindow window;
    window.setCurrentSfen(initialSfen());
    QVERIFY(window.m_positionSummaryLabel->text().startsWith(QStringLiteral("初期配置")));
    window.setCurrentSfen(QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
    QVERIFY2(window.m_positionSummaryLabel->text().startsWith(QStringLiteral("香落ち")),
             qPrintable(window.m_positionSummaryLabel->text()));
    window.setCurrentSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1"));
    QVERIFY2(window.m_positionSummaryLabel->text().startsWith(QStringLiteral("1手目")),
             qPrintable(window.m_positionSummaryLabel->text()));
}

/// 読み込めなかったファイルは履歴に入れない
void TestJosekiWindow::testHistoryKeepsOnlyLoadedFiles()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiWindow window;
    const QString book = m_tempDir.filePath(QStringLiteral("history.db"));
    QVERIFY(seedWindow(window, book));
    window.m_session->clearRecentFiles();
    const QString broken = m_tempDir.filePath(QStringLiteral("broken.db"));
    QFile file(broken);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not a book\n");
    file.close();

    // 読み込み失敗の警告は非同期の完了後に出るので、出るまで繰り返し応答する
    m_responseRole = QMessageBox::AcceptRole;
    m_messageSeen = false;
    QTimer responder;
    connect(&responder, &QTimer::timeout, this, &TestJosekiWindow::respondToMessageBox);
    responder.start(20);
    window.m_files->openFile(broken);
    QTRY_VERIFY(!window.isIoBusy() && m_messageSeen);
    responder.stop();
    QVERIFY(!window.m_session->recentFiles().contains(broken));

    window.m_files->openFile(book);
    QTRY_VERIFY(!window.isIoBusy());
    QCOMPARE(window.m_session->recentFiles().value(0), book);
}

/// 「履歴をクリア」の後は、開き直すか保存するまで次回の自動読込の対象にしない
void TestJosekiWindow::testClearHistoryStopsAutoLoad()
{
    JosekiWindow window;
    const QString book = m_tempDir.filePath(QStringLiteral("clear.db"));
    QVERIFY(seedWindow(window, book));
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    window.m_files->clearRecentFiles();
    QVERIFY(JosekiSettings::josekiWindowLastFilePath().isEmpty());
    window.saveSettings();  // ウィンドウを閉じるときの保存でも戻らない
    QVERIFY(JosekiSettings::josekiWindowLastFilePath().isEmpty());
    QVERIFY(window.m_session->save(book));
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
}

void TestJosekiWindow::testDirtyStatusSurvivesRefresh()
{
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("dirty.db"))));
    window.m_session->setModified(true);
    window.updateJosekiDisplay();
    QCOMPARE(window.m_fileStatusLabel->text(), QStringLiteral("未保存"));
    QVERIFY(window.m_session->removeMove(initialSfen(), QStringLiteral("7g7f")));
    window.updateJosekiDisplay();
    QCOMPARE(window.m_fileStatusLabel->text(), QStringLiteral("未保存"));
    window.m_session->setModified(false);
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
    const QString key = JosekiBookSession::positionKey(initialSfen());
    QVERIFY(window.m_session->updateMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 99, 32, 1, {}}));
    m_responseRole = static_cast<QMessageBox::ButtonRole>(role);
    m_messageSeen = false;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QCOMPARE(window.confirmClose(), accepted);
    QVERIFY(m_messageSeen);
    const auto disk = JosekiRepository::parseFromFile(path);
    QVERIFY(disk.success);
    QCOMPARE(disk.josekiData.value(key).first().value, saved ? 99 : 10);
    QCOMPARE(window.m_session->isModified(), !saved);
}

void TestJosekiWindow::testConfirmCloseSavesNewFile()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    JosekiWindow window;
    window.m_session->addMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    QVERIFY(window.m_saveButton->isEnabled());
    m_newSavePath = m_tempDir.filePath(QStringLiteral("close-new.db"));
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(window.confirmClose());
    QVERIFY(!window.isIoBusy());
    QVERIFY(!window.m_session->isModified());
    QVERIFY(JosekiRepository::parseFromFile(m_newSavePath).success);
    m_newSavePath.clear();
}

void TestJosekiWindow::testConfirmCloseWhileSaving()
{
    JosekiWindow window;
    const QString path = m_tempDir.filePath(QStringLiteral("close-busy.db"));
    QVERIFY(seedWindow(window, path));
    window.m_files->saveTo(path);
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
    const QString key = JosekiBookSession::positionKey(initialSfen());
    QVERIFY(window.m_session->updateMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("8c8d"), 10, 32, 2000000, {}}));
    window.updateJosekiDisplay();
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptMoveEdit);
    window.editMoveAt(0);
    QCOMPARE(window.m_model->moves().first().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(window.m_model->moves().first().frequency, 2000000);
    QVERIFY(window.m_session->save(path));
    const auto saved = JosekiRepository::parseFromFile(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.josekiData.value(key).first().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(saved.josekiData.value(key).first().frequency, 2000000);
}

void TestJosekiWindow::testColumnWidthsSurviveRefresh()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    {
        JosekiWindow window;
        QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("widths.db"))));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.m_tableView->setColumnWidth(2, 280);
        window.m_tableView->setColumnWidth(3, 270);
        window.setCurrentSfen(initialSfen());
        QCOMPARE(window.m_tableView->columnWidth(2), 280);
        window.hide();
        window.show();
        QCOMPARE(window.m_tableView->columnWidth(3), 270);
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
    QCOMPARE(restored.m_tableView->columnWidth(2), 280);
    QCOMPARE(restored.m_tableView->columnWidth(3), 270);
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
    QVERIFY(window.m_tableView->viewport()->rect().contains(window.m_emptyGuideLabel->geometry()));
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
    const QString afterMove = SfenReplay::positionAfter(SfenUtils::hirateSfen(), QStringLiteral("2g2f"));
    QVERIFY(!afterMove.isEmpty());
    window->setCurrentSfen(afterMove);
    dialog->setMove(QStringLiteral("7g7f"));
    dialog->accept();
}

void TestJosekiWindow::testAddKeepsTargetPosition()
{
    JosekiWindow window;
    window.setCurrentSfen(initialSfen());
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptMoveAfterPositionChange);
    window.onAddMoveButtonClicked();
    const auto moves = window.m_session->movesAt(initialSfen());
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().move, QStringLiteral("7g7f"));
    QVERIFY(!window.m_session->hasPosition(window.m_currentSfen));
}

/// 同じ局面が続けて設定されたときは表を更新しない。人間が指せるかが変わったとき・定跡データを
/// 変えたとき・局面が変わったときは中身を入れ替える（行の増減は挿入・削除で知らせ、列幅を保つ）
void TestJosekiWindow::testSameSfenKeepsTableRows()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("same.db"))));
    JosekiTableModel *model = window.m_model;
    QCOMPARE(model->rowCount(), 1);
    const auto cell = [model](int row, int column) { return model->index(row, column).data().toString(); };
    QCOMPARE(cell(0, JosekiTableModel::Play), QStringLiteral("着手"));
    QCOMPARE(cell(0, JosekiTableModel::Edit), QStringLiteral("編集"));

    // 同じ局面: 表は変えない
    QSignalSpy changed(model, &QAbstractItemModel::dataChanged);
    QSignalSpy reset(model, &QAbstractItemModel::modelReset);
    window.setCurrentSfen(initialSfen());
    QCOMPARE(changed.count(), 0);

    // 人間が指せなくなると「着手」の列だけ空になる
    window.setHumanCanPlayProvider([] { return false; });
    window.setCurrentSfen(initialSfen());
    QVERIFY(cell(0, JosekiTableModel::Play).isEmpty());
    QCOMPARE(cell(0, JosekiTableModel::Edit), QStringLiteral("編集"));
    window.setHumanCanPlayProvider([] { return true; });
    window.setCurrentSfen(initialSfen());
    QCOMPARE(cell(0, JosekiTableModel::Play), QStringLiteral("着手"));

    // 定跡手を足すと行が増える
    QSignalSpy inserted(model, &QAbstractItemModel::rowsInserted);
    window.m_session->addMove(initialSfen(), {QStringLiteral("2g2f"), QStringLiteral("none"), 5, 20, 1, {}});
    window.updateJosekiDisplay();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(window.m_model->moves().size(), 2);

    // 定跡に無い局面では行が消え、戻すと戻る
    window.setCurrentSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
    QCOMPARE(model->rowCount(), 0);
    window.setCurrentSfen(initialSfen());
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(reset.count(), 0);
}

/// 「着手」「編集」「削除」の列を押すと、その行の定跡手を指す・編集する・消す
void TestJosekiWindow::testButtonColumnsActOnRow()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiWindow window;
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("buttons.db"))));
    QSignalSpy selected(&window, &JosekiWindow::josekiMoveSelected);
    window.onTableClicked(window.m_model->index(0, JosekiTableModel::Play));
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.first().first().toString(), QStringLiteral("7g7f"));
    // 人間が指せないときは「着手」を押しても指さない
    window.setHumanCanPlayProvider([] { return false; });
    window.updateJosekiDisplay();
    window.onTableClicked(window.m_model->index(0, JosekiTableModel::Play));
    QCOMPARE(selected.count(), 1);

    m_responseRole = QMessageBox::YesRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    window.onTableClicked(window.m_model->index(0, JosekiTableModel::Delete));
    m_responseRole = QMessageBox::AcceptRole;
    QCOMPARE(window.m_model->rowCount(), 0);
    QVERIFY(window.m_session->isModified());
}

QTEST_MAIN(TestJosekiWindow)
#include "tst_josekiwindow.moc"
