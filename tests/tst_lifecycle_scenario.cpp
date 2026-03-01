/// @file tst_lifecycle_scenario.cpp
/// @brief アプリケーションライフサイクル全体の構造的シナリオテスト
///
/// 「初期化→対局開始→指し手→保存→リセット→再対局」の完全フローを
/// ソースコード構造検証方式で検証する。
/// 各フェーズの呼び出しチェーンが正しく接続されていることを確認する。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestLifecycleScenario : public QObject
{
    Q_OBJECT

private:
    static QString readSourceFile(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&file).readAll();
    }

    static QStringList readSourceLines(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        QStringList lines;
        QTextStream in(&file);
        while (!in.atEnd())
            lines << in.readLine();
        return lines;
    }

    static QPair<int, int> findFunctionBody(const QStringList& lines, const QString& signature)
    {
        const int sz = int(lines.size());
        int sigLine = -1;
        for (int i = 0; i < sz; ++i) {
            if (lines[i].contains(signature)) {
                sigLine = i;
                break;
            }
        }
        if (sigLine < 0)
            return {-1, -1};

        int braceStart = -1;
        for (int i = sigLine; i < sz; ++i) {
            if (lines[i].contains(QLatin1Char('{'))) {
                braceStart = i;
                break;
            }
        }
        if (braceStart < 0)
            return {-1, -1};

        int depth = 0;
        for (int i = braceStart; i < sz; ++i) {
            for (const QChar& ch : lines[i]) {
                if (ch == QLatin1Char('{'))
                    ++depth;
                else if (ch == QLatin1Char('}'))
                    --depth;
            }
            if (depth == 0)
                return {braceStart, i};
        }
        return {-1, -1};
    }

    static QString bodyText(const QStringList& lines, const QPair<int, int>& range)
    {
        QStringList body;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            body << lines[i];
        return body.join(QLatin1Char('\n'));
    }

private slots:
    // ================================================================
    // Phase 1: 初期化フロー
    //   MainWindow → LifecyclePipeline::runStartup() → 8段階
    // ================================================================

    /// MainWindow コンストラクタが LifecyclePipeline を使って初期化すること
    void phase1_mainWindowDelegatesToPipeline()
    {
        const QString mw = readSourceFile(QStringLiteral("src/app/mainwindow.cpp"));
        QVERIFY2(!mw.isEmpty(), "Failed to read mainwindow.cpp");

        QVERIFY2(mw.contains(QStringLiteral("MainWindowLifecyclePipeline")),
                  "MainWindow must use LifecyclePipeline");
        QVERIFY2(mw.contains(QStringLiteral("m_pipeline->runStartup()")),
                  "Constructor must call runStartup()");
    }

    /// runStartup() が createFoundationObjects → finalizeAndConfigureUi を順序通り呼ぶこと
    void phase1_startupStepsAreOrdered()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runStartup()"));
        QVERIFY2(range.first >= 0, "runStartup() not found");

        const QString body = bodyText(lines, range);

        // 最初と最後のステップの順序を検証
        const auto firstStep = body.indexOf(QStringLiteral("createFoundationObjects"));
        const auto lastStep = body.indexOf(QStringLiteral("finalizeAndConfigureUi"));
        QVERIFY2(firstStep >= 0, "createFoundationObjects not found in runStartup");
        QVERIFY2(lastStep >= 0, "finalizeAndConfigureUi not found in runStartup");
        QVERIFY2(firstStep < lastStep,
                  "createFoundationObjects must precede finalizeAndConfigureUi");
    }

    /// connectSignals ステップが SignalRouter を初期化すること
    void phase1_connectSignalsUsesSignalRouter()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::connectSignals()"));
        QVERIFY2(range.first >= 0, "connectSignals() not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("SignalRouter")),
                  "connectSignals must reference SignalRouter");
    }

    // ================================================================
    // Phase 2: 対局開始フロー
    //   GSO::initializeGame() → GSC → invokeStartGame() → MC::prepareAndStartGame()
    // ================================================================

    /// initializeGame が GameStartCoordinator に委譲すること
    void phase2_initializeGameDelegatesToGSC()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read GSO source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::initializeGame()"));
        QVERIFY2(range.first >= 0, "initializeGame not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("ensureGameStartCoordinator")),
                  "Must lazy-init GameStartCoordinator");
        QVERIFY2(body.contains(QStringLiteral("initializeGame")),
                  "Must delegate to GameStartCoordinator::initializeGame");
    }

    /// invokeStartGame が MatchCoordinator::prepareAndStartGame に委譲すること
    void phase2_invokeStartGameDelegatesToMC()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read GSO source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::invokeStartGame()"));
        QVERIFY2(range.first >= 0, "invokeStartGame not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("prepareAndStartGame")),
                  "Must call mc->prepareAndStartGame()");
    }

    /// MatchCoordinator::prepareAndStartGame が GameStartOrchestrator に委譲すること
    void phase2_mcDelegatesToGameStartOrchestrator()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/game/matchcoordinator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read MC source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MatchCoordinator::prepareAndStartGame"));
        QVERIFY2(range.first >= 0, "prepareAndStartGame not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("m_gameStartOrchestrator")),
                  "Must delegate to GameStartOrchestrator");
    }

    // ================================================================
    // Phase 3: 指し手処理フロー
    //   MatchCoordinator → MatchTurnHandler → GC 更新 → 棋譜追加
    // ================================================================

    /// MatchCoordinator が MatchTurnHandler を所有すること
    void phase3_mcOwnsMatchTurnHandler()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/game/matchcoordinator.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read MC header");

        QVERIFY2(header.contains(QStringLiteral("MatchTurnHandler")),
                  "MatchCoordinator must reference MatchTurnHandler");
    }

    /// MatchTurnHandler が指し手処理の核となるメソッドを持つこと
    void phase3_turnHandlerHasMoveProcessing()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/game/matchturnhandler.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read MTH header");

        QVERIFY2(header.contains(QStringLiteral("onHumanMove"))
                     || header.contains(QStringLiteral("handleHumanMove"))
                     || header.contains(QStringLiteral("processMove")),
                  "MatchTurnHandler must have move processing method");
    }

    /// GameStartOrchestrator の Hooks に棋譜追加コールバックがあること
    void phase3_hooksHaveKifuAppend()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/game/matchcoordinator.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read MC header");

        QVERIFY2(header.contains(QStringLiteral("appendKifuLine"))
                     || header.contains(QStringLiteral("appendKifu")),
                  "Hooks must have kifu append callback");
    }

    // ================================================================
    // Phase 4: 保存フロー
    //   KifuExportController → format変換 → ファイル書き出し
    // ================================================================

    /// KifuExportController に saveToFile と overwriteFile メソッドがあること
    void phase4_exportControllerHasSaveMethods()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/kifu/kifuexportcontroller.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read export controller header");

        QVERIFY2(header.contains(QStringLiteral("saveToFile")),
                  "Must have saveToFile method");
        QVERIFY2(header.contains(QStringLiteral("overwriteFile")),
                  "Must have overwriteFile method");
    }

    /// saveToFile が ExportContext を構築して形式変換すること
    void phase4_saveToFileBuildContext()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/kifu/kifuexportcontroller.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read export controller source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("KifuExportController::saveToFile"));
        QVERIFY2(range.first >= 0, "saveToFile not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("buildExportContext"))
                     || body.contains(QStringLiteral("ExportContext"))
                     || body.contains(QStringLiteral("resolveUsiMoves")),
                  "saveToFile must build export context or resolve moves");
    }

    /// KifuExportClipboard にクリップボード出力メソッドがあること
    void phase4_clipboardExportExists()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/kifu/kifuexportclipboard.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read clipboard export header");

        QVERIFY2(header.contains(QStringLiteral("copyKifToClipboard"))
                     || header.contains(QStringLiteral("copyToClipboard")),
                  "Must have clipboard copy method");
    }

    // ================================================================
    // Phase 5: リセットフロー
    //   SessionLifecycleCoordinator::resetToInitialState() → 全状態クリア
    // ================================================================

    /// SessionLifecycleCoordinator に resetToInitialState メソッドがあること
    void phase5_slcHasResetMethod()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/app/sessionlifecyclecoordinator.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read SLC header");

        QVERIFY2(header.contains(QStringLiteral("resetToInitialState")),
                  "Must have resetToInitialState method");
    }

    /// resetToInitialState がエンジン停止・クリーンアップ・UI リセットを行うこと
    void phase5_resetPerformsFullCleanup()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/sessionlifecyclecoordinator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read SLC source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("SessionLifecycleCoordinator::resetToInitialState"));
        QVERIFY2(range.first >= 0, "resetToInitialState not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("resetEngineState"))
                     || body.contains(QStringLiteral("destroyEngines")),
                  "Reset must stop engines");
        QVERIFY2(body.contains(QStringLiteral("resetGameState"))
                     || body.contains(QStringLiteral("PlayMode")),
                  "Reset must clear game state");
        QVERIFY2(body.contains(QStringLiteral("resetUiState"))
                     || body.contains(QStringLiteral("resetModels")),
                  "Reset must reset UI/models");
    }

    /// PreStartCleanupHandler が対局前クリーンアップを担当すること
    void phase5_preStartCleanupHandlerExists()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/game/prestartcleanuphandler.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read cleanup handler header");

        QVERIFY2(header.contains(QStringLiteral("performCleanup")),
                  "Must have performCleanup method");
    }

    // ================================================================
    // Phase 6: 再対局フロー
    //   SLC::startNewGame() → GSO::invokeStartGame() → MC::prepareAndStartGame()
    // ================================================================

    /// SessionLifecycleCoordinator::startNewGame が対局開始コールバックを呼ぶこと
    void phase6_startNewGameCallsStartCallback()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/sessionlifecyclecoordinator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read SLC source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("SessionLifecycleCoordinator::startNewGame"));
        QVERIFY2(range.first >= 0, "startNewGame not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("startGame")),
                  "startNewGame must call startGame callback");
    }

    /// 再対局が同一の invokeStartGame → prepareAndStartGame チェーンを使うこと
    void phase6_restartUsesExistingChain()
    {
        // GSO::invokeStartGame が最終的に MC::prepareAndStartGame を呼ぶことは
        // phase2_invokeStartGameDelegatesToMC で検証済み。
        // ここでは startNewGame の Deps に startGame コールバックがあることを確認。
        const QString header = readSourceFile(
            QStringLiteral("src/app/sessionlifecyclecoordinator.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read SLC header");

        QVERIFY2(header.contains(QStringLiteral("startGame")),
                  "Deps must have startGame callback for restart chain");
    }

    // ================================================================
    // Phase 7: シャットダウンフロー（リセット→終了）
    //   LifecyclePipeline::runShutdown() → 設定保存 → エンジン停止
    // ================================================================

    /// runShutdown が二重実行防止ガードを持つこと
    void phase7_shutdownHasGuard()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("m_shutdownDone")),
                  "runShutdown must have double-execution guard");
    }

    /// runShutdown がエンジン停止と設定保存を行うこと
    void phase7_shutdownSavesAndStopsEngines()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown not found");

        const QString body = bodyText(lines, range);

        // 設定保存
        QVERIFY2(body.contains(QStringLiteral("saveWindowAndBoard"))
                     || body.contains(QStringLiteral("saveDockStates"))
                     || body.contains(QStringLiteral("saveSettings")),
                  "Shutdown must save settings");

        // エンジン停止
        QVERIFY2(body.contains(QStringLiteral("destroyEngines")),
                  "Shutdown must destroy engines");
    }

    // ================================================================
    // Cross-cutting: フロー間接続の検証
    // ================================================================

    /// 終局→保存→リセットの接続: GameEndHandler が GameEndInfo シグナルを発行すること
    void crossCut_gameEndHandlerEmitsSignal()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/game/gameendhandler.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read GEH header");

        QVERIFY2(header.contains(QStringLiteral("gameEnded"))
                     || header.contains(QStringLiteral("GameEndInfo")),
                  "GameEndHandler must emit gameEnded signal with GameEndInfo");
    }

    /// GSO::onMatchGameEnded が SLC と連携すること
    void crossCut_gameEndReachesSLC()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read GSO source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onMatchGameEnded"));
        QVERIFY2(range.first >= 0, "onMatchGameEnded not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("ensureSessionLifecycleCoordinator")),
                  "Game end must reach SessionLifecycleCoordinator");
    }

    /// UiStatePolicyManager が初期状態 Idle で開始すること
    void crossCut_uiPolicyStartsIdle()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::finalizeAndConfigureUi()"));
        QVERIFY2(range.first >= 0, "finalizeAndConfigureUi not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("UiStatePolicyManager"))
                     || body.contains(QStringLiteral("Idle")),
                  "finalizeAndConfigureUi must initialize UI policy to Idle state");
    }

    /// 全フェーズの主要クラスがそれぞれの .h/.cpp ファイルに実装されていること
    void crossCut_allPhaseClassesExist()
    {
        const QStringList requiredHeaders = {
            QStringLiteral("src/app/mainwindowlifecyclepipeline.h"),
            QStringLiteral("src/app/gamesessionorchestrator.h"),
            QStringLiteral("src/app/sessionlifecyclecoordinator.h"),
            QStringLiteral("src/game/matchcoordinator.h"),
            QStringLiteral("src/game/gamestartorchestrator.h"),
            QStringLiteral("src/game/gameendhandler.h"),
            QStringLiteral("src/game/prestartcleanuphandler.h"),
            QStringLiteral("src/kifu/kifuexportcontroller.h"),
        };

        for (const QString& hdr : requiredHeaders) {
            const QString content = readSourceFile(hdr);
            QVERIFY2(!content.isEmpty(),
                      qPrintable(QStringLiteral("Required header missing: %1").arg(hdr)));
        }
    }
};

QTEST_MAIN(TestLifecycleScenario)

#include "tst_lifecycle_scenario.moc"
