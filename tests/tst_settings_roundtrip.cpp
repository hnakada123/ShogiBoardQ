/// @file tst_settings_roundtrip.cpp
/// @brief 設定値の保存→復元ラウンドトリップテスト
///
/// テスト対象:
/// - 各ドメイン設定の setter → getter ラウンドトリップ
/// - int, bool, QString, QSize, QStringList, QList<int> の各型
/// - 全ドメインカテゴリ（App, Analysis, Game, Network, Engine, Joseki, Tsumeshogi）

#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include "settingsservice.h"

class Tst_SettingsRoundtrip : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // テストモードを有効にして、テスト用の設定パスを使用
        QStandardPaths::setTestModeEnabled(true);

        // 既存のテスト設定ファイルがあれば削除してクリーンな状態にする
        QString path = SettingsCommon::settingsFilePath();
        QFile::remove(path);
    }

    void cleanupTestCase()
    {
        // テスト設定ファイルを削除
        QString path = SettingsCommon::settingsFilePath();
        QFile::remove(path);
    }

    // ========================================
    // AppSettings: UI状態
    // ========================================

    void appSettings_lastSelectedTabIndex()
    {
        AppSettings::setLastSelectedTabIndex(3);
        QCOMPARE(AppSettings::lastSelectedTabIndex(), 3);

        AppSettings::setLastSelectedTabIndex(0);
        QCOMPARE(AppSettings::lastSelectedTabIndex(), 0);
    }

    void appSettings_toolbarVisible()
    {
        AppSettings::setToolbarVisible(false);
        QCOMPARE(AppSettings::toolbarVisible(), false);

        AppSettings::setToolbarVisible(true);
        QCOMPARE(AppSettings::toolbarVisible(), true);
    }

    void appSettings_language()
    {
        AppSettings::setLanguage(QStringLiteral("en"));
        QCOMPARE(AppSettings::language(), QStringLiteral("en"));

        AppSettings::setLanguage(QStringLiteral("ja_JP"));
        QCOMPARE(AppSettings::language(), QStringLiteral("ja_JP"));
    }

    void appSettings_menuWindow()
    {
        QSize testSize(900, 700);
        AppSettings::setMenuWindowSize(testSize);
        QCOMPARE(AppSettings::menuWindowSize(), testSize);

        AppSettings::setMenuWindowButtonSize(64);
        QCOMPARE(AppSettings::menuWindowButtonSize(), 64);

        AppSettings::setMenuWindowFontSize(14);
        QCOMPARE(AppSettings::menuWindowFontSize(), 14);

        QStringList favorites = {QStringLiteral("item1"), QStringLiteral("item2")};
        AppSettings::setMenuWindowFavorites(favorites);
        QCOMPARE(AppSettings::menuWindowFavorites(), favorites);
    }

    // ========================================
    // AnalysisSettings: 解析設定
    // ========================================

    void analysisSettings_evalChart()
    {
        AnalysisSettings::setEvalChartYLimit(3000);
        QCOMPARE(AnalysisSettings::evalChartYLimit(), 3000);

        AnalysisSettings::setEvalChartXLimit(300);
        QCOMPARE(AnalysisSettings::evalChartXLimit(), 300);

        AnalysisSettings::setEvalChartXInterval(20);
        QCOMPARE(AnalysisSettings::evalChartXInterval(), 20);

        AnalysisSettings::setEvalChartLabelFontSize(12);
        QCOMPARE(AnalysisSettings::evalChartLabelFontSize(), 12);
    }

    void analysisSettings_fontSizes()
    {
        AnalysisSettings::setUsiLogFontSize(11);
        QCOMPARE(AnalysisSettings::usiLogFontSize(), 11);

        AnalysisSettings::setThinkingFontSize(13);
        QCOMPARE(AnalysisSettings::thinkingFontSize(), 13);

        AnalysisSettings::setKifuAnalysisFontSize(14);
        QCOMPARE(AnalysisSettings::kifuAnalysisFontSize(), 14);

        AnalysisSettings::setConsiderationDialogFontSize(12);
        QCOMPARE(AnalysisSettings::considerationDialogFontSize(), 12);

        AnalysisSettings::setConsiderationFontSize(15);
        QCOMPARE(AnalysisSettings::considerationFontSize(), 15);
    }

    void analysisSettings_kifuAnalysis()
    {
        AnalysisSettings::setKifuAnalysisByoyomiSec(30);
        QCOMPARE(AnalysisSettings::kifuAnalysisByoyomiSec(), 30);

        AnalysisSettings::setKifuAnalysisEngineIndex(2);
        QCOMPARE(AnalysisSettings::kifuAnalysisEngineIndex(), 2);

        AnalysisSettings::setKifuAnalysisFullRange(true);
        QCOMPARE(AnalysisSettings::kifuAnalysisFullRange(), true);

        AnalysisSettings::setKifuAnalysisFullRange(false);
        QCOMPARE(AnalysisSettings::kifuAnalysisFullRange(), false);

        AnalysisSettings::setKifuAnalysisStartPly(10);
        QCOMPARE(AnalysisSettings::kifuAnalysisStartPly(), 10);

        AnalysisSettings::setKifuAnalysisEndPly(50);
        QCOMPARE(AnalysisSettings::kifuAnalysisEndPly(), 50);

        QSize dialogSize(850, 650);
        AnalysisSettings::setKifuAnalysisDialogSize(dialogSize);
        QCOMPARE(AnalysisSettings::kifuAnalysisDialogSize(), dialogSize);

        QSize resultsSize(700, 500);
        AnalysisSettings::setKifuAnalysisResultsWindowSize(resultsSize);
        QCOMPARE(AnalysisSettings::kifuAnalysisResultsWindowSize(), resultsSize);
    }

    void analysisSettings_consideration()
    {
        AnalysisSettings::setConsiderationEngineIndex(1);
        QCOMPARE(AnalysisSettings::considerationEngineIndex(), 1);

        AnalysisSettings::setConsiderationUnlimitedTime(true);
        QCOMPARE(AnalysisSettings::considerationUnlimitedTime(), true);

        AnalysisSettings::setConsiderationByoyomiSec(60);
        QCOMPARE(AnalysisSettings::considerationByoyomiSec(), 60);

        AnalysisSettings::setConsiderationMultiPV(4);
        QCOMPARE(AnalysisSettings::considerationMultiPV(), 4);
    }

    void analysisSettings_pvBoard()
    {
        QSize pvSize(500, 500);
        AnalysisSettings::setPvBoardDialogSize(pvSize);
        QCOMPARE(AnalysisSettings::pvBoardDialogSize(), pvSize);
    }

    void analysisSettings_columnWidths()
    {
        QList<int> widths = {100, 200, 150};
        AnalysisSettings::setEngineInfoColumnWidths(0, widths);
        QCOMPARE(AnalysisSettings::engineInfoColumnWidths(0), widths);

        QList<int> thinkWidths = {80, 120, 200, 50};
        AnalysisSettings::setThinkingViewColumnWidths(1, thinkWidths);
        QCOMPARE(AnalysisSettings::thinkingViewColumnWidths(1), thinkWidths);
    }

    // ========================================
    // GameSettings: 対局設定
    // ========================================

    void gameSettings_kifuDirectory()
    {
        QString dir = QStringLiteral("/home/test/kifu");
        GameSettings::setLastKifuDirectory(dir);
        QCOMPARE(GameSettings::lastKifuDirectory(), dir);

        QString saveDir = QStringLiteral("/home/test/kifu/save");
        GameSettings::setLastKifuSaveDirectory(saveDir);
        QCOMPARE(GameSettings::lastKifuSaveDirectory(), saveDir);
    }

    void gameSettings_recordPane()
    {
        GameSettings::setKifuPaneFontSize(12);
        QCOMPARE(GameSettings::kifuPaneFontSize(), 12);

        GameSettings::setKifuTimeColumnVisible(false);
        QCOMPARE(GameSettings::kifuTimeColumnVisible(), false);

        GameSettings::setKifuTimeColumnVisible(true);
        QCOMPARE(GameSettings::kifuTimeColumnVisible(), true);

        GameSettings::setKifuBookmarkColumnVisible(false);
        QCOMPARE(GameSettings::kifuBookmarkColumnVisible(), false);

        GameSettings::setKifuCommentColumnVisible(false);
        QCOMPARE(GameSettings::kifuCommentColumnVisible(), false);
    }

    void gameSettings_commentAndGameInfo()
    {
        GameSettings::setCommentFontSize(11);
        QCOMPARE(GameSettings::commentFontSize(), 11);

        GameSettings::setGameInfoFontSize(13);
        QCOMPARE(GameSettings::gameInfoFontSize(), 13);
    }

    void gameSettings_dialogSizes()
    {
        QSize startSize(1200, 700);
        GameSettings::setStartGameDialogSize(startSize);
        QCOMPARE(GameSettings::startGameDialogSize(), startSize);

        GameSettings::setStartGameDialogFontSize(11);
        QCOMPARE(GameSettings::startGameDialogFontSize(), 11);

        QSize pasteSize(700, 600);
        GameSettings::setKifuPasteDialogSize(pasteSize);
        QCOMPARE(GameSettings::kifuPasteDialogSize(), pasteSize);

        GameSettings::setKifuPasteDialogFontSize(12);
        QCOMPARE(GameSettings::kifuPasteDialogFontSize(), 12);

        QSize jishogiSize(300, 350);
        GameSettings::setJishogiScoreDialogSize(jishogiSize);
        QCOMPARE(GameSettings::jishogiScoreDialogSize(), jishogiSize);

        GameSettings::setJishogiScoreFontSize(14);
        QCOMPARE(GameSettings::jishogiScoreFontSize(), 14);
    }

    void gameSettings_sfenCollection()
    {
        QSize sfenSize(700, 800);
        GameSettings::setSfenCollectionDialogSize(sfenSize);
        QCOMPARE(GameSettings::sfenCollectionDialogSize(), sfenSize);

        QStringList recentFiles = {
            QStringLiteral("/path/to/file1.sfen"),
            QStringLiteral("/path/to/file2.sfen"),
        };
        GameSettings::setSfenCollectionRecentFiles(recentFiles);
        QCOMPARE(GameSettings::sfenCollectionRecentFiles(), recentFiles);

        GameSettings::setSfenCollectionSquareSize(60);
        QCOMPARE(GameSettings::sfenCollectionSquareSize(), 60);

        GameSettings::setSfenCollectionLastDirectory(QStringLiteral("/tmp/sfen"));
        QCOMPARE(GameSettings::sfenCollectionLastDirectory(), QStringLiteral("/tmp/sfen"));
    }

    // ========================================
    // NetworkSettings: CSAネットワーク設定
    // ========================================

    void networkSettings_fontSizes()
    {
        NetworkSettings::setCsaLogFontSize(12);
        QCOMPARE(NetworkSettings::csaLogFontSize(), 12);

        NetworkSettings::setCsaWaitingDialogFontSize(14);
        QCOMPARE(NetworkSettings::csaWaitingDialogFontSize(), 14);

        NetworkSettings::setCsaGameDialogFontSize(11);
        QCOMPARE(NetworkSettings::csaGameDialogFontSize(), 11);
    }

    void networkSettings_dialogSizes()
    {
        QSize logSize(600, 500);
        NetworkSettings::setCsaLogWindowSize(logSize);
        QCOMPARE(NetworkSettings::csaLogWindowSize(), logSize);

        QSize gameSize(500, 400);
        NetworkSettings::setCsaGameDialogSize(gameSize);
        QCOMPARE(NetworkSettings::csaGameDialogSize(), gameSize);
    }

    // ========================================
    // EngineDialogSettings: エンジン設定
    // ========================================

    void engineDialogSettings()
    {
        EngineDialogSettings::setEngineSettingsFontSize(13);
        QCOMPARE(EngineDialogSettings::engineSettingsFontSize(), 13);

        QSize settingsSize(900, 600);
        EngineDialogSettings::setEngineSettingsDialogSize(settingsSize);
        QCOMPARE(EngineDialogSettings::engineSettingsDialogSize(), settingsSize);

        EngineDialogSettings::setEngineRegistrationFontSize(11);
        QCOMPARE(EngineDialogSettings::engineRegistrationFontSize(), 11);

        QSize regSize(800, 550);
        EngineDialogSettings::setEngineRegistrationDialogSize(regSize);
        QCOMPARE(EngineDialogSettings::engineRegistrationDialogSize(), regSize);
    }

    // ========================================
    // JosekiSettings: 定跡設定
    // ========================================

    void josekiSettings_window()
    {
        JosekiSettings::setJosekiWindowFontSize(12);
        QCOMPARE(JosekiSettings::josekiWindowFontSize(), 12);

        JosekiSettings::setJosekiWindowSfenFontSize(10);
        QCOMPARE(JosekiSettings::josekiWindowSfenFontSize(), 10);

        QSize josekiSize(1100, 800);
        JosekiSettings::setJosekiWindowSize(josekiSize);
        QCOMPARE(JosekiSettings::josekiWindowSize(), josekiSize);

        JosekiSettings::setJosekiWindowAutoLoadEnabled(true);
        QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);

        JosekiSettings::setJosekiWindowAutoLoadEnabled(false);
        QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), false);

        JosekiSettings::setJosekiWindowDisplayEnabled(true);
        QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), true);

        JosekiSettings::setJosekiWindowSfenDetailVisible(true);
        QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), true);

        JosekiSettings::setJosekiWindowSfenDetailVisible(false);
        QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), false);

        QString lastPath = QStringLiteral("/home/test/joseki/book.db");
        JosekiSettings::setJosekiWindowLastFilePath(lastPath);
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), lastPath);

        QStringList recentFiles = {
            QStringLiteral("/path/to/joseki1.db"),
            QStringLiteral("/path/to/joseki2.db"),
        };
        JosekiSettings::setJosekiWindowRecentFiles(recentFiles);
        QCOMPARE(JosekiSettings::josekiWindowRecentFiles(), recentFiles);

        QList<int> colWidths = {50, 120, 80, 200};
        JosekiSettings::setJosekiWindowColumnWidths(colWidths);
        QCOMPARE(JosekiSettings::josekiWindowColumnWidths(), colWidths);
    }

    void josekiSettings_dialogs()
    {
        JosekiSettings::setJosekiMoveDialogFontSize(11);
        QCOMPARE(JosekiSettings::josekiMoveDialogFontSize(), 11);

        QSize moveSize(500, 400);
        JosekiSettings::setJosekiMoveDialogSize(moveSize);
        QCOMPARE(JosekiSettings::josekiMoveDialogSize(), moveSize);

        JosekiSettings::setJosekiMergeDialogFontSize(12);
        QCOMPARE(JosekiSettings::josekiMergeDialogFontSize(), 12);

        QSize mergeSize(600, 500);
        JosekiSettings::setJosekiMergeDialogSize(mergeSize);
        QCOMPARE(JosekiSettings::josekiMergeDialogSize(), mergeSize);
    }

    // ========================================
    // TsumeshogiSettings: 詰将棋設定
    // ========================================

    void tsumeshogiSettings()
    {
        QSize tsumSize(700, 600);
        TsumeshogiSettings::setTsumeshogiGeneratorDialogSize(tsumSize);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorDialogSize(), tsumSize);

        TsumeshogiSettings::setTsumeshogiGeneratorFontSize(13);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorFontSize(), 13);

        TsumeshogiSettings::setTsumeshogiGeneratorEngineIndex(1);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorEngineIndex(), 1);

        TsumeshogiSettings::setTsumeshogiGeneratorTargetMoves(7);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorTargetMoves(), 7);

        TsumeshogiSettings::setTsumeshogiGeneratorMaxAttackPieces(5);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces(), 5);

        TsumeshogiSettings::setTsumeshogiGeneratorMaxDefendPieces(3);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces(), 3);

        TsumeshogiSettings::setTsumeshogiGeneratorAttackRange(4);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorAttackRange(), 4);

        TsumeshogiSettings::setTsumeshogiGeneratorTimeoutSec(120);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec(), 120);

        TsumeshogiSettings::setTsumeshogiGeneratorMaxPositions(1000);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions(), 1000);

        TsumeshogiSettings::setTsumeshogiGeneratorLastSaveDirectory(QStringLiteral("/tmp/tsume"));
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(), QStringLiteral("/tmp/tsume"));
    }

    // ========================================
    // DockSettings: ドック設定
    // ========================================

    void dockSettings_lockState()
    {
        DockSettings::setDocksLocked(true);
        QCOMPARE(DockSettings::docksLocked(), true);

        DockSettings::setDocksLocked(false);
        QCOMPARE(DockSettings::docksLocked(), false);
    }

    void dockSettings_layoutManagement()
    {
        QByteArray layoutData("test-layout-data-12345");
        DockSettings::saveDockLayout(QStringLiteral("TestLayout"), layoutData);

        QByteArray loaded = DockSettings::loadDockLayout(QStringLiteral("TestLayout"));
        QCOMPARE(loaded, layoutData);

        QStringList names = DockSettings::savedDockLayoutNames();
        QVERIFY(names.contains(QStringLiteral("TestLayout")));

        DockSettings::setStartupDockLayoutName(QStringLiteral("TestLayout"));
        QCOMPARE(DockSettings::startupDockLayoutName(), QStringLiteral("TestLayout"));

        DockSettings::deleteDockLayout(QStringLiteral("TestLayout"));
        QByteArray deleted = DockSettings::loadDockLayout(QStringLiteral("TestLayout"));
        QVERIFY(deleted.isEmpty());
    }
};

QTEST_MAIN(Tst_SettingsRoundtrip)
#include "tst_settings_roundtrip.moc"
