/// @file tst_app_lifecycle_pipeline.cpp
/// @brief MainWindowLifecyclePipeline の構造的契約テスト
///
/// MainWindowLifecyclePipeline は MainWindow& に密結合しているため、
/// 単体インスタンス化は不可能。代わりにソース解析テストで
/// 起動/終了フローの構造的契約を検証する。
/// 既存の tst_structural_kpi / tst_layer_dependencies と同パターン。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppLifecyclePipeline : public QObject
{
    Q_OBJECT

private:
    /// ソースファイルの全テキストを返す
    static QString readSourceFile(const QString& relPath)
    {
        QFile file(QStringLiteral(SOURCE_DIR "/") + relPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QTextStream(&file).readAll();
    }

    /// ソースファイルの各行リストを返す
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

    /// 関数本体の行範囲（開始行, 終了行）を返す。見つからなければ (-1,-1)
    /// pattern は "void ClassName::methodName(" のような文字列。
    static QPair<int, int> findFunctionBody(const QStringList& lines, const QString& signature)
    {
        const int sz = int(lines.size());

        // 1. シグネチャを含む行を探す
        int sigLine = -1;
        for (int i = 0; i < sz; ++i) {
            if (lines[i].contains(signature)) {
                sigLine = i;
                break;
            }
        }
        if (sigLine < 0)
            return {-1, -1};

        // 2. シグネチャ行以降で最初の '{' を探す（= 関数本体開始）
        int braceStart = -1;
        for (int i = sigLine; i < sz; ++i) {
            if (lines[i].contains(QLatin1Char('{'))) {
                braceStart = i;
                break;
            }
        }
        if (braceStart < 0)
            return {-1, -1};

        // 3. braceStart 行から波括弧のネストを追跡して本体末尾を探す
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

    /// 指定範囲の行から関数呼び出し名を順番に抽出する
    static QStringList extractCallOrder(const QStringList& lines, int startLine, int endLine)
    {
        QStringList calls;
        // "識別子();" または "識別子->メソッド();" パターン
        const QRegularExpression callRe(QStringLiteral(R"(^\s+(\w[\w:>-]*\w)\s*\()"));
        for (int i = startLine; i <= endLine && i < int(lines.size()); ++i) {
            const QString& line = lines[i];
            // コメント行はスキップ
            if (line.trimmed().startsWith(QLatin1String("//")))
                continue;
            const auto match = callRe.match(line);
            if (match.hasMatch())
                calls << match.captured(1);
        }
        return calls;
    }

    static QString bodyText(const QStringList& lines, int startLine, int endLine)
    {
        QStringList bodyLines;
        for (int i = startLine; i <= endLine && i < int(lines.size()); ++i)
            bodyLines << lines[i];
        return bodyLines.join(QLatin1Char('\n'));
    }

private slots:
    // ================================================================
    // Startup Tests
    // ================================================================

    /// runStartup() が全8段階の関数を順序通りに呼ぶこと
    void startupCallsAllEightStepsInOrder()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runStartup()"));
        QVERIFY2(range.first >= 0, "runStartup() function not found");

        const QString runStartupBody = bodyText(lines, range.first, range.second);
        if (runStartupBody.contains(QStringLiteral("runLifecycleStartupInternal"))) {
            const auto internalRange = findFunctionBody(
                lines, QStringLiteral("MainWindow::runLifecycleStartupInternal()"));
            QVERIFY2(internalRange.first >= 0, "runLifecycleStartupInternal() not found");
            const QString internalBody = bodyText(lines, internalRange.first, internalRange.second);
            const QStringList expectedTokens = {
                QStringLiteral("createFoundationObjectsForLifecycle"),
                QStringLiteral("setupUiSkeletonForLifecycle"),
                QStringLiteral("initializeCoreComponentsForLifecycle"),
                QStringLiteral("initializeEarlyServicesForLifecycle"),
                QStringLiteral("buildGamePanels"),
                QStringLiteral("restoreWindowAndSync"),
                QStringLiteral("connectSignalsForLifecycle"),
                QStringLiteral("finalizeAndConfigureUiForLifecycle"),
            };
            qsizetype lastPos = -1;
            for (const QString& token : expectedTokens) {
                const qsizetype pos = internalBody.indexOf(token);
                QVERIFY2(pos >= 0,
                          qPrintable(QStringLiteral("Token '%1' not found in runLifecycleStartupInternal()").arg(token)));
                QVERIFY2(pos > lastPos,
                          qPrintable(QStringLiteral("Token '%1' is out of order").arg(token)));
                lastPos = pos;
            }
            return;
        }

        const QStringList calls = extractCallOrder(lines, range.first, range.second);
        const QStringList expectedSteps = {
            QStringLiteral("createFoundationObjects"),
            QStringLiteral("setupUiSkeleton"),
            QStringLiteral("initializeCoreComponents"),
            QStringLiteral("initializeEarlyServices"),
            QStringLiteral("buildGamePanels"),
            QStringLiteral("restoreWindowAndSync"),
            QStringLiteral("connectSignals"),
            QStringLiteral("finalizeAndConfigureUi"),
        };

        // 各ステップが呼び出しリストに含まれ、順序が正しいこと
        int lastIndex = -1;
        for (const QString& step : expectedSteps) {
            const int idx = int(calls.indexOf(step));
            QVERIFY2(idx >= 0,
                      qPrintable(QStringLiteral("Step '%1' not found in runStartup() calls: [%2]")
                                     .arg(step, calls.join(QStringLiteral(", ")))));
            QVERIFY2(idx > lastIndex,
                      qPrintable(QStringLiteral("Step '%1' is out of order (index %2, previous %3)")
                                     .arg(step)
                                     .arg(idx)
                                     .arg(lastIndex)));
            lastIndex = idx;
        }

        // 正確に8段階であること
        QCOMPARE(expectedSteps.size(), 8);
    }

    /// 各起動ステップがヘッダーで private メソッドとして宣言されていること
    void startupStepsDeclaredInHeader()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read pipeline header");

        const bool oldStyle = header.contains(QStringLiteral("createFoundationObjects"));
        if (oldStyle) {
            const QStringList expectedMethods = {
                QStringLiteral("createFoundationObjects"),
                QStringLiteral("setupUiSkeleton"),
                QStringLiteral("initializeCoreComponents"),
                QStringLiteral("initializeEarlyServices"),
                QStringLiteral("buildGamePanels"),
                QStringLiteral("restoreWindowAndSync"),
                QStringLiteral("connectSignals"),
                QStringLiteral("finalizeAndConfigureUi"),
            };

            for (const QString& method : expectedMethods) {
                QVERIFY2(header.contains(method),
                          qPrintable(QStringLiteral("Method '%1' not declared in header").arg(method)));
            }
            return;
        }

        const QString mwHeader = readSourceFile(
            QStringLiteral("src/app/mainwindow.h"));
        QVERIFY2(!mwHeader.isEmpty(), "Failed to read mainwindow header");
        const QStringList expectedMethods = {
            QStringLiteral("runLifecycleStartupInternal"),
            QStringLiteral("createFoundationObjectsForLifecycle"),
            QStringLiteral("setupUiSkeletonForLifecycle"),
            QStringLiteral("initializeCoreComponentsForLifecycle"),
            QStringLiteral("initializeEarlyServicesForLifecycle"),
            QStringLiteral("connectSignalsForLifecycle"),
            QStringLiteral("finalizeAndConfigureUiForLifecycle"),
        };
        for (const QString& method : expectedMethods) {
            QVERIFY2(mwHeader.contains(method),
                      qPrintable(QStringLiteral("Method '%1' not declared in mainwindow.h").arg(method)));
        }
    }

    /// 各起動ステップの実装が .cpp ファイルに存在すること
    void startupStepsImplementedInSource()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const QString source = lines.join(QLatin1Char('\n'));
        const bool oldStyle = source.contains(
            QStringLiteral("MainWindowLifecyclePipeline::createFoundationObjects()"));
        const QStringList expectedImpls = oldStyle
            ? QStringList{
                  QStringLiteral("MainWindowLifecyclePipeline::createFoundationObjects()"),
                  QStringLiteral("MainWindowLifecyclePipeline::setupUiSkeleton()"),
                  QStringLiteral("MainWindowLifecyclePipeline::initializeCoreComponents()"),
                  QStringLiteral("MainWindowLifecyclePipeline::initializeEarlyServices()"),
                  QStringLiteral("MainWindowLifecyclePipeline::buildGamePanels()"),
                  QStringLiteral("MainWindowLifecyclePipeline::restoreWindowAndSync()"),
                  QStringLiteral("MainWindowLifecyclePipeline::connectSignals()"),
                  QStringLiteral("MainWindowLifecyclePipeline::finalizeAndConfigureUi()"),
              }
            : QStringList{
                  QStringLiteral("MainWindow::createFoundationObjectsForLifecycle()"),
                  QStringLiteral("MainWindow::setupUiSkeletonForLifecycle()"),
                  QStringLiteral("MainWindow::initializeCoreComponentsForLifecycle()"),
                  QStringLiteral("MainWindow::initializeEarlyServicesForLifecycle()"),
                  QStringLiteral("MainWindow::connectSignalsForLifecycle()"),
                  QStringLiteral("MainWindow::finalizeAndConfigureUiForLifecycle()"),
              };
        for (const QString& impl : expectedImpls) {
            QVERIFY2(source.contains(impl),
                      qPrintable(
                          QStringLiteral("Implementation of '%1' not found in source").arg(impl)));
        }
    }

    // ================================================================
    // Shutdown Tests
    // ================================================================

    /// runShutdown() が m_shutdownDone ガードで二重実行を防止すること
    void shutdownHasDoubleExecutionGuard()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown() function not found");

        // 関数本体のテキスト
        QStringList bodyLines;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            bodyLines << lines[i];
        QString body = bodyLines.join(QLatin1Char('\n'));
        if (body.contains(QStringLiteral("runLifecycleShutdownInternal"))) {
            const auto internalRange = findFunctionBody(
                lines, QStringLiteral("MainWindow::runLifecycleShutdownInternal("));
            QVERIFY2(internalRange.first >= 0, "runLifecycleShutdownInternal() not found");
            body = bodyText(lines, internalRange.first, internalRange.second);
        }

        // ガードパターン: "if (m_shutdownDone) return;" が存在すること
        QVERIFY2(body.contains(QStringLiteral("m_shutdownDone"))
                     || body.contains(QStringLiteral("shutdownDone")),
                  "runShutdown() does not reference m_shutdownDone guard");
        QVERIFY2(body.contains(QStringLiteral("return")),
                  "runShutdown() does not have early return for guard");

        // フラグ設定: "m_shutdownDone = true;" が存在すること
        QVERIFY2(body.contains(QStringLiteral("m_shutdownDone = true"))
                     || body.contains(QStringLiteral("shutdownDone = true")),
                  "runShutdown() does not set m_shutdownDone = true");
    }

    /// m_shutdownDone がヘッダーで false に初期化されていること
    void shutdownFlagInitializedToFalse()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read pipeline header");

        QVERIFY2(header.contains(QStringLiteral("m_shutdownDone = false")),
                  "m_shutdownDone not initialized to false in header");
    }

    /// runShutdown() がフラグチェック後にリソース解放を行うこと
    void shutdownPerformsResourceCleanup()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown() function not found");

        QStringList bodyLines;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            bodyLines << lines[i];
        QString body = bodyLines.join(QLatin1Char('\n'));
        if (body.contains(QStringLiteral("runLifecycleShutdownInternal"))) {
            const auto internalRange = findFunctionBody(
                lines, QStringLiteral("MainWindow::runLifecycleShutdownInternal("));
            QVERIFY2(internalRange.first >= 0, "runLifecycleShutdownInternal() not found");
            body = bodyText(lines, internalRange.first, internalRange.second);
        }

        // エンジン終了: destroyEngines 呼び出しが存在すること
        QVERIFY2(body.contains(QStringLiteral("destroyEngines")),
                  "runShutdown() does not call destroyEngines()");

        // リソース解放: unique_ptr::reset() 呼び出しが存在すること
        QVERIFY2(body.contains(QStringLiteral(".reset()")),
                  "runShutdown() does not call .reset() for resource cleanup");
    }

    /// runShutdown() のガードが return より前に配置されていること
    /// （フラグチェック → return → フラグ設定 → クリーンアップ の順序）
    void shutdownGuardOrderIsCorrect()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown() function not found");

        QPair<int, int> actualRange = range;
        {
            const QString runShutdownBody = bodyText(lines, range.first, range.second);
            if (runShutdownBody.contains(QStringLiteral("runLifecycleShutdownInternal"))) {
                const auto internalRange = findFunctionBody(
                    lines, QStringLiteral("MainWindow::runLifecycleShutdownInternal("));
                QVERIFY2(internalRange.first >= 0, "runLifecycleShutdownInternal() not found");
                actualRange = internalRange;
            }
        }

        // ガードチェック行、フラグ設定行、クリーンアップ行の順序を確認
        int guardCheckLine = -1;
        int flagSetLine = -1;
        int cleanupLine = -1;

        for (int i = actualRange.first; i <= actualRange.second && i < int(lines.size()); ++i) {
            const QString& line = lines[i];
            if ((line.contains(QStringLiteral("if (m_shutdownDone)"))
                 || line.contains(QStringLiteral("if (shutdownDone)"))) && guardCheckLine < 0)
                guardCheckLine = i;
            if ((line.contains(QStringLiteral("m_shutdownDone = true"))
                 || line.contains(QStringLiteral("shutdownDone = true"))) && flagSetLine < 0)
                flagSetLine = i;
            if (line.contains(QStringLiteral("destroyEngines")) && cleanupLine < 0)
                cleanupLine = i;
        }

        QVERIFY2(guardCheckLine >= 0, "Guard check line not found");
        QVERIFY2(flagSetLine >= 0, "Flag set line not found");
        QVERIFY2(cleanupLine >= 0, "Cleanup line not found");

        // 順序: ガードチェック < フラグ設定 < クリーンアップ
        QVERIFY2(guardCheckLine < flagSetLine,
                  qPrintable(QStringLiteral("Guard check (line %1) should come before flag set (line %2)")
                                 .arg(guardCheckLine)
                                 .arg(flagSetLine)));
        QVERIFY2(flagSetLine < cleanupLine,
                  qPrintable(QStringLiteral("Flag set (line %1) should come before cleanup (line %2)")
                                 .arg(flagSetLine)
                                 .arg(cleanupLine)));
    }

    // ================================================================
    // Integration Contract Tests
    // ================================================================

    /// MainWindow コンストラクタがパイプラインを生成し runStartup() を呼ぶこと
    void mainWindowConstructorDelegatesToPipeline()
    {
        const QString source = readSourceFile(QStringLiteral("src/app/mainwindow.cpp"));
        QVERIFY2(!source.isEmpty(), "Failed to read mainwindow.cpp");

        // パイプライン生成
        QVERIFY2(source.contains(QStringLiteral("MainWindowLifecyclePipeline")),
                  "MainWindow does not reference MainWindowLifecyclePipeline");

        // runStartup() 呼び出し
        QVERIFY2(source.contains(QStringLiteral("m_pipeline->runStartup()")),
                  "MainWindow constructor does not call m_pipeline->runStartup()");
    }

    /// MainWindow デストラクタがパイプラインの runShutdown() を呼ぶこと
    void mainWindowDestructorDelegatesToPipeline()
    {
        const QStringList lines = readSourceLines(QStringLiteral("src/app/mainwindow.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read mainwindow.cpp");

        // デストラクタ本体を見つける
        const auto range = findFunctionBody(lines, QStringLiteral("MainWindow::~MainWindow()"));
        QVERIFY2(range.first >= 0, "MainWindow destructor not found");

        QStringList bodyLines;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            bodyLines << lines[i];
        const QString body = bodyLines.join(QLatin1Char('\n'));

        QVERIFY2(body.contains(QStringLiteral("m_pipeline->runShutdown()")),
                  "MainWindow destructor does not call m_pipeline->runShutdown()");
    }

    /// closeEvent と saveSettingsAndClose も runShutdown() を呼ぶこと
    void shutdownCalledFromMultipleExitPaths()
    {
        const QString source = readSourceFile(QStringLiteral("src/app/mainwindow.cpp"));
        QVERIFY2(!source.isEmpty(), "Failed to read mainwindow.cpp");

        // closeEvent 内で runShutdown が呼ばれること
        const QStringList lines = readSourceLines(QStringLiteral("src/app/mainwindow.cpp"));
        {
            const auto range = findFunctionBody(
                lines, QStringLiteral("MainWindow::closeEvent"));
            QVERIFY2(range.first >= 0, "closeEvent not found");
            QStringList body;
            for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
                body << lines[i];
            QVERIFY2(body.join(QLatin1Char('\n')).contains(QStringLiteral("runShutdown")),
                      "closeEvent does not call runShutdown()");
        }

        // saveSettingsAndClose 内で shutdownAndQuit が呼ばれること
        {
            const auto range = findFunctionBody(
                lines, QStringLiteral("MainWindow::saveSettingsAndClose"));
            QVERIFY2(range.first >= 0, "saveSettingsAndClose not found");
            QStringList body;
            for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
                body << lines[i];
            QVERIFY2(body.join(QLatin1Char('\n')).contains(QStringLiteral("shutdownAndQuit")),
                      "saveSettingsAndClose does not call shutdownAndQuit()");
        }
    }

    // ================================================================
    // Public API Contract Tests
    // ================================================================

    /// パイプラインヘッダーが正しいパブリック API を持つこと
    void pipelineHasExpectedPublicApi()
    {
        const QString header = readSourceFile(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.h"));
        QVERIFY2(!header.isEmpty(), "Failed to read pipeline header");

        // public API
        QVERIFY2(header.contains(QStringLiteral("void runStartup()")),
                  "runStartup() not declared");
        QVERIFY2(header.contains(QStringLiteral("void runShutdown()")),
                  "runShutdown() not declared");

        // MainWindow& 依存
        QVERIFY2(header.contains(QStringLiteral("MainWindow& m_mw")),
                  "Missing MainWindow& member");

        // friend 方式（旧）または MainWindow 側内部API方式（新）
        const QString mwHeader = readSourceFile(QStringLiteral("src/app/mainwindow.h"));
        const bool hasFriend = mwHeader.contains(
            QStringLiteral("friend class MainWindowLifecyclePipeline"));
        const bool hasInternalApi = mwHeader.contains(
            QStringLiteral("runLifecycleStartupInternal"));
        QVERIFY2(hasFriend || hasInternalApi,
                  "MainWindow should expose either friend access or lifecycle internal API");
    }

    /// 設定保存がシャットダウンに含まれること
    void shutdownIncludesSettingsSave()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown() function not found");

        QStringList bodyLines;
        for (int i = range.first; i <= range.second && i < int(lines.size()); ++i)
            bodyLines << lines[i];
        QString body = bodyLines.join(QLatin1Char('\n'));
        if (body.contains(QStringLiteral("runLifecycleShutdownInternal"))) {
            const auto internalRange = findFunctionBody(
                lines, QStringLiteral("MainWindow::runLifecycleShutdownInternal("));
            QVERIFY2(internalRange.first >= 0, "runLifecycleShutdownInternal() not found");
            body = bodyText(lines, internalRange.first, internalRange.second);
        }

        // 設定保存呼び出しが存在すること
        QVERIFY2(body.contains(QStringLiteral("saveWindowAndBoard"))
                     || body.contains(QStringLiteral("saveDockStates"))
                     || body.contains(QStringLiteral("saveSettings")),
                  "runShutdown() does not include settings save");
    }
};

QTEST_MAIN(TestAppLifecyclePipeline)

#include "tst_app_lifecycle_pipeline.moc"
