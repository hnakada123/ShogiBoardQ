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
#include "settingskeys.h"
#include "enginelistsettings.h"
#include "settingscommon.h"
#include <QSettings>

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
    // Setting<T>: 宣言の表の既定値と消去
    // ========================================

    /// 未保存の項目は宣言した既定値を返すこと（保存前に実行する）
    void declaredDefaultsBeforeSaving()
    {
        QCOMPARE(AppSettings::language(), QStringLiteral("system"));
        QCOMPARE(AppSettings::toolbarVisible(), true);
        QCOMPARE(AppSettings::menuWindowSize(), QSize(500, 400));
        QCOMPARE(AppSettings::menuWindowFavorites(), QStringList());
        QCOMPARE(GameSettings::kifuPaneFontSize(), 10);
        QCOMPARE(GameSettings::startGameDialogSize(), QSize(1000, 580));
        QCOMPARE(GameSettings::lastKifuDirectory(), QString());
    }

    /// 宣言したキーへ保存され、remove() で既定値へ戻ること
    void settingWritesDeclaredKeyAndRemoveRestoresDefault()
    {
        const Setting<int>& fontSize = GameSettings::jishogiScoreFontSize;
        QCOMPARE(fontSize.defaultValue(), 10);

        fontSize.set(15);
        QCOMPARE(fontSize(), 15);
        QCOMPARE(SettingsCommon::openSettings().value(QLatin1String(fontSize.key())).toInt(), 15);

        fontSize.remove();
        QVERIFY(!SettingsCommon::openSettings().contains(QLatin1String(fontSize.key())));
        QCOMPARE(fontSize(), fontSize.defaultValue());
    }

    /// BoundedSetting: 保存した範囲外の値も、読むときに範囲へ収めること
    void boundedSettingClampsStoredValue()
    {
        const BoundedSetting<int>& target = AnalysisSettings::usiLogCommandTarget;
        SettingsCommon::openSettings().setValue(QLatin1String(target.key()), 9);
        QCOMPARE(target(), 2);
        target.set(-3);
        QCOMPARE(SettingsCommon::openSettings().value(QLatin1String(target.key())).toInt(), 0);
        target.remove();
        QCOMPARE(target(), 0);
    }

    /// StructSetting: 項目ごとのキーに保存し、保存していない項目は構造体の既定値のままにすること
    void structSettingUsesFieldKeys()
    {
        QSettings& s = SettingsCommon::openSettings();
        s.remove(QStringLiteral("TsumePlay"));
        s.setValue(QStringLiteral("TsumePlay/squareSize"), 50);
        const TsumeshogiSettings::PlayPreferences partial = TsumeshogiSettings::playPreferences();
        QCOMPARE(partial.squareSize, 50);
        QCOMPARE(partial.problemIndex, TsumeshogiSettings::PlayPreferences{}.problemIndex);
        QCOMPARE(partial.timeoutSec, TsumeshogiSettings::PlayPreferences{}.timeoutSec);

        TsumeshogiSettings::PlayPreferences preferences;
        preferences.timeoutSec = 30;
        TsumeshogiSettings::playPreferences.set(preferences);
        QCOMPARE(s.value(QStringLiteral("TsumePlay/timeout")).toInt(), 30);
        QCOMPARE(TsumeshogiSettings::playPreferences().squareSize, preferences.squareSize);
        s.remove(QStringLiteral("TsumePlay"));
    }

    /// グループ名・番号だけ違う組は、組み立てたキーに保存し、組ごとに別のキーになること
    void groupedSettingsUseBuiltKeys()
    {
        using GameSettings::StartGame::player1;
        using GameSettings::StartGame::player2;
        QCOMPARE(QByteArray(player1.engineName.key()), QByteArray("GameSettings/engineName1"));
        QCOMPARE(QByteArray(player2.addEachMoveSec.key()), QByteArray("GameSettings/addEachMoveSec2"));
        QVERIFY(player1.isHuman.defaultValue());
        QVERIFY(!player2.isHuman.defaultValue());
        QVERIFY(player2.isEngine.defaultValue());

        QSettings& s = SettingsCommon::openSettings();
        s.remove(QStringLiteral("JosekiPlay1"));
        s.remove(QStringLiteral("JosekiPlay2"));
        JosekiPlayOptions options;
        options.enabled = true;
        options.maxPly = 12;
        JosekiSettings::playOptions2.set(options);
        QCOMPARE(s.value(QStringLiteral("JosekiPlay2/maxPly")).toInt(), 12);
        QVERIFY(!s.contains(QStringLiteral("JosekiPlay1/maxPly")));
        QCOMPARE(JosekiSettings::playOptions(2)().maxPly, 12);
        QCOMPARE(JosekiSettings::playOptions1().maxPly, JosekiPlayOptions{}.maxPly);
        s.remove(QStringLiteral("JosekiPlay2"));
    }

    /// ListSetting: 配列（size と 番号/項目）で保存し、実行時に決めたキーでも読み書きできること
    void listSettingUsesArrayFormat()
    {
        const ListSetting<int> widths(QStringLiteral("RoundtripTest/widths"), "width");
        QVERIFY(widths().isEmpty());
        widths.set({40, 0, 120});
        QSettings& s = SettingsCommon::openSettings();
        QCOMPARE(s.value(QStringLiteral("RoundtripTest/widths/size")).toInt(), 3);
        QCOMPARE(s.value(QStringLiteral("RoundtripTest/widths/3/width")).toInt(), 120);
        QCOMPARE(widths(), QList<int>({40, 0, 120}));
        s.remove(QStringLiteral("RoundtripTest"));
    }

    /// StructListSetting: CSA の接続先の履歴が従来の配列の形式で読み書きできること
    void structListSettingKeepsCsaHistoryFormat()
    {
        QSettings& s = SettingsCommon::openSettings();
        s.remove(QStringLiteral("CsaServerHistory"));
        // 以前の版が書いた形式（port・useTls のない要素は既定値で読む）
        s.beginWriteArray(QStringLiteral("CsaServerHistory"), 2);
        s.setArrayIndex(0);
        s.setValue(QStringLiteral("host"), QStringLiteral("wdoor.c.u-tokyo.ac.jp"));
        s.setValue(QStringLiteral("port"), 4081);
        s.setValue(QStringLiteral("useTls"), true);
        s.setValue(QStringLiteral("id"), QStringLiteral("alice"));
        s.setArrayIndex(1);
        s.setValue(QStringLiteral("host"), QStringLiteral("localhost"));
        s.endArray();

        const QList<NetworkSettings::CsaServerHistoryEntry> history = NetworkSettings::csaServerHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).host, QStringLiteral("wdoor.c.u-tokyo.ac.jp"));
        QCOMPARE(history.at(0).useTls, true);
        QCOMPARE(history.at(0).id, QStringLiteral("alice"));
        QCOMPARE(history.at(1).host, QStringLiteral("localhost"));
        QCOMPARE(history.at(1).port, 4081);
        QCOMPARE(history.at(1).useTls, false);

        NetworkSettings::csaServerHistory.set({history.at(1)});
        QCOMPARE(NetworkSettings::csaServerHistory(), QList<NetworkSettings::CsaServerHistoryEntry>({history.at(1)}));
        s.remove(QStringLiteral("CsaServerHistory"));
    }

    // ========================================
    // AppSettings: UI状態
    // ========================================

    void appSettings_toolbarVisible()
    {
        AppSettings::toolbarVisible.set(false);
        QCOMPARE(AppSettings::toolbarVisible(), false);

        AppSettings::toolbarVisible.set(true);
        QCOMPARE(AppSettings::toolbarVisible(), true);
    }

    void appSettings_pieceSoundEnabled()
    {
        AppSettings::pieceSoundEnabled.set(false);
        QCOMPARE(AppSettings::pieceSoundEnabled(), false);

        AppSettings::pieceSoundEnabled.set(true);
        QCOMPARE(AppSettings::pieceSoundEnabled(), true);
    }

    void appSettings_pieceSoundVolume()
    {
        // 既定値は 30 %（標準の駒音はこの音量を前提に調整されている）
        QCOMPARE(AppSettings::pieceSoundVolume(), 30);

        AppSettings::pieceSoundVolume.set(35);
        QCOMPARE(AppSettings::pieceSoundVolume(), 35);

        // 範囲外は 0〜100 に丸める
        AppSettings::pieceSoundVolume.set(150);
        QCOMPARE(AppSettings::pieceSoundVolume(), 100);
        AppSettings::pieceSoundVolume.set(-5);
        QCOMPARE(AppSettings::pieceSoundVolume(), 0);

        AppSettings::pieceSoundVolume.set(100);
        QCOMPARE(AppSettings::pieceSoundVolume(), 100);
    }

    void appSettings_pieceSoundTone()
    {
        PieceSoundTone tone;
        tone.pitchSemitones = -4;
        tone.lowDb = 3;
        tone.midDb = -6;
        tone.highDb = 9;
        AppSettings::pieceSoundTone.set(tone);
        QVERIFY(AppSettings::pieceSoundTone() == tone);

        // 範囲外は ±12 に丸める
        tone.pitchSemitones = 30;
        tone.highDb = -40;
        AppSettings::pieceSoundTone.set(tone);
        QCOMPARE(AppSettings::pieceSoundTone().pitchSemitones, PieceSoundTone::kMaxSemitones);
        QCOMPARE(AppSettings::pieceSoundTone().highDb, -PieceSoundTone::kMaxGainDb);

        AppSettings::pieceSoundTone.set(PieceSoundTone{});
        QVERIFY(AppSettings::pieceSoundTone().isDefault());
    }

    void appSettings_language()
    {
        AppSettings::language.set(QStringLiteral("en"));
        QCOMPARE(AppSettings::language(), QStringLiteral("en"));

        AppSettings::language.set(QStringLiteral("ja_JP"));
        QCOMPARE(AppSettings::language(), QStringLiteral("ja_JP"));
    }

    void appSettings_appearanceSection()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.remove(SettingsKeys::kAppearanceSection);
        QCOMPARE(AppSettings::appearanceSection(), QStringLiteral("pieces"));
        // 背景タブの追加前に保存したタブの番号も、同じ項目を開くよう読み替える
        const QStringList legacyIds{QStringLiteral("pieces"), QStringLiteral("board"), QStringLiteral("stand"),
                                    QStringLiteral("information"), QStringLiteral("details")};
        for (int legacy = 0; legacy <= 4; ++legacy) {
            settings.setValue(SettingsKeys::kAppearanceSection, legacy);
            QCOMPARE(AppSettings::appearanceSection(), legacyIds.at(legacy));
        }
        settings.setValue(SettingsKeys::kAppearanceSection, 9);
        QCOMPARE(AppSettings::appearanceSection(), QStringLiteral("pieces"));
        AppSettings::setAppearanceSection(QStringLiteral("background"));
        settings.sync();
        const QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kAppearanceSection).toString(), QStringLiteral("background"));
        QCOMPARE(AppSettings::appearanceSection(), QStringLiteral("background"));
    }

    void appSettings_boardVisuals()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.remove(QStringLiteral("BoardVisuals"));
        QVERIFY(AppSettings::boardVisuals() == BoardVisuals{});
        settings.setValue(SettingsKeys::kBoardWoodGrain, false);
        QVERIFY(!AppSettings::boardVisuals().standWoodGrain);
        AppSettings::setBoardVisuals({false, false, 94});
        settings.sync();
        const QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(SettingsKeys::kBoardWoodGrain).toBool(), false);
        QCOMPARE(restored.value(SettingsKeys::kStandWoodGrain).toBool(), true);
        QCOMPARE(restored.value(SettingsKeys::kBoardPieceShadow).toBool(), false);
        QCOMPARE(restored.value(SettingsKeys::kBoardPieceScale).toInt(), 94);
        QVERIFY(AppSettings::boardVisuals() == (BoardVisuals{false, false, 94}));
        settings.setValue(SettingsKeys::kBoardPieceScale, 10000);
        QCOMPARE(AppSettings::boardVisuals().pieceScale, 112);
        settings.setValue(SettingsKeys::kBoardPieceScale, -3);
        QCOMPARE(AppSettings::boardVisuals().pieceScale, 90);
        AppSettings::setBoardVisuals(BoardVisuals{});
    }

    void appSettings_boardColors()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.remove(QStringLiteral("BoardColors"));
        const BoardColors defaults;
        QVERIFY(AppSettings::boardColors() == defaults);
        const BoardColors custom{QColor("#182838"), QColor("#c0d8d0"),
                                 QColor("#6a8494"), QColor("#193b45")};
        AppSettings::setBoardColors(custom);
        QVERIFY(AppSettings::boardColors() == custom);
        settings.sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(restored.value(QStringLiteral("BoardColors/background")).toString(), custom.background.name());
        QCOMPARE(restored.value(QStringLiteral("BoardColors/board")).toString(), custom.board.name());
        QCOMPARE(restored.value(QStringLiteral("BoardColors/stand")).toString(), custom.stand.name());
        QCOMPARE(restored.value(QStringLiteral("BoardColors/grid")).toString(), custom.grid.name());

        settings.setValue(QStringLiteral("BoardColors/grid"), QStringLiteral("invalid-color"));
        auto expected = custom;
        expected.grid = defaults.grid;
        QVERIFY(AppSettings::boardColors() == expected);
        expected.board = QColor();
        expected.stand.setAlpha(32);
        AppSettings::setBoardColors(expected);
        expected.board = defaults.board;
        expected.stand.setAlpha(255);
        QVERIFY(AppSettings::boardColors() == expected);
        AppSettings::setBoardColors(defaults);

        AppSettings::boardColorDialogSize.set(QSize(600, 380));
        QCOMPARE(AppSettings::boardColorDialogSize(), QSize(600, 380));
        AppSettings::boardColorPickerSize.set(QSize(700, 450));
        QCOMPARE(AppSettings::boardColorPickerSize(), QSize(700, 450));
    }

    void appSettings_boardInformationColors()
    {
        const BoardColors defaults;
        BoardColors custom;
        custom.cardBackground = QColor("#d9e3f0");
        custom.cardBorder = QColor("#758291");
        custom.activeCardBorder = QColor("#284a75");
        custom.turnBackground = QColor("#49658a");
        custom.turnBorder = QColor("#183459");
        custom.turnText = QColor("#fff4ca");
        custom.nameBackground = QColor(241, 221, 184, 128);
        custom.nameBorder = QColor("#967049");
        custom.nameText = QColor("#41325d");
        custom.clockBackground = QColor("#e8ebdd");
        custom.clockBorder = QColor("#687950");
        custom.clockText = QColor("#254a3c");
        custom.clockWarningText = QColor("#926129");
        custom.clockCriticalText = QColor("#a42d64");
        AppSettings::setBoardColors(custom);
        QVERIFY(AppSettings::boardColors() == custom);
        auto& settings = SettingsCommon::openSettings();
        settings.sync();
        QSettings restored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(QColor(restored.value(QStringLiteral("BoardColors/nameBackground")).toString()), custom.nameBackground);
        QCOMPARE(QColor(restored.value(QStringLiteral("BoardColors/clockCriticalText")).toString()), custom.clockCriticalText);

        settings.setValue(QStringLiteral("BoardColors/turnText"), QStringLiteral("invalid-color"));
        custom.turnText = defaults.turnText;
        QVERIFY(AppSettings::boardColors() == custom);
        custom.nameBackground = QColor(Qt::transparent);
        custom.cardBackground.setAlpha(32);
        AppSettings::setBoardColors(custom);
        custom.cardBackground.setAlpha(255);
        QVERIFY(AppSettings::boardColors() == custom);
        AppSettings::setBoardColors(defaults);
        AppSettings::boardColorDialogTab.set(4);
        QCOMPARE(AppSettings::boardColorDialogTab(), 4);
        AppSettings::boardColorDialogTab.set(0);
    }

    void appSettings_menuWindow()
    {
        QSize testSize(900, 700);
        AppSettings::menuWindowSize.set(testSize);
        QCOMPARE(AppSettings::menuWindowSize(), testSize);

        AppSettings::menuWindowButtonSize.set(64);
        QCOMPARE(AppSettings::menuWindowButtonSize(), 64);

        AppSettings::menuWindowFontSize.set(14);
        QCOMPARE(AppSettings::menuWindowFontSize(), 14);

        QStringList favorites = {QStringLiteral("item1"), QStringLiteral("item2")};
        AppSettings::menuWindowFavorites.set(favorites);
        QCOMPARE(AppSettings::menuWindowFavorites(), favorites);
    }

    // ========================================
    // AnalysisSettings: 解析設定
    // ========================================

    void analysisSettings_evalChart()
    {
        EvaluationChartSettings chart;
        chart.automatic = false;
        chart.yLimit = 3000;
        chart.xLimit = 300;
        chart.xInterval = 20;
        chart.labelFontSize = 12;
        AnalysisSettings::evalChart.set(chart);
        QVERIFY(AnalysisSettings::evalChart() == chart);
        // 以前の版と同じキーに1項目ずつ保存する
        QSettings& settings = SettingsCommon::openSettings();
        QCOMPARE(settings.value(QStringLiteral("EvalChart/yLimit")).toInt(), 3000);
        QCOMPARE(settings.value(QStringLiteral("EvalChart/automaticRange")).toBool(), false);
        // 範囲外の値は読むときに範囲に収める
        settings.setValue(QStringLiteral("EvalChart/yLimit"), 5);
        settings.setValue(QStringLiteral("EvalChart/labelFontSize"), 99);
        QCOMPARE(AnalysisSettings::evalChart().yLimit, EvaluationChartSettings::kMinYLimit);
        QCOMPARE(AnalysisSettings::evalChart().labelFontSize, EvaluationChartSettings::kMaxFontSize);
    }

    void analysisSettings_fontSizes()
    {
        AnalysisSettings::usiLogFontSize.set(11);
        QCOMPARE(AnalysisSettings::usiLogFontSize(), 11);

        AnalysisSettings::thinkingFontSize.set(13);
        QCOMPARE(AnalysisSettings::thinkingFontSize(), 13);

        AnalysisSettings::kifuAnalysisFontSize.set(14);
        QCOMPARE(AnalysisSettings::kifuAnalysisFontSize(), 14);

        AnalysisSettings::considerationFontSize.set(15);
        QCOMPARE(AnalysisSettings::considerationFontSize(), 15);
    }

    void analysisSettings_kifuAnalysis()
    {
        AnalysisSettings::kifuAnalysisByoyomiSec.set(30);
        QCOMPARE(AnalysisSettings::kifuAnalysisByoyomiSec(), 30);

        AnalysisSettings::kifuAnalysisFullRange.set(true);
        QCOMPARE(AnalysisSettings::kifuAnalysisFullRange(), true);

        AnalysisSettings::kifuAnalysisFullRange.set(false);
        QCOMPARE(AnalysisSettings::kifuAnalysisFullRange(), false);

        AnalysisSettings::kifuAnalysisStartPly.set(10);
        QCOMPARE(AnalysisSettings::kifuAnalysisStartPly(), 10);

        AnalysisSettings::kifuAnalysisEndPly.set(50);
        QCOMPARE(AnalysisSettings::kifuAnalysisEndPly(), 50);

        QSize dialogSize(850, 650);
        AnalysisSettings::kifuAnalysisDialogSize.set(dialogSize);
        QCOMPARE(AnalysisSettings::kifuAnalysisDialogSize(), dialogSize);
    }

    /// エンジンの選択は名前で保存し、登録の順番が変わっても同じエンジンを選ぶ。
    /// 以前の版が保存した番号・パスも読み、保存し直すと名前に置き換える
    void engineSelection_storedByName()
    {
        const QList<EngineEntry> entries{{QStringLiteral("A"), QStringLiteral("/engines/a"), {}},
                                         {QStringLiteral("B"), QStringLiteral("/engines/b"), {}},
                                         {QStringLiteral("C"), QStringLiteral("/engines/c"), {}}};
        const RegisteredEngines engines(entries);
        const RegisteredEngines reordered({entries.at(2), entries.at(0), entries.at(1)});
        QSettings& s = SettingsCommon::openSettings();
        for (const auto& keys : {EngineListSettings::considerationSelection(), EngineListSettings::kifuAnalysisSelection(),
                                 EngineListSettings::tsumeSearchSelection(),
                                 EngineListSettings::tsumeshogiGeneratorSelection()}) {
            s.remove(QString::fromLatin1(keys.name));
            if (keys.legacyIndex) s.setValue(keys.legacyIndex, 1);
            if (keys.legacyPath) s.setValue(keys.legacyPath, QStringLiteral("/engines/b"));
            QCOMPARE(EngineListSettings::restoreSelection(keys, engines), 1);  // 以前の版の保存（B）

            EngineListSettings::saveSelection(keys, QStringLiteral("B"));
            if (keys.legacyIndex) QVERIFY(!s.contains(keys.legacyIndex));
            if (keys.legacyPath) QVERIFY(!s.contains(keys.legacyPath));
            QCOMPARE(EngineListSettings::restoreSelection(keys, reordered), 2);  // 並びが変わっても B

            EngineListSettings::saveSelection(keys, QStringLiteral("removed"));
            QCOMPARE(EngineListSettings::restoreSelection(keys, engines), -1);
        }
    }

    void analysisSettings_consideration()
    {
        AnalysisSettings::considerationUnlimitedTime.set(true);
        QCOMPARE(AnalysisSettings::considerationUnlimitedTime(), true);

        AnalysisSettings::considerationByoyomiSec.set(60);
        QCOMPARE(AnalysisSettings::considerationByoyomiSec(), 60);

        AnalysisSettings::considerationMultiPV.set(4);
        QCOMPARE(AnalysisSettings::considerationMultiPV(), 4);
    }

    void analysisSettings_columnWidths()
    {
        QList<int> widths = {100, 200, 150};
        AnalysisSettings::setEngineInfoColumnWidths(AnalysisSettings::ThinkingTable::Engine1, widths);
        QCOMPARE(AnalysisSettings::engineInfoColumnWidths(AnalysisSettings::ThinkingTable::Engine1), widths);

        QList<int> thinkWidths = {80, 120, 200, 50};
        AnalysisSettings::setThinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine2, thinkWidths);
        QCOMPARE(AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine2), thinkWidths);
    }

    // ========================================
    // GameSettings: 対局設定
    // ========================================

    void gameSettings_kifuDirectory()
    {
        QString dir = QStringLiteral("/home/test/kifu");
        GameSettings::lastKifuDirectory.set(dir);
        QCOMPARE(GameSettings::lastKifuDirectory(), dir);

        QString saveDir = QStringLiteral("/home/test/kifu/save");
        GameSettings::lastKifuSaveDirectory.set(saveDir);
        QCOMPARE(GameSettings::lastKifuSaveDirectory(), saveDir);
    }

    void gameSettings_recordPane()
    {
        GameSettings::kifuPaneFontSize.set(12);
        QCOMPARE(GameSettings::kifuPaneFontSize(), 12);

        GameSettings::kifuTimeColumnVisible.set(false);
        QCOMPARE(GameSettings::kifuTimeColumnVisible(), false);

        GameSettings::kifuTimeColumnVisible.set(true);
        QCOMPARE(GameSettings::kifuTimeColumnVisible(), true);

        GameSettings::kifuBookmarkColumnVisible.set(false);
        QCOMPARE(GameSettings::kifuBookmarkColumnVisible(), false);

        GameSettings::kifuCommentColumnVisible.set(false);
        QCOMPARE(GameSettings::kifuCommentColumnVisible(), false);
    }

    void gameSettings_commentAndGameInfo()
    {
        GameSettings::commentFontSize.set(11);
        QCOMPARE(GameSettings::commentFontSize(), 11);

        GameSettings::gameInfoFontSize.set(13);
        QCOMPARE(GameSettings::gameInfoFontSize(), 13);
    }

    void gameSettings_dialogSizes()
    {
        QSize startSize(1200, 700);
        GameSettings::startGameDialogSize.set(startSize);
        QCOMPARE(GameSettings::startGameDialogSize(), startSize);

        GameSettings::startGameDialogFontSize.set(11);
        QCOMPARE(GameSettings::startGameDialogFontSize(), 11);

        QSize pasteSize(700, 600);
        GameSettings::kifuPasteDialogSize.set(pasteSize);
        QCOMPARE(GameSettings::kifuPasteDialogSize(), pasteSize);

        GameSettings::kifuPasteDialogFontSize.set(12);
        QCOMPARE(GameSettings::kifuPasteDialogFontSize(), 12);

        QSize jishogiSize(300, 350);
        GameSettings::jishogiScoreDialogSize.set(jishogiSize);
        QCOMPARE(GameSettings::jishogiScoreDialogSize(), jishogiSize);

        GameSettings::jishogiScoreFontSize.set(14);
        QCOMPARE(GameSettings::jishogiScoreFontSize(), 14);
    }

    void gameSettings_sfenCollection()
    {
        QStringList recentFiles = {
            QStringLiteral("/path/to/file1.sfen"),
            QStringLiteral("/path/to/file2.sfen"),
        };
        GameSettings::sfenCollectionRecentFiles.set(recentFiles);
        QCOMPARE(GameSettings::sfenCollectionRecentFiles(), recentFiles);

        GameSettings::sfenCollectionSquareSize.set(60);
        QCOMPARE(GameSettings::sfenCollectionSquareSize(), 60);

        GameSettings::sfenCollectionLastDirectory.set(QStringLiteral("/tmp/sfen"));
        QCOMPARE(GameSettings::sfenCollectionLastDirectory(), QStringLiteral("/tmp/sfen"));
    }

    // ========================================
    // NetworkSettings: CSAネットワーク設定
    // ========================================

    void networkSettings_fontSizes()
    {
        NetworkSettings::csaLogFontSize.set(12);
        QCOMPARE(NetworkSettings::csaLogFontSize(), 12);

        NetworkSettings::csaWaitingDialogFontSize.set(14);
        QCOMPARE(NetworkSettings::csaWaitingDialogFontSize(), 14);

        NetworkSettings::csaGameDialogFontSize.set(11);
        QCOMPARE(NetworkSettings::csaGameDialogFontSize(), 11);
    }

    void networkSettings_dialogSizes()
    {
        QSize logSize(600, 500);
        NetworkSettings::csaLogWindowSize.set(logSize);
        QCOMPARE(NetworkSettings::csaLogWindowSize(), logSize);

        QSize gameSize(500, 400);
        NetworkSettings::csaGameDialogSize.set(gameSize);
        QCOMPARE(NetworkSettings::csaGameDialogSize(), gameSize);

        QSize waitingSize(460, 300);
        NetworkSettings::csaWaitingDialogSize.set(waitingSize);
        QCOMPARE(NetworkSettings::csaWaitingDialogSize(), waitingSize);
    }

    /// 通信対局ダイアログの詳細設定（接続の維持・対局数・自動保存・盤反転・shogi-server）
    void networkSettings_csaAdvancedOptions()
    {
        // 既定値
        QVERIFY(NetworkSettings::csaKeepAliveEnabled());
        QCOMPARE(NetworkSettings::csaKeepAliveIntervalSec(), 40);
        QVERIFY(NetworkSettings::csaTcpKeepAliveEnabled());
        QCOMPARE(NetworkSettings::csaThinkingMarginSec(), 1);
        QCOMPARE(NetworkSettings::csaGameCountMode(), 0);
        QCOMPARE(NetworkSettings::csaGameCount(), 2);
        QVERIFY(!NetworkSettings::csaReconnectAfterGame());
        QVERIFY(!NetworkSettings::csaAutoSaveEnabled());
        QVERIFY(NetworkSettings::csaAutoSaveDir().isEmpty());
        QCOMPARE(NetworkSettings::csaAutoSaveFormat(), QStringLiteral("kif"));
        QVERIFY(NetworkSettings::csaAutoFlipBoard());
        QVERIFY(NetworkSettings::csaGameName().isEmpty());
        QCOMPARE(NetworkSettings::csaTurnPreference(), 0);

        NetworkSettings::csaKeepAliveEnabled.set(false);
        NetworkSettings::csaKeepAliveIntervalSec.set(45);
        NetworkSettings::csaTcpKeepAliveEnabled.set(false);
        NetworkSettings::csaThinkingMarginSec.set(3);
        NetworkSettings::csaGameCountMode.set(1);
        NetworkSettings::csaGameCount.set(5);
        NetworkSettings::csaReconnectAfterGame.set(true);
        NetworkSettings::csaAutoSaveEnabled.set(true);
        NetworkSettings::csaAutoSaveDir.set(QStringLiteral("/tmp/csa-records"));
        NetworkSettings::csaAutoSaveFormat.set(QStringLiteral("csa"));
        NetworkSettings::csaAutoFlipBoard.set(false);
        NetworkSettings::csaGameName.set(QStringLiteral("floodgate-300-10F"));
        NetworkSettings::csaTurnPreference.set(2);

        QVERIFY(!NetworkSettings::csaKeepAliveEnabled());
        QCOMPARE(NetworkSettings::csaKeepAliveIntervalSec(), 45);
        QVERIFY(!NetworkSettings::csaTcpKeepAliveEnabled());
        QCOMPARE(NetworkSettings::csaThinkingMarginSec(), 3);
        QCOMPARE(NetworkSettings::csaGameCountMode(), 1);
        QCOMPARE(NetworkSettings::csaGameCount(), 5);
        QVERIFY(NetworkSettings::csaReconnectAfterGame());
        QVERIFY(NetworkSettings::csaAutoSaveEnabled());
        QCOMPARE(NetworkSettings::csaAutoSaveDir(), QStringLiteral("/tmp/csa-records"));
        QCOMPARE(NetworkSettings::csaAutoSaveFormat(), QStringLiteral("csa"));
        QVERIFY(!NetworkSettings::csaAutoFlipBoard());
        QCOMPARE(NetworkSettings::csaGameName(), QStringLiteral("floodgate-300-10F"));
        QCOMPARE(NetworkSettings::csaTurnPreference(), 2);
    }

    // ========================================
    // EngineDialogSettings: エンジン設定
    // ========================================

    void engineDialogSettings()
    {
        EngineDialogSettings::engineSettingsFontSize.set(13);
        QCOMPARE(EngineDialogSettings::engineSettingsFontSize(), 13);

        QSize settingsSize(900, 600);
        EngineDialogSettings::engineSettingsDialogSize.set(settingsSize);
        QCOMPARE(EngineDialogSettings::engineSettingsDialogSize(), settingsSize);

        EngineDialogSettings::engineRegistrationFontSize.set(11);
        QCOMPARE(EngineDialogSettings::engineRegistrationFontSize(), 11);

        QSize regSize(800, 550);
        EngineDialogSettings::engineRegistrationDialogSize.set(regSize);
        QCOMPARE(EngineDialogSettings::engineRegistrationDialogSize(), regSize);
    }

    // ========================================
    // JosekiSettings: 定跡設定
    // ========================================

    void josekiSettings_window()
    {
        JosekiSettings::josekiWindowFontSize.set(12);
        QCOMPARE(JosekiSettings::josekiWindowFontSize(), 12);

        QSize josekiSize(1100, 800);
        JosekiSettings::josekiWindowSize.set(josekiSize);
        QCOMPARE(JosekiSettings::josekiWindowSize(), josekiSize);

        JosekiSettings::josekiWindowAutoLoadEnabled.set(true);
        QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), true);

        JosekiSettings::josekiWindowAutoLoadEnabled.set(false);
        QCOMPARE(JosekiSettings::josekiWindowAutoLoadEnabled(), false);

        JosekiSettings::josekiWindowDisplayEnabled.set(true);
        QCOMPARE(JosekiSettings::josekiWindowDisplayEnabled(), true);

        JosekiSettings::josekiWindowSfenDetailVisible.set(true);
        QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), true);

        JosekiSettings::josekiWindowSfenDetailVisible.set(false);
        QCOMPARE(JosekiSettings::josekiWindowSfenDetailVisible(), false);

        QString lastPath = QStringLiteral("/home/test/joseki/book.db");
        JosekiSettings::josekiWindowLastFilePath.set(lastPath);
        QCOMPARE(JosekiSettings::josekiWindowLastFilePath(), lastPath);

        QStringList recentFiles = {
            QStringLiteral("/path/to/joseki1.db"),
            QStringLiteral("/path/to/joseki2.db"),
        };
        JosekiSettings::josekiWindowRecentFiles.set(recentFiles);
        QCOMPARE(JosekiSettings::josekiWindowRecentFiles(), recentFiles);

        QList<int> colWidths = {50, 120, 80, 200};
        JosekiSettings::josekiWindowColumnWidths.set(colWidths);
        QCOMPARE(JosekiSettings::josekiWindowColumnWidths(), colWidths);
    }

    void josekiSettings_dialogs()
    {
        JosekiSettings::josekiMoveDialogFontSize.set(11);
        QCOMPARE(JosekiSettings::josekiMoveDialogFontSize(), 11);

        QSize moveSize(500, 400);
        JosekiSettings::josekiMoveDialogSize.set(moveSize);
        QCOMPARE(JosekiSettings::josekiMoveDialogSize(), moveSize);

        JosekiSettings::josekiMergeDialogFontSize.set(12);
        QCOMPARE(JosekiSettings::josekiMergeDialogFontSize(), 12);

        QSize mergeSize(600, 500);
        JosekiSettings::josekiMergeDialogSize.set(mergeSize);
        QCOMPARE(JosekiSettings::josekiMergeDialogSize(), mergeSize);
    }

    // ========================================
    // TsumeshogiSettings: 詰将棋設定
    // ========================================

    void tsumeshogiSettings()
    {
        QSize tsumSize(700, 600);
        TsumeshogiSettings::tsumeshogiGeneratorDialogSize.set(tsumSize);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorDialogSize(), tsumSize);

        TsumeshogiSettings::tsumeshogiGeneratorFontSize.set(13);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorFontSize(), 13);

        TsumeshogiSettings::tsumeshogiGeneratorTargetMoves.set(7);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorTargetMoves(), 7);

        TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces.set(5);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces(), 5);

        TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces.set(3);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces(), 3);

        TsumeshogiSettings::tsumeshogiGeneratorAttackRange.set(4);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorAttackRange(), 4);

        TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec.set(120);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec(), 120);

        TsumeshogiSettings::tsumeshogiGeneratorMaxPositions.set(1000);
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions(), 1000);

        TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory.set(QStringLiteral("/tmp/tsume"));
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(), QStringLiteral("/tmp/tsume"));

        TsumeshogiSettings::collectionDialogSize.set(QSize(1200, 850));
        QCOMPARE(TsumeshogiSettings::collectionDialogSize(), QSize(1200, 850));
        TsumeshogiSettings::CollectionPreferences collection;
        collection.lastFile = QStringLiteral("/tmp/problems.sfen");
        collection.pageSize = 50;
        collection.page = 7;
        collection.filter = 3;
        collection.timeoutSec = 30;
        TsumeshogiSettings::collectionPreferences.set(collection);
        const auto restored = TsumeshogiSettings::collectionPreferences();
        QCOMPARE(restored.lastFile, collection.lastFile);
        QCOMPARE(restored.pageSize, collection.pageSize);
        QCOMPARE(restored.page, collection.page);
        QCOMPARE(restored.filter, collection.filter);
        QCOMPARE(restored.timeoutSec, collection.timeoutSec);
    }

    // ========================================
    // DockSettings: ドック設定
    // ========================================

    void dockSettings_lockState()
    {
        DockSettings::docksLocked.set(true);
        QCOMPARE(DockSettings::docksLocked(), true);

        DockSettings::docksLocked.set(false);
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

        DockSettings::startupDockLayoutName.set(QStringLiteral("TestLayout"));
        QCOMPARE(DockSettings::startupDockLayoutName(), QStringLiteral("TestLayout"));

        DockSettings::deleteDockLayout(QStringLiteral("TestLayout"));
        QByteArray deleted = DockSettings::loadDockLayout(QStringLiteral("TestLayout"));
        QVERIFY(deleted.isEmpty());
    }
};

QTEST_MAIN(Tst_SettingsRoundtrip)
#include "tst_settings_roundtrip.moc"
