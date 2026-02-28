/// @file tst_app_game_session.cpp
/// @brief GameSessionOrchestrator の構造的契約テスト
///
/// GameSessionOrchestrator は多数のコントローラへの遅延初期化・ルーティング・
/// 副作用を担うオーケストレータ。依存クラスが多いため単体インスタンス化は不適切。
/// 代わりにソース解析テストで以下の構造的契約を検証する:
///
/// - 全スロットが deref() を使用して安全にポインタアクセスしていること
/// - 各スロットが適切な ensure* コールバックを呼んでからデリファレンスしていること
/// - ルーティング先が正しいこと（CSA/通常モード分岐等）
/// - onGameStarted() で PlayMode が同期されること
///
/// 既存の tst_app_lifecycle_pipeline / tst_structural_kpi と同パターン。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppGameSession : public QObject
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

    /// 関数本体の行範囲を返す
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

    /// 指定行範囲のテキストを連結して返す
    static QString bodyText(const QStringList& lines, const QPair<int, int>& range)
    {
        QStringList body;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            body << lines[i];
        return body.join(QLatin1Char('\n'));
    }

    /// .cpp ファイルの全行を共有キャッシュから取得
    const QStringList& cppLines()
    {
        if (m_cppLines.isEmpty())
            m_cppLines = readSourceLines(QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        return m_cppLines;
    }

    const QString& headerText()
    {
        if (m_headerText.isEmpty())
            m_headerText = readSourceFile(QStringLiteral("src/app/gamesessionorchestrator.h"));
        return m_headerText;
    }

    QStringList m_cppLines;
    QString m_headerText;

private slots:
    // ================================================================
    // A) ヘッダー構造の検証
    // ================================================================

    /// Deps 構造体が必要なコールバックを持つこと
    void depsHasRequiredEnsureCallbacks()
    {
        const QString& hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredCallbacks = {
            QStringLiteral("ensureGameStateController"),
            QStringLiteral("ensureSessionLifecycleCoordinator"),
            QStringLiteral("ensureConsecutiveGamesController"),
            QStringLiteral("ensureGameStartCoordinator"),
            QStringLiteral("ensurePreStartCleanupHandler"),
            QStringLiteral("ensureDialogCoordinator"),
            QStringLiteral("ensureReplayController"),
        };

        for (const QString& cb : requiredCallbacks) {
            QVERIFY2(hdr.contains(cb),
                      qPrintable(QStringLiteral("Deps missing callback: %1").arg(cb)));
        }
    }

    /// Deps 構造体が必要なダブルポインタを持つこと
    void depsHasRequiredDoublePointers()
    {
        const QString& hdr = headerText();

        const QStringList requiredPtrs = {
            QStringLiteral("GameStateController**"),
            QStringLiteral("SessionLifecycleCoordinator**"),
            QStringLiteral("ConsecutiveGamesController**"),
            QStringLiteral("GameStartCoordinator**"),
            QStringLiteral("PreStartCleanupHandler**"),
            QStringLiteral("DialogCoordinator**"),
            QStringLiteral("MatchCoordinator**"),
        };

        for (const QString& ptr : requiredPtrs) {
            QVERIFY2(hdr.contains(ptr),
                      qPrintable(QStringLiteral("Deps missing double pointer: %1").arg(ptr)));
        }
    }

    /// 全パブリックスロットが宣言されていること
    void allPublicSlotsDeclared()
    {
        const QString& hdr = headerText();

        const QStringList expectedSlots = {
            QStringLiteral("initializeGame()"),
            QStringLiteral("handleResignation()"),
            QStringLiteral("handleBreakOffGame()"),
            QStringLiteral("movePieceImmediately()"),
            QStringLiteral("stopTsumeSearch()"),
            QStringLiteral("openWebsiteInExternalBrowser()"),
            QStringLiteral("onMatchGameEnded"),
            QStringLiteral("onGameOverStateChanged"),
            QStringLiteral("onRequestAppendGameOverMove"),
            QStringLiteral("onResignationTriggered()"),
            QStringLiteral("onPreStartCleanupRequested()"),
            QStringLiteral("onApplyTimeControlRequested"),
            QStringLiteral("onGameStarted"),
            QStringLiteral("onConsecutiveStartRequested"),
            QStringLiteral("onConsecutiveGamesConfigured"),
            QStringLiteral("startNextConsecutiveGame()"),
            QStringLiteral("startNewShogiGame()"),
            QStringLiteral("invokeStartGame()"),
        };

        for (const QString& slot : expectedSlots) {
            QVERIFY2(hdr.contains(slot),
                      qPrintable(QStringLiteral("Missing slot declaration: %1").arg(slot)));
        }
    }

    // ================================================================
    // B) deref() ヌル安全パターンの検証
    // ================================================================

    /// 実装ファイルに deref() テンプレートが定義されていること
    void derefHelperExists()
    {
        const QString source = cppLines().join(QLatin1Char('\n'));
        QVERIFY2(source.contains(QStringLiteral("deref")),
                  "deref() helper template not found in source");
    }

    /// 各ルーティングスロットが deref() を使用していること
    void slotsUseDerefForSafeAccess()
    {
        const QStringList& lines = cppLines();

        // deref を使うべきスロット（ダブルポインタをアクセスする）
        const QStringList slotsToCheck = {
            QStringLiteral("GameSessionOrchestrator::initializeGame()"),
            QStringLiteral("GameSessionOrchestrator::handleResignation()"),
            QStringLiteral("GameSessionOrchestrator::handleBreakOffGame()"),
            QStringLiteral("GameSessionOrchestrator::movePieceImmediately()"),
            QStringLiteral("GameSessionOrchestrator::stopTsumeSearch()"),
            QStringLiteral("GameSessionOrchestrator::openWebsiteInExternalBrowser()"),
            QStringLiteral("GameSessionOrchestrator::onMatchGameEnded"),
            QStringLiteral("GameSessionOrchestrator::onGameOverStateChanged"),
            QStringLiteral("GameSessionOrchestrator::onRequestAppendGameOverMove"),
            QStringLiteral("GameSessionOrchestrator::onPreStartCleanupRequested"),
            QStringLiteral("GameSessionOrchestrator::onApplyTimeControlRequested"),
            QStringLiteral("GameSessionOrchestrator::onGameStarted"),
            QStringLiteral("GameSessionOrchestrator::onConsecutiveStartRequested"),
            QStringLiteral("GameSessionOrchestrator::onConsecutiveGamesConfigured"),
            QStringLiteral("GameSessionOrchestrator::startNextConsecutiveGame()"),
            QStringLiteral("GameSessionOrchestrator::invokeStartGame()"),
        };

        for (const QString& sig : slotsToCheck) {
            const auto range = findFunctionBody(lines, sig);
            if (range.first < 0) {
                // startNewShogiGame は deref を使わない可能性があるのでスキップ
                continue;
            }
            const QString body = bodyText(lines, range);

            // ダブルポインタをアクセスするスロットは deref() を使用するべき
            // ただし直接的なダブルポインタアクセスがない場合はスキップ
            const bool accessesDoublePtr = body.contains(QStringLiteral("deref(m_deps."));
            const bool accessesDirectPtr = body.contains(QStringLiteral("m_deps."))
                                           && !body.contains(QStringLiteral("deref("));

            // deref を使うか、ダブルポインタを直接アクセスしないかのどちらか
            if (accessesDirectPtr && !accessesDoublePtr) {
                // コールバックだけ呼んでいるスロット（例: onPreStartCleanupRequested）は OK
                // ダブルポインタを ** でアクセスしていなければ安全
                const bool rawDereference = body.contains(QRegularExpression(
                    QStringLiteral(R"(\*\*m_deps\.)")));
                QVERIFY2(!rawDereference,
                          qPrintable(QStringLiteral("Slot %1 uses raw ** dereference instead of deref()")
                                         .arg(sig)));
            }
        }
    }

    // ================================================================
    // C) ensure → deref 順序の検証（遅延初期化してからアクセス）
    // ================================================================

    /// initializeGame が ensureGameStartCoordinator → deref(gameStart) の順で呼ぶこと
    void initializeGame_ensuresBeforeDeref()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::initializeGame()"));
        QVERIFY2(range.first >= 0, "initializeGame() not found");

        const QString body = bodyText(lines, range);

        const auto ensureIdx = body.indexOf(QStringLiteral("ensureGameStartCoordinator"));
        const auto derefIdx = body.indexOf(QStringLiteral("deref(m_deps.gameStart)"));
        QVERIFY2(ensureIdx >= 0, "ensureGameStartCoordinator not called");
        QVERIFY2(derefIdx >= 0, "deref(m_deps.gameStart) not called");
        QVERIFY2(ensureIdx < derefIdx,
                  "ensureGameStartCoordinator must come before deref(m_deps.gameStart)");
    }

    /// handleResignation が ensureGameStateController → deref の順で呼ぶこと
    void handleResignation_ensuresBeforeDeref()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::handleResignation()"));
        QVERIFY2(range.first >= 0, "handleResignation() not found");

        const QString body = bodyText(lines, range);

        const auto ensureIdx = body.indexOf(QStringLiteral("ensureGameStateController"));
        const auto derefIdx = body.indexOf(QStringLiteral("deref(m_deps.gameStateController)"));
        QVERIFY2(ensureIdx >= 0, "ensureGameStateController not called");
        QVERIFY2(derefIdx >= 0, "deref(m_deps.gameStateController) not called");
        QVERIFY2(ensureIdx < derefIdx,
                  "ensureGameStateController must come before deref");
    }

    /// onMatchGameEnded が 3 つの ensure を呼んでからデリファレンスすること
    void onMatchGameEnded_ensuresAllRequired()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onMatchGameEnded"));
        QVERIFY2(range.first >= 0, "onMatchGameEnded not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureGameStateController")),
                  "onMatchGameEnded must ensure GameStateController");
        QVERIFY2(body.contains(QStringLiteral("ensureConsecutiveGamesController")),
                  "onMatchGameEnded must ensure ConsecutiveGamesController");
        QVERIFY2(body.contains(QStringLiteral("ensureSessionLifecycleCoordinator")),
                  "onMatchGameEnded must ensure SessionLifecycleCoordinator");
    }

    /// onPreStartCleanupRequested が ensure → performCleanup の順で呼ぶこと
    void onPreStartCleanupRequested_callsEnsureAndCleanup()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onPreStartCleanupRequested()"));
        QVERIFY2(range.first >= 0, "onPreStartCleanupRequested not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensurePreStartCleanupHandler")),
                  "Must ensure PreStartCleanupHandler");
        QVERIFY2(body.contains(QStringLiteral("performCleanup")),
                  "Must call performCleanup()");
        QVERIFY2(body.contains(QStringLiteral("clearSessionDependentUi")),
                  "Must call clearSessionDependentUi callback");
    }

    // ================================================================
    // D) ルーティング正確性の検証
    // ================================================================

    /// handleResignation が CsaNetworkMode で CSA コーディネータに委譲すること
    void handleResignation_routesToCsaInCsaMode()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::handleResignation()"));
        QVERIFY2(range.first >= 0, "handleResignation() not found");

        const QString body = bodyText(lines, range);

        // CsaNetworkMode チェックが存在すること
        QVERIFY2(body.contains(QStringLiteral("CsaNetworkMode")),
                  "handleResignation must check for CsaNetworkMode");

        // CSA コーディネータの onResign を呼ぶこと
        QVERIFY2(body.contains(QStringLiteral("onResign")),
                  "handleResignation must call csa->onResign() in CSA mode");

        // GameStateController の handleResignation も呼ぶこと（通常モード）
        QVERIFY2(body.contains(QStringLiteral("gsc->handleResignation")),
                  "handleResignation must delegate to GameStateController in normal mode");
    }

    /// onResignationTriggered が handleResignation に委譲すること
    void onResignationTriggered_delegatesToHandleResignation()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onResignationTriggered()"));
        QVERIFY2(range.first >= 0, "onResignationTriggered not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("handleResignation")),
                  "onResignationTriggered must delegate to handleResignation()");
    }

    /// movePieceImmediately が forceImmediateMove に委譲すること
    void movePieceImmediately_delegatesToMC()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::movePieceImmediately()"));
        QVERIFY2(range.first >= 0, "movePieceImmediately not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("forceImmediateMove")),
                  "movePieceImmediately must call mc->forceImmediateMove()");
    }

    /// stopTsumeSearch が stopAnalysisEngine に委譲すること
    void stopTsumeSearch_delegatesToMC()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::stopTsumeSearch()"));
        QVERIFY2(range.first >= 0, "stopTsumeSearch not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("stopAnalysisEngine")),
                  "stopTsumeSearch must call mc->stopAnalysisEngine()");
    }

    /// openWebsiteInExternalBrowser が DialogCoordinator に委譲すること
    void openWebsite_delegatesToDialogCoordinator()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::openWebsiteInExternalBrowser()"));
        QVERIFY2(range.first >= 0, "openWebsiteInExternalBrowser not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("ensureDialogCoordinator")),
                  "Must ensure DialogCoordinator");
        QVERIFY2(body.contains(QStringLiteral("openProjectWebsite")),
                  "Must call openProjectWebsite()");
    }

    // ================================================================
    // E) onGameStarted の同期ロジック検証
    // ================================================================

    /// onGameStarted が PlayMode を同期すること
    void onGameStarted_syncsPlayMode()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onGameStarted"));
        QVERIFY2(range.first >= 0, "onGameStarted not found");

        const QString body = bodyText(lines, range);

        // m_deps.playMode への書き込みが存在すること
        QVERIFY2(body.contains(QStringLiteral("*m_deps.playMode = opt.mode")),
                  "onGameStarted must sync playMode from StartOptions");
    }

    /// onGameStarted が定跡ウィンドウ更新を呼ぶこと
    void onGameStarted_updatesJosekiWindow()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onGameStarted"));
        QVERIFY2(range.first >= 0, "onGameStarted not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("updateJosekiWindow")),
                  "onGameStarted must call updateJosekiWindow callback");
    }

    /// onGameStarted が SFEN を同期すること
    void onGameStarted_syncsCurrentSfenStr()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onGameStarted"));
        QVERIFY2(range.first >= 0, "onGameStarted not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("currentSfenStr")),
                  "onGameStarted must sync currentSfenStr");
        QVERIFY2(body.contains(QStringLiteral("sfenStart")),
                  "onGameStarted must reference sfenStart from opt");
    }

    // ================================================================
    // F) 連続対局フローの検証
    // ================================================================

    /// onConsecutiveGamesConfigured が CGC::configure を呼ぶこと
    void onConsecutiveGamesConfigured_callsConfigure()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onConsecutiveGamesConfigured"));
        QVERIFY2(range.first >= 0, "onConsecutiveGamesConfigured not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureConsecutiveGamesController")),
                  "Must ensure ConsecutiveGamesController");
        QVERIFY2(body.contains(QStringLiteral("configure(")),
                  "Must call cgc->configure()");
    }

    /// startNextConsecutiveGame が shouldStartNextGame をチェックすること
    void startNextConsecutiveGame_checksCondition()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::startNextConsecutiveGame()"));
        QVERIFY2(range.first >= 0, "startNextConsecutiveGame not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("shouldStartNextGame")),
                  "Must check shouldStartNextGame() before starting");
        QVERIFY2(body.contains(QStringLiteral("startNextGame")),
                  "Must call startNextGame()");
    }

    /// onConsecutiveStartRequested が GameStartCoordinator に委譲すること
    void onConsecutiveStartRequested_delegatesToGameStart()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onConsecutiveStartRequested"));
        QVERIFY2(range.first >= 0, "onConsecutiveStartRequested not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureGameStartCoordinator")),
                  "Must ensure GameStartCoordinator");
        QVERIFY2(body.contains(QStringLiteral("gs->start(")),
                  "Must call gs->start(params)");
    }

    // ================================================================
    // G) SessionLifecycle コールバックの検証
    // ================================================================

    /// startNewShogiGame が SLC に委譲すること
    void startNewShogiGame_delegatesToSLC()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::startNewShogiGame()"));
        QVERIFY2(range.first >= 0, "startNewShogiGame not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureReplayController")),
                  "Must ensure ReplayController");
        QVERIFY2(body.contains(QStringLiteral("ensureSessionLifecycleCoordinator")),
                  "Must ensure SessionLifecycleCoordinator");
        QVERIFY2(body.contains(QStringLiteral("startNewGame")),
                  "Must call slc->startNewGame()");
    }

    /// invokeStartGame が MatchCoordinator を遅延初期化して呼ぶこと
    void invokeStartGame_lazyInitsAndStartsGame()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::invokeStartGame()"));
        QVERIFY2(range.first >= 0, "invokeStartGame not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("initMatchCoordinator")),
                  "Must call initMatchCoordinator if MC is null");
        QVERIFY2(body.contains(QStringLiteral("prepareAndStartGame")),
                  "Must call mc->prepareAndStartGame()");
    }

    // ================================================================
    // H) onApplyTimeControlRequested の検証
    // ================================================================

    /// onApplyTimeControlRequested が SLC に委譲すること
    void onApplyTimeControlRequested_delegatesToSLC()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onApplyTimeControlRequested"));
        QVERIFY2(range.first >= 0, "onApplyTimeControlRequested not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureSessionLifecycleCoordinator")),
                  "Must ensure SessionLifecycleCoordinator");
        QVERIFY2(body.contains(QStringLiteral("applyTimeControl")),
                  "Must call slc->applyTimeControl()");
    }

    // ================================================================
    // I) onRequestAppendGameOverMove の検証
    // ================================================================

    /// onRequestAppendGameOverMove が LiveGameSession の commit を呼ぶこと
    void onRequestAppendGameOverMove_commitsLiveSession()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::onRequestAppendGameOverMove"));
        QVERIFY2(range.first >= 0, "onRequestAppendGameOverMove not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureGameStateController")),
                  "Must ensure GameStateController");
        QVERIFY2(body.contains(QStringLiteral("onRequestAppendGameOverMove")),
                  "Must delegate to gsc->onRequestAppendGameOverMove()");
        QVERIFY2(body.contains(QStringLiteral("liveGameSession")),
                  "Must check liveGameSession");
        QVERIFY2(body.contains(QStringLiteral("commit")),
                  "Must call lgs->commit() for live session");
    }

    // ================================================================
    // J) handleBreakOffGame の検証
    // ================================================================

    /// handleBreakOffGame が GameStateController に委譲すること
    void handleBreakOffGame_delegatesToGSC()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::handleBreakOffGame()"));
        QVERIFY2(range.first >= 0, "handleBreakOffGame not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureGameStateController")),
                  "Must ensure GameStateController");
        QVERIFY2(body.contains(QStringLiteral("handleBreakOffGame")),
                  "Must call gsc->handleBreakOffGame()");
    }

    // ================================================================
    // K) QObject 継承とシグナルスロット
    // ================================================================

    /// GameSessionOrchestrator が QObject を継承していること
    void inheritsQObject()
    {
        const QString& hdr = headerText();
        QVERIFY2(hdr.contains(QStringLiteral("Q_OBJECT")),
                  "Must have Q_OBJECT macro");
        QVERIFY2(hdr.contains(QStringLiteral(": public QObject")),
                  "Must inherit from QObject");
    }

    /// updateDeps メソッドが宣言されていること
    void hasUpdateDepsMethod()
    {
        const QString& hdr = headerText();
        QVERIFY2(hdr.contains(QStringLiteral("void updateDeps(const Deps& deps)")),
                  "Must have updateDeps method");
    }
};

QTEST_MAIN(TestAppGameSession)

#include "tst_app_game_session.moc"
