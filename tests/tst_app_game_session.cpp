/// @file tst_app_game_session.cpp
/// @brief GameSessionOrchestrator の構造的契約テスト

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
    void depsHasRequiredEnsureCallbacks()
    {
        const QString& hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredCallbacks = {
            QStringLiteral("ensureGameStateController"),
            QStringLiteral("ensureGameStartCoordinator"),
            QStringLiteral("ensureDialogCoordinator"),
        };

        for (const QString& cb : requiredCallbacks) {
            QVERIFY2(hdr.contains(cb),
                      qPrintable(QStringLiteral("Deps missing callback: %1").arg(cb)));
        }
    }

    void depsHasRequiredControllerPointers()
    {
        const QString& hdr = headerText();

        const QStringList requiredPtrs = {
            QStringLiteral("GameStateController**"),
            QStringLiteral("GameStartCoordinator**"),
            QStringLiteral("DialogCoordinator**"),
            QStringLiteral("CsaGameCoordinator**"),
            QStringLiteral("MatchCoordinator**"),
            QStringLiteral("ReplayController*"),
        };

        for (const QString& ptr : requiredPtrs) {
            QVERIFY2(hdr.contains(ptr),
                      qPrintable(QStringLiteral("Deps missing pointer: %1").arg(ptr)));
        }
    }

    void removedLifecycleDepsAreAbsent()
    {
        const QString& hdr = headerText();

        const QStringList removedEntries = {
            QStringLiteral("SessionLifecycleCoordinator**"),
            QStringLiteral("ConsecutiveGamesController**"),
            QStringLiteral("PreStartCleanupHandler**"),
            QStringLiteral("TimeControlController**"),
            QStringLiteral("LiveGameSession**"),
            QStringLiteral("lastTimeControl"),
            QStringLiteral("ensureSessionLifecycleCoordinator"),
            QStringLiteral("ensureConsecutiveGamesController"),
            QStringLiteral("ensurePreStartCleanupHandler"),
            QStringLiteral("ensureReplayController"),
            QStringLiteral("clearSessionDependentUi"),
            QStringLiteral("updateJosekiWindow"),
            QStringLiteral("sfenRecord"),
        };

        for (const QString& removed : removedEntries) {
            QVERIFY2(!hdr.contains(removed),
                      qPrintable(QStringLiteral("Deprecated lifecycle dependency still present: %1")
                                     .arg(removed)));
        }
    }

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
            QStringLiteral("onResignationTriggered()"),
        };

        for (const QString& slot : expectedSlots) {
            QVERIFY2(hdr.contains(slot),
                      qPrintable(QStringLiteral("Missing slot declaration: %1").arg(slot)));
        }
    }

    void lifecycleSlotsAreRemoved()
    {
        const QString& hdr = headerText();

        const auto join = [](const QStringList& parts) {
            return parts.join(QString());
        };
        const QStringList removedSlots = {
            join({QStringLiteral("on"), QStringLiteral("Match"), QStringLiteral("GameEnded")}),
            join({QStringLiteral("on"), QStringLiteral("GameOver"), QStringLiteral("StateChanged")}),
            join({QStringLiteral("on"), QStringLiteral("RequestAppend"), QStringLiteral("GameOverMove")}),
            join({QStringLiteral("on"), QStringLiteral("PreStart"), QStringLiteral("CleanupRequested")}),
            join({QStringLiteral("on"), QStringLiteral("ApplyTimeControl"), QStringLiteral("Requested")}),
            join({QStringLiteral("on"), QStringLiteral("GameStarted")}),
            join({QStringLiteral("on"), QStringLiteral("ConsecutiveStart"), QStringLiteral("Requested")}),
            join({QStringLiteral("on"), QStringLiteral("ConsecutiveGames"), QStringLiteral("Configured")}),
            join({QStringLiteral("start"), QStringLiteral("NextConsecutive"), QStringLiteral("Game")}),
        };

        for (const QString& slot : removedSlots) {
            QVERIFY2(!hdr.contains(slot),
                      qPrintable(QStringLiteral("Removed lifecycle slot still declared: %1")
                                     .arg(slot)));
        }
    }

    void derefHelperExists()
    {
        const QString source = cppLines().join(QLatin1Char('\n'));
        QVERIFY2(source.contains(QStringLiteral("deref")),
                  "deref() helper template not found in source");
    }

    void slotsUseDerefForSafeAccess()
    {
        const QStringList& lines = cppLines();

        const QStringList slotsToCheck = {
            QStringLiteral("GameSessionOrchestrator::initializeGame()"),
            QStringLiteral("GameSessionOrchestrator::handleResignation()"),
            QStringLiteral("GameSessionOrchestrator::handleBreakOffGame()"),
            QStringLiteral("GameSessionOrchestrator::movePieceImmediately()"),
            QStringLiteral("GameSessionOrchestrator::stopTsumeSearch()"),
            QStringLiteral("GameSessionOrchestrator::openWebsiteInExternalBrowser()"),
        };

        for (const QString& sig : slotsToCheck) {
            const auto range = findFunctionBody(lines, sig);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Slot not found: %1").arg(sig)));

            const QString body = bodyText(lines, range);
            QVERIFY2(body.contains(QStringLiteral("deref(")),
                      qPrintable(QStringLiteral("Slot must use deref(): %1").arg(sig)));

            const bool rawDereference = body.contains(QRegularExpression(
                QStringLiteral(R"(\*\*m_deps\.)")));
            QVERIFY2(!rawDereference,
                      qPrintable(QStringLiteral("Slot %1 uses raw ** dereference").arg(sig)));
        }
    }

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

    void initializeGame_usesMatchForClockAndSfenRecord()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::initializeGame()"));
        QVERIFY2(range.first >= 0, "initializeGame() not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("match ? match->sfenRecordPtr() : nullptr")),
                  "initializeGame must obtain sfenRecord from MatchCoordinator");
        QVERIFY2(body.contains(QStringLiteral("match ? match->clock() : nullptr")),
                  "initializeGame must obtain clock from MatchCoordinator");
        QVERIFY2(!body.contains(QStringLiteral("timeController")),
                  "initializeGame must not depend on TimeControlController");
    }

    void initializeGame_usesReplayModeDirectly()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::initializeGame()"));
        QVERIFY2(range.first >= 0, "initializeGame() not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("m_deps.replayController ? m_deps.replayController->isReplayMode() : false")),
                  "initializeGame must read replay mode directly from ReplayController");
    }

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

    void handleResignation_routesToCsaInCsaMode()
    {
        const QStringList& lines = cppLines();
        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSessionOrchestrator::handleResignation()"));
        QVERIFY2(range.first >= 0, "handleResignation() not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("CsaNetworkMode")),
                  "handleResignation must check for CsaNetworkMode");
        QVERIFY2(body.contains(QStringLiteral("onResign")),
                  "handleResignation must call csa->onResign() in CSA mode");
        QVERIFY2(body.contains(QStringLiteral("gsc->handleResignation")),
                  "handleResignation must delegate to GameStateController in normal mode");
    }

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

    void inheritsQObject()
    {
        const QString& hdr = headerText();
        QVERIFY2(hdr.contains(QStringLiteral("Q_OBJECT")),
                  "Must have Q_OBJECT macro");
        QVERIFY2(hdr.contains(QStringLiteral(": public QObject")),
                  "Must inherit from QObject");
    }

    void hasUpdateDepsMethod()
    {
        const QString& hdr = headerText();
        QVERIFY2(hdr.contains(QStringLiteral("void updateDeps(const Deps& deps)")),
                  "Must have updateDeps method");
    }
};

QTEST_MAIN(TestAppGameSession)

#include "tst_app_game_session.moc"
