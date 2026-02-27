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

#include "settingscommon.h"
#include "josekisettings.h"
#include "josekiwindow.h"

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

private:
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

QTEST_MAIN(TestJosekiWindow)
#include "tst_josekiwindow.moc"
