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
#include "josekibook.h"
#include "josekifilecontroller.h"
#include "josekitablemodel.h"
#include "josekimergecontroller.h"
#include "josekimergedialog.h"
#include "josekiexpanddialog.h"
#include "josekimovedialog.h"
#include "josekimoveinputwidget.h"
#include "sfenreplay.h"
#include "sfenutils.h"

namespace {
/// 定跡手の入力欄に指し手と予想応手を入れる（値は既定値）
void setMoves(JosekiMoveDialog &dialog, const QString &move, const QString &next = QStringLiteral("none"))
{
    JosekiMove entry;
    entry.move = move;
    entry.nextMove = next;
    entry.depth = 32;
    entry.frequency = 1;
    dialog.setEntry(entry);
}
} // namespace

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
    void testAutoLoadAtStartup();
    void testDirtyStatusSurvivesRefresh();
    void testConfirmClose_data();
    void testConfirmClose();
    void testConfirmCloseSavesNewFile();
    void testDockCloseDiscardRevertsBook();
    void testAutoLoadOffKeepsLastFile();
    void testConfirmCloseWhileSaving();
    void testMoveValidation_data();
    void testMoveValidation();
    void testReplyValidationAndPreview();
    void testEditReplyAndLargeFrequency();
    void testColumnWidthsSurviveRefresh();
    void testEmptyAndStoppedDisplay();
    void testDialogCancelPersistsSize();
    void testAddKeepsTargetPosition();
    void testEditAndDeleteKeepTargetPosition();
    void testSameSfenKeepsTableRows();
    void testDerivedReplyFollowsNextPosition();
    void testRecordBoardMoves();
    void testExternalChangesRefreshTable();
    void testMergeMenuAndDroppedBook();
    void testReorderRows();
    void testExpandAndDeviation();
    void testButtonColumnsActOnRow();
    void testMoveListPlaceholder();
    void testFontButtonsKeepWidth();

private:
    static QString initialSfen();
    bool seedWindow(JosekiWindow &window, const QString &path);
    void respondToMessageBox();
    void chooseSaveFile();
    void acceptMoveEdit();
    void acceptMoveAfterPositionChange();
    void editMoveAfterPositionChange();
    void confirmDeleteAfterPositionChange();
    void acceptExpandDialog();
    QMessageBox::ButtonRole m_responseRole = QMessageBox::AcceptRole;
    bool m_messageSeen = false;
    QString m_lastMessageTitle;        ///< 最後に答えたメッセージボックスの題名
    QString m_lastMessageInformative;  ///< その補足の文
    QMessageBox::Icon m_lastMessageIcon = QMessageBox::NoIcon;
    /// 非同期の保存が終わってから出るメッセージボックスに答える（答えたら止まる）
    QTimer m_responder;
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
    m_responder.setInterval(10);
    connect(&m_responder, &QTimer::timeout, this, &TestJosekiWindow::respondToMessageBox);
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

    JosekiBookSession session;

    JosekiWindow window(&session);

    // ウィンドウのフォントサイズが 14 に設定されているか
    QCOMPARE(window.font().pointSize(), 14);

    // クリーンアップ
    JosekiSettings::josekiWindowFontSize.set(10);
}

void TestJosekiWindow::testApplyFontSizeToTableHeader()
{
    // フォントサイズ 16 を設定しておく
    JosekiSettings::josekiWindowFontSize.set(16);

    JosekiBookSession session;

    JosekiWindow window(&session);

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

    JosekiBookSession session;

    JosekiWindow window(&session);
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
        JosekiBookSession session2;
        JosekiWindow window2(&session2);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    m_lastMessageTitle = box->windowTitle();
    m_lastMessageInformative = box->informativeText();
    m_lastMessageIcon = box->icon();
    m_responder.stop();
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    // 右クリックのメニューの編集・削除も押せない
    window.m_tableView->updateMenuActions(0);
    QVERIFY(!window.m_tableView->action(JosekiTableView::RowAction::Edit)->isEnabled());
    QVERIFY(!window.m_tableView->action(JosekiTableView::RowAction::Delete)->isEnabled());
    QVERIFY(window.m_tableView->action(JosekiTableView::RowAction::CopyMove)->isEnabled());
    QVERIFY(!merge->isEnabled());
    window.m_session->setModified(true);
    QVERIFY(!window.m_saveButton->isEnabled());
    window.editMoveAt(0);
    window.deleteMoveAt(0);
    window.onAddMoveButtonClicked();
    QVERIFY(!window.m_merge->registerEntries({KifuMergeEntry{1, initialSfen(), QStringLiteral("2g2f"), {}, false}}));
    QVERIFY(!window.m_session->hasMove(initialSfen(), QStringLiteral("2g2f")));
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(window.m_tableView->isEnabled());
    QVERIFY(merge->isEnabled());
    QVERIFY(!window.m_session->isModified());
    const auto saved = JosekiBookFile::load(path);
    QVERIFY(saved.success);
    const auto moves = saved.book.movesAt(initialSfen());
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    const auto saved = JosekiBookFile::load(target);
    QVERIFY(saved.success);
    QCOMPARE(saved.book.movesAt(initialSfen()).size(), 1);
}

void TestJosekiWindow::testMergeSaveFailureAndRetry()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    m_responder.start();
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    // 保存はワーカースレッドで行い、失敗は終わってから知らせる
    QTRY_VERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(table->item(0, 3)->text().isEmpty());
    QVERIFY(table->cellWidget(0, 2)->isEnabled());
    QVERIFY(window.m_session->isModified());
    QCOMPARE(window.m_model->moves().first().frequency, 1);
    QVERIFY(QDir().mkpath(directory));
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QVERIFY(window.isIoBusy());
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(!table->cellWidget(0, 2)->isEnabled());
    QVERIFY(!window.m_session->isModified());
    const auto saved = JosekiBookFile::load(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.book.movesAt(initialSfen()).first().frequency, 2);
}

void TestJosekiWindow::testMergeRegisterAllSkipsRegistered()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("all.db"))));
    window.setKifuDataForMerge({initialSfen(), initialSfen(), initialSfen()},
                              {QStringLiteral("7g7f"), QStringLiteral("2g2f"), QStringLiteral("7g7f")}, {}, 1);
    auto *merge = window.findChild<JosekiMergeDialog*>();
    auto *table = merge->findChild<QTableWidget*>();
    qobject_cast<QPushButton*>(table->cellWidget(0, 2))->click();
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(!table->cellWidget(2, 2)->isEnabled());
    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    m_responder.start();
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    // 保存中はダイアログを使えないので、続けて押しても登録しない
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QTRY_VERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(!table->cellWidget(1, 2)->isEnabled());
    QCOMPARE(window.m_model->moves().size(), 2);
    QCOMPARE(window.m_model->moves().first().frequency, 2);
    QCOMPARE(window.m_model->moves().last().frequency, 1);
}

/// 「全て登録」はまとめて保存する。保存に失敗したら1手も登録せず、再試行で全て登録できる
void TestJosekiWindow::testMergeRegisterAllSavesAtOnce()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    m_responder.start();
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QTRY_VERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(table->cellWidget(0, 2)->isEnabled());
    QVERIFY(table->cellWidget(1, 2)->isEnabled());

    QVERIFY(QDir().mkpath(directory));
    m_messageSeen = false;
    m_responder.start();
    QVERIFY(QMetaObject::invokeMethod(merge, "onRegisterAllButtonClicked", Qt::DirectConnection));
    QTRY_VERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(!table->cellWidget(0, 2)->isEnabled());
    QVERIFY(!table->cellWidget(1, 2)->isEnabled());
    const auto saved = JosekiBookFile::load(path);
    QVERIFY(saved.success);
    const auto moves = saved.book.movesAt(initialSfen());
    QCOMPARE(moves.size(), 2);
    QCOMPARE(moves.first().frequency, 2);
}

/// マージの表は指し手と登録ボタンが収まる幅になる（指し手が縦に折り返さない）
void TestJosekiWindow::testMergeTableFitsMovesAndButtons()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
/// 「自動読込」: 起動時の設定の読み込みで前回のファイルを消さず、初めて表示したときに読み込む。
/// 表示しないまま閉じても前回のファイルを覚えている。履歴をクリアしたら読み込まない
void TestJosekiWindow::testAutoLoadAtStartup()
{
    const QString book = m_tempDir.filePath(QStringLiteral("auto.db"));
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        QVERIFY(seedWindow(window, book));
    }
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    JosekiSettings::josekiWindowAutoLoadEnabled.set(true);
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        QCOMPARE(window.m_pendingAutoLoadPath, book);
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
        window.saveSettings();  // 表示しないまま閉じたとき
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    }
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(window.m_session->filePath() == book && !window.isIoBusy());
        QCOMPARE(window.m_session->movesAt(initialSfen()).size(), 1);
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    }
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        QCOMPARE(window.m_pendingAutoLoadPath, book);
        window.m_files->clearRecentFiles();
        QVERIFY(window.m_pendingAutoLoadPath.isEmpty());
        QVERIFY(JosekiSettings::josekiWindowLastFilePath().isEmpty());
    }
    // 表示する前に別のファイルを作ったら、前回のファイルは読み込まない
    QVERIFY(JosekiBookFile::save(book, JosekiBookFile::load(book).book).success);
    JosekiSettings::josekiWindowLastFilePath.set(book);
    JosekiSettings::josekiWindowRecentFiles.set({book});
    JosekiBookSession session;
    JosekiWindow window(&session);
    QCOMPARE(window.m_pendingAutoLoadPath, book);
    window.m_files->newBook();
    QVERIFY(window.m_pendingAutoLoadPath.isEmpty());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(!window.isIoBusy());
    QVERIFY(window.m_session->filePath().isEmpty());
}

void TestJosekiWindow::testClearHistoryStopsAutoLoad()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    JosekiBookSession session;
    JosekiWindow window(&session);
    const QString path = m_tempDir.filePath(QStringLiteral("close.db"));
    QVERIFY(seedWindow(window, path));
    const QString key = JosekiBookSession::positionKey(initialSfen());
    QVERIFY(window.m_session->updateMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 99, 32, 1, {}}));
    m_responseRole = static_cast<QMessageBox::ButtonRole>(role);
    m_messageSeen = false;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QCOMPARE(window.confirmClose(), accepted);
    QVERIFY(m_messageSeen);
    // 棋譜の未保存の警告と同じ見た目で、どの定跡ファイルかを添える
    QCOMPARE(m_lastMessageTitle, QStringLiteral("未保存の定跡"));
    QCOMPARE(m_lastMessageIcon, QMessageBox::Warning);
    QCOMPARE(m_lastMessageInformative, QStringLiteral("定跡ファイル: close.db"));
    const auto disk = JosekiBookFile::load(path);
    QVERIFY(disk.success);
    QCOMPARE(disk.book.movesAt(key).first().value, saved ? 99 : 10);
    QCOMPARE(window.m_session->isModified(), !saved);
}

/// 定跡ドックを閉じるときに「破棄」を選ぶと、定跡は保存した内容に戻る（閉じても矢印・対局に使い続けるため）
void TestJosekiWindow::testDockCloseDiscardRevertsBook()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
    QDockWidget dock;
    window.setDockWidget(&dock);
    const QString path = m_tempDir.filePath(QStringLiteral("dock-close.db"));
    QVERIFY(seedWindow(window, path));
    const QString key = JosekiBookSession::positionKey(initialSfen());
    QVERIFY(window.m_session->updateMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 99, 32, 1, {}}));
    QVERIFY(window.m_session->isModified());

    m_responseRole = QMessageBox::DestructiveRole;
    m_messageSeen = false;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    dock.show();
    QVERIFY(dock.close());
    QVERIFY(m_messageSeen);
    QTRY_VERIFY(!window.isIoBusy());
    QVERIFY(!window.m_session->isModified());
    QCOMPARE(window.m_session->movesAt(key).first().value, 10);

    // 新しい定跡（ファイルなし）の変更を破棄すると空の定跡になる
    window.m_files->newBook();
    window.m_session->addMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    m_messageSeen = false;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    dock.show();
    QVERIFY(dock.close());
    QVERIFY(m_messageSeen);
    QVERIFY(!window.m_session->isModified());
    QVERIFY(window.m_session->movesAt(key).isEmpty());
}

/// 「自動読込」をオフにして起動しても、前回のファイルの記録は消えない（後でオンにすれば次の起動で読み込む）
void TestJosekiWindow::testAutoLoadOffKeepsLastFile()
{
    const QString book = m_tempDir.filePath(QStringLiteral("auto-off.db"));
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        QVERIFY(seedWindow(window, book));
    }
    QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
        QVERIFY(window.m_pendingAutoLoadPath.isEmpty());
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
        window.saveSettings();
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), book);
    }
    JosekiSettings::josekiWindowAutoLoadEnabled.set(true);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QCOMPARE(window.m_pendingAutoLoadPath, book);
}

void TestJosekiWindow::testConfirmCloseSavesNewFile()
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    JosekiBookSession session;
    JosekiWindow window(&session);
    window.m_session->addMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 10, 32, 1, {}});
    QVERIFY(window.m_saveButton->isEnabled());
    m_newSavePath = m_tempDir.filePath(QStringLiteral("close-new.db"));
    m_responseRole = QMessageBox::AcceptRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    QVERIFY(window.confirmClose());
    QCOMPARE(m_lastMessageInformative, QStringLiteral("新しい定跡（まだファイルに保存していません）"));
    QVERIFY(!window.isIoBusy());
    QVERIFY(!window.m_session->isModified());
    QVERIFY(JosekiBookFile::load(m_newSavePath).success);
    m_newSavePath.clear();
}

void TestJosekiWindow::testConfirmCloseWhileSaving()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    setMoves(dialog, usi);
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
    setMoves(dialog, QStringLiteral("7g7f"));
    auto *buttons = dialog.findChild<QDialogButtonBox*>();
    auto *input = dialog.findChild<JosekiMoveInputWidget*>(QStringLiteral("moveInput"));
    auto *reply = dialog.findChild<JosekiMoveInputWidget*>(QStringLiteral("nextMoveInput"));
    QVERIFY(input && reply && buttons);
    QCOMPARE(input->findChild<QLabel*>(QStringLiteral("movePreview"))->text(), QStringLiteral("▲７六歩(77)"));
    // 指し手は局面の合法手の一覧から選ぶ（30手）。予想応手は着手後の局面の合法手と「なし」
    auto *moves = input->findChild<QComboBox*>(QStringLiteral("moveInputCombo"));
    auto *replies = reply->findChild<QComboBox*>(QStringLiteral("nextMoveInputCombo"));
    QVERIFY(moves && replies);
    QCOMPARE(moves->count(), 30);
    QCOMPARE(replies->count(), 31);
    QCOMPARE(replies->itemData(0).toString(), QStringLiteral("none"));
    setMoves(dialog, QStringLiteral("7g7f"), QStringLiteral("8c8d"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QCOMPARE(reply->findChild<QLabel*>(QStringLiteral("movePreview"))->text(), QStringLiteral("△８四歩(83)"));
    // 着手後の局面の合法手でない予想応手は選べない
    setMoves(dialog, QStringLiteral("7g7f"), QStringLiteral("7f7e"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    setMoves(dialog, QStringLiteral("7g7f"), QStringLiteral("8c8c"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    setMoves(dialog, QStringLiteral("7g7f"), QStringLiteral("none"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    setMoves(dialog, QStringLiteral("7g7e"));
    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QVERIFY(!reply->isEnabled());
    setMoves(dialog, QStringLiteral("7g7f"), QStringLiteral("8c8d"));
    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
    QVERIFY(reply->isEnabled());
    // 一覧で選び直すと、予想応手の一覧も着手後の局面に替わる
    moves->setCurrentIndex(moves->findData(QStringLiteral("2g2f")));
    QCOMPARE(dialog.entry().move, QStringLiteral("2g2f"));
    QCOMPARE(dialog.entry().nextMove, QStringLiteral("8c8d"));
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    const QString shots = qEnvironmentVariable("JOSEKI_SCREENSHOT_DIR");
    if (!shots.isEmpty()) QVERIFY(dialog.grab().save(shots + QStringLiteral("/joseki-add.png")));
}

void TestJosekiWindow::acceptMoveEdit()
{
    auto *dialog = qobject_cast<JosekiMoveDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    QCOMPARE(dialog->entry().move, QStringLiteral("7g7f"));
    QCOMPARE(dialog->entry().frequency, 2000000);
    auto *reply = dialog->findChild<JosekiMoveInputWidget*>(QStringLiteral("nextMoveInput"));
    QVERIFY(reply && reply->isVisible());
    JosekiMove edited = dialog->entry();
    QCOMPARE(edited.nextMove, QStringLiteral("8c8d"));
    edited.nextMove = QStringLiteral("3c3d");
    dialog->setEntry(edited);
    dialog->accept();
}

void TestJosekiWindow::testEditReplyAndLargeFrequency()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
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
    const auto saved = JosekiBookFile::load(path);
    QVERIFY(saved.success);
    QCOMPARE(saved.book.movesAt(key).first().nextMove, QStringLiteral("3c3d"));
    QCOMPARE(saved.book.movesAt(key).first().frequency, 2000000);
}

void TestJosekiWindow::testColumnWidthsSurviveRefresh()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    {
        JosekiBookSession session;
        JosekiWindow window(&session);
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
    JosekiBookSession restoredBook;
    JosekiWindow restored(&restoredBook);
    restored.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored));
    QCOMPARE(restored.m_tableView->columnWidth(2), 280);
    QCOMPARE(restored.m_tableView->columnWidth(3), 270);
}

void TestJosekiWindow::testEmptyAndStoppedDisplay()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
    window.setCurrentSfen(initialSfen());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.m_tableView->emptyGuideLabel()->isVisible());
    window.resize(1100, 700);
    QCoreApplication::processEvents();
    QVERIFY(window.m_tableView->viewport()->rect().contains(window.m_tableView->emptyGuideLabel()->geometry()));
    window.m_stopButton->setChecked(true);
    window.onStopButtonClicked();
    QVERIFY(window.m_stopButton->isVisible()); // 空の定跡でも再開できる
    QVERIFY(window.m_tableView->emptyGuideLabel()->text().contains(QStringLiteral("停止")));
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
    setMoves(*dialog, QStringLiteral("7g7f"));
    dialog->accept();
}

void TestJosekiWindow::testAddKeepsTargetPosition()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
    window.setCurrentSfen(initialSfen());
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptMoveAfterPositionChange);
    window.onAddMoveButtonClicked();
    const auto moves = window.m_session->movesAt(initialSfen());
    QCOMPARE(moves.size(), 1);
    QCOMPARE(moves.first().move, QStringLiteral("7g7f"));
    QVERIFY(!window.m_session->hasPosition(window.m_currentSfen));
}

void TestJosekiWindow::editMoveAfterPositionChange()
{
    auto *dialog = qobject_cast<JosekiMoveDialog*>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    auto *window = qobject_cast<JosekiWindow*>(dialog->parentWidget());
    QVERIFY(window);
    window->setCurrentSfen(SfenReplay::positionAfter(SfenUtils::hirateSfen(), QStringLiteral("2g2f")));
    JosekiMove edited = dialog->entry();
    edited.value = 99;
    dialog->setEntry(edited);
    dialog->accept();
}

void TestJosekiWindow::confirmDeleteAfterPositionChange()
{
    auto *box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    QVERIFY(box);
    auto *window = qobject_cast<JosekiWindow*>(box->parentWidget());
    QVERIFY(window);
    window->setCurrentSfen(SfenReplay::positionAfter(SfenUtils::hirateSfen(), QStringLiteral("2g2f")));
    box->button(QMessageBox::Yes)->click();
}

/// 編集・削除も、ダイアログを開いている間に局面が進んだら（対局中にエンジンが指すなど）、開いたときの局面の手に行う
void TestJosekiWindow::testEditAndDeleteKeepTargetPosition()
{
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("keep-target.db"))));
    QTimer::singleShot(0, this, &TestJosekiWindow::editMoveAfterPositionChange);
    window.editMoveAt(0);
    QCOMPARE(window.m_session->movesAt(initialSfen()).first().value, 99);
    QVERIFY(!window.m_session->hasPosition(window.m_currentSfen));

    window.setCurrentSfen(initialSfen());
    QTimer::singleShot(0, this, &TestJosekiWindow::confirmDeleteAfterPositionChange);
    window.deleteMoveAt(0);
    QVERIFY(!window.m_session->hasPosition(initialSfen()));
}

/// 同じ局面が続けて設定されたときは表を更新しない。人間が指せるかが変わったとき・定跡データを
/// 変えたとき・局面が変わったときは中身を入れ替える（行の増減は挿入・削除で知らせ、列幅を保つ）
void TestJosekiWindow::testSameSfenKeepsTableRows()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
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
/// 予想応手が「なし」の手には、次の局面の定跡手からやねうら王が先読みに使う手を灰色で出し、
/// 次の局面の定跡が変わればそれに合わせる
void TestJosekiWindow::testDerivedReplyFollowsNextPosition()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("derived.db"))));
    const auto next = [&window](int role) { return window.m_model->index(0, JosekiTableModel::NextMove).data(role); };
    QCOMPARE(next(Qt::DisplayRole).toString(), QStringLiteral("なし"));

    // ▲７六歩の後の局面に △８四歩（出現頻度1）と △３四歩（出現頻度5）を登録する
    const QString after = SfenReplay::positionAfter(initialSfen(), QStringLiteral("7g7f"));
    session.addMove(after, {QStringLiteral("8c8d"), QStringLiteral("none"), 0, 0, 1, {}});
    session.addMove(after, {QStringLiteral("3c3d"), QStringLiteral("none"), 0, 0, 5, {}});
    window.updateJosekiDisplay();
    QCOMPARE(next(Qt::DisplayRole).toString(), QStringLiteral("（△３四歩(33)）"));
    QCOMPARE(next(Qt::ForegroundRole).value<QColor>(), QColor(Qt::darkGray));

    // 予想応手を書けば、その手を普通の色で出す
    QVERIFY(session.updateMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("8c8d"), 10, 32, 1, {}}));
    window.updateJosekiDisplay();
    QCOMPARE(next(Qt::DisplayRole).toString(), QStringLiteral("△８四歩(83)"));
    QVERIFY(!next(Qt::ForegroundRole).isValid());
}

/// 「盤の手を登録」: オンの間だけ、まだ無い手を予想応手なし・評価値0・深さ0・出現頻度1で登録する（ある手は変えない）
void TestJosekiWindow::testRecordBoardMoves()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("record.db"))));
    const QString after = SfenReplay::positionAfter(initialSfen(), QStringLiteral("7g7f"));

    // オフ（起動したとき）なら登録しない
    QVERIFY(!window.isRecordingBoardMoves());
    window.recordBoardMove(after, QStringLiteral("3c3d"));
    QVERIFY(!session.hasPosition(after));
    QVERIFY(!session.isModified());

    window.m_recordBoardMovesCheckBox->setChecked(true);
    QVERIFY2(window.m_statusLabel->text().contains(QStringLiteral("盤の手を登録中")), qPrintable(window.m_statusLabel->text()));
    window.recordBoardMove(after, QStringLiteral("3c3d"));
    QCOMPARE(session.movesAt(after).size(), 1);
    const JosekiMove added = session.movesAt(after).constFirst();
    QCOMPARE(added.move, QStringLiteral("3c3d"));
    QCOMPARE(added.nextMove, QStringLiteral("none"));
    QCOMPARE(added.value, 0);
    QCOMPARE(added.depth, 0);
    QCOMPARE(added.frequency, 1);
    QVERIFY(session.isModified());

    // 登録済みの手は変えない
    window.recordBoardMove(initialSfen(), QStringLiteral("7g7f"));
    const JosekiMove kept = session.movesAt(initialSfen()).constFirst();
    QCOMPARE(kept.value, 10);
    QCOMPARE(kept.depth, 32);
    QCOMPARE(kept.frequency, 1);

    // 前回のファイルの読み込みを待っている間（読み込むとデータを置き換える）は登録しない
    window.m_pendingAutoLoadPath = m_tempDir.filePath(QStringLiteral("pending.db"));
    QVERIFY(!window.canRegisterMoves());
    window.recordBoardMove(after, QStringLiteral("8c8d"));
    QCOMPARE(session.movesAt(after).size(), 1);
    window.m_pendingAutoLoadPath.clear();
    QVERIFY(window.canRegisterMoves());
}

/// ほかの部品が定跡を変えても表を作り直す（表示中ならすぐ、隠れていれば次に局面が届いたとき）
void TestJosekiWindow::testExternalChangesRefreshTable()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("external.db"))));
    QCOMPARE(window.m_model->rowCount(), 1);

    session.addMove(initialSfen(), {QStringLiteral("2g2f"), QStringLiteral("none"), 0, 0, 1, {}});
    QCOMPARE(window.m_model->rowCount(), 1);
    window.setCurrentSfen(initialSfen());
    QCOMPARE(window.m_model->rowCount(), 2);

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    session.addMove(initialSfen(), {QStringLiteral("5g5f"), QStringLiteral("none"), 0, 0, 1, {}});
    QCOMPARE(window.m_model->rowCount(), 3);
}

/// 「マージ ▼」の項目。定跡ファイル1つのドロップは開き、定跡ファイルを足している間は状態に出す
void TestJosekiWindow::testMergeMenuAndDroppedBook()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    auto *menu = window.findChild<QMenu *>(QStringLiteral("josekiMergeMenu"));
    QVERIFY(menu);
    QStringList texts;
    const QList<QAction *> actions = menu->actions();
    for (QAction *action : actions) {
        if (!action->isSeparator()) texts << action->text();
    }
    QCOMPARE(texts, (QStringList{QStringLiteral("現在の棋譜から"), QStringLiteral("棋譜ファイルから"),
                                 QStringLiteral("複数の棋譜から..."), QStringLiteral("定跡ファイルから...")}));
    QVERIFY(window.acceptDrops());

    const QString path = m_tempDir.filePath(QStringLiteral("dropped.db"));
    {
        JosekiBookSession other;
        other.addMove(initialSfen(), {QStringLiteral("7g7f"), QStringLiteral("none"), 0, 0, 3, {}});
        QVERIFY(other.save(path));
    }
    QSignalSpy loaded(&session, &JosekiBookSession::loaded);
    window.m_droppedPaths = {path};
    window.openDroppedPaths();
    QVERIFY(loaded.wait(5000));
    QCOMPARE(session.filePath(), path);

    // 足し終えたら結果のメッセージを出す（ここでは閉じる）
    QSignalSpy merged(&session, &JosekiBookSession::bookMerged);
    QVERIFY(session.startMergeBook(path, {}));
    QVERIFY2(window.m_statusLabel->text().contains(QStringLiteral("定跡ファイルを足しています")),
             qPrintable(window.m_statusLabel->text()));
    m_messageSeen = false;
    m_responseRole = QMessageBox::AcceptRole;
    m_responder.start();
    QVERIFY(merged.wait(5000));
    QTRY_VERIFY(m_messageSeen);
    QCOMPARE(session.movesAt(initialSfen()).constFirst().frequency, 6);
    QVERIFY(session.isModified());
}

/// 右クリックの並べ替え: 先頭へ・上へ・下へ移し、出現頻度・評価値の順に並べる。動かした手は選んだまま
void TestJosekiWindow::testReorderRows()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("reorder.db"))));  // 7g7f（値10・頻度1）
    session.addMove(initialSfen(), {QStringLiteral("2g2f"), QStringLiteral("none"), 50, 0, 7, {}});
    session.addMove(initialSfen(), {QStringLiteral("5g5f"), QStringLiteral("none"), -20, 0, 3, {}});
    session.setModified(false);
    window.updateJosekiDisplay();
    const auto order = [&window] {
        QStringList moves;
        for (const JosekiMove &move : window.m_model->moves()) moves << move.move;
        return moves;
    };
    using Action = JosekiTableView::RowAction;
    JosekiTableView *table = window.m_tableView;

    // 先頭の行は上へ動かせず、末尾の行は下へ動かせない
    table->updateMenuActions(0);
    QVERIFY(!table->action(Action::MoveToTop)->isEnabled());
    QVERIFY(!table->action(Action::MoveUp)->isEnabled());
    QVERIFY(table->action(Action::MoveDown)->isEnabled());
    table->updateMenuActions(2);
    QVERIFY(table->action(Action::MoveUp)->isEnabled());
    QVERIFY(!table->action(Action::MoveDown)->isEnabled());

    emit table->rowActionRequested(Action::MoveToTop, 2);
    QCOMPARE(order(), (QStringList{"5g5f", "7g7f", "2g2f"}));
    QCOMPARE(table->currentIndex().row(), 0);
    QVERIFY(session.isModified());
    emit table->rowActionRequested(Action::MoveDown, 0);
    QCOMPARE(order(), (QStringList{"7g7f", "5g5f", "2g2f"}));
    QCOMPARE(table->currentIndex().row(), 1);
    emit table->rowActionRequested(Action::MoveUp, 2);
    QCOMPARE(order(), (QStringList{"7g7f", "2g2f", "5g5f"}));
    emit table->rowActionRequested(Action::SortByFrequency, 0);
    QCOMPARE(order(), (QStringList{"2g2f", "5g5f", "7g7f"}));
    QCOMPARE(table->currentIndex().row(), 2);
    emit table->rowActionRequested(Action::SortByValue, 0);
    QCOMPARE(order(), (QStringList{"2g2f", "7g7f", "5g5f"}));

    // 並びはファイルに書く順なので、保存して読み直しても同じ
    QVERIFY(session.save(m_tempDir.filePath(QStringLiteral("reorder.db"))));
    JosekiBookSession reloaded;
    QSignalSpy loaded(&reloaded, &JosekiBookSession::loaded);
    QVERIFY(reloaded.startLoad(m_tempDir.filePath(QStringLiteral("reorder.db"))));
    QVERIFY(loaded.wait(5000));
    QCOMPARE(reloaded.movesAt(initialSfen()).constFirst().move, QStringLiteral("2g2f"));
    QCOMPARE(reloaded.movesAt(initialSfen()).constLast().move, QStringLiteral("5g5f"));
}

/// 「棋譜に展開...」は定跡手がある局面で押せ、ダイアログの条件を知らせる。定跡を外れた手の表示は押すとその手数を知らせる
void TestJosekiWindow::testExpandAndDeviation()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("expand.db"))));
    QVERIFY(window.m_expandButton->isEnabled());
    window.setCurrentSfen(SfenReplay::positionAfter(initialSfen(), QStringLiteral("2g2f")));
    QVERIFY(!window.m_expandButton->isEnabled());
    window.setCurrentSfen(initialSfen());

    QSignalSpy expand(&window, &JosekiWindow::expandRequested);
    QTimer::singleShot(0, this, &TestJosekiWindow::acceptExpandDialog);
    window.onExpandButtonClicked();
    QCOMPARE(expand.count(), 1);
    const auto options = expand.constFirst().constFirst().value<JosekiLines::ExpandOptions>();
    QCOMPARE(options.maxPlies, 5);
    QCOMPARE(JosekiSettings::josekiExpandMaxPlies(), 5);

    QSignalSpy deviation(&window, &JosekiWindow::deviationPlyRequested);
    window.setBookDeviation(-1, QString());
    QVERIFY(window.m_deviationButton->isHidden());
    window.setBookDeviation(0, QString());
    QVERIFY(!window.m_deviationButton->isHidden());
    QVERIFY(!window.m_deviationButton->isEnabled());
    window.setBookDeviation(14, QStringLiteral("▲２五歩(26)"));
    QVERIFY(window.m_deviationButton->text().contains(QStringLiteral("14手目の▲２五歩(26)")));
    window.m_deviationButton->click();
    QCOMPARE(deviation.count(), 1);
    QCOMPARE(deviation.constFirst().constFirst().toInt(), 14);
}

void TestJosekiWindow::acceptExpandDialog()
{
    auto *dialog = qobject_cast<JosekiExpandDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    dialog->findChild<QSpinBox *>(QStringLiteral("josekiExpandMaxPlies"))->setValue(5);
    dialog->accept();
}

void TestJosekiWindow::testButtonColumnsActOnRow()
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
    JosekiBookSession session;
    JosekiWindow window(&session);
    QVERIFY(seedWindow(window, m_tempDir.filePath(QStringLiteral("buttons.db"))));
    QSignalSpy selected(&window, &JosekiWindow::josekiMoveSelected);
    emit window.m_tableView->clicked(window.m_model->index(0, JosekiTableModel::Play));
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.first().first().toString(), QStringLiteral("7g7f"));
    // 人間が指せないときは「着手」を押しても指さない
    window.setHumanCanPlayProvider([] { return false; });
    window.updateJosekiDisplay();
    emit window.m_tableView->clicked(window.m_model->index(0, JosekiTableModel::Play));
    QCOMPARE(selected.count(), 1);

    m_responseRole = QMessageBox::YesRole;
    QTimer::singleShot(0, this, &TestJosekiWindow::respondToMessageBox);
    emit window.m_tableView->clicked(window.m_model->index(0, JosekiTableModel::Delete));
    m_responseRole = QMessageBox::AcceptRole;
    QCOMPARE(window.m_model->rowCount(), 0);
    QVERIFY(window.m_session->isModified());
}

QTEST_MAIN(TestJosekiWindow)
void TestJosekiWindow::testMoveListPlaceholder()
{
    // 合法手が並んでいるのに、選んでいないだけで「局面がありません」と出さない
    JosekiMoveInputWidget input(QStringLiteral("moveInput"), QStringLiteral("指し手"), false);
    auto *combo = input.findChild<QComboBox*>(QStringLiteral("moveInputCombo"));
    QVERIFY(combo);
    QCOMPARE(combo->count(), 0);
    QCOMPARE(combo->placeholderText(), QStringLiteral("局面がありません"));
    input.setCurrentSfen(initialSfen());
    QCOMPARE(combo->count(), 30);
    QCOMPARE(combo->currentIndex(), -1);
    QCOMPARE(combo->placeholderText(), QStringLiteral("合法手から選んでください"));
    input.setCurrentSfen(QString());
    QCOMPARE(combo->count(), 0);
    QCOMPARE(combo->placeholderText(), QStringLiteral("局面がありません"));
}

void TestJosekiWindow::testFontButtonsKeepWidth()
{
    // A-/A+ は「表示」の枠の中で文字が収まる幅を保つ（スタイルだけだと 20px ほどに縮む）
    JosekiBookSession session;
    JosekiWindow window(&session);
    for (const char *name : {"josekiFontDecrease", "josekiFontIncrease"}) {
        auto *button = window.findChild<QPushButton*>(QString::fromLatin1(name));
        QVERIFY2(button, name);
        QCOMPARE(button->minimumWidth(), 36);
        QCOMPARE(button->maximumWidth(), 36);
    }
}

#include "tst_josekiwindow.moc"
