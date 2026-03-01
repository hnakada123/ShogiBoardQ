/// @file tst_app_error_handling.cpp
/// @brief app層の異常系・防御パターンの構造的契約テスト
///
/// 初期化失敗、依存未生成、二重実行防止の3パターンについて
/// ソースコード解析で構造的契約を検証する。
/// 加えて SFEN パース等の純粋関数に対する異常系ユニットテストも含む。
/// 既存の tst_app_lifecycle_pipeline / tst_wiring_contracts と同パターン。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include "shogiboard.h"

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppErrorHandling : public QObject
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

    /// 指定範囲の行を結合して返す
    static QString bodyText(const QStringList& lines, int startLine, int endLine)
    {
        QStringList body;
        for (int i = startLine; i <= endLine && i < int(lines.size()); ++i)
            body << lines[i];
        return body.join(QLatin1Char('\n'));
    }

private slots:
    // ================================================================
    // 1. ensure* メソッドの冪等性ガード
    // ================================================================

    /// FoundationRegistry の ensure* メソッドが冪等性ガードを持つこと
    void foundationEnsureMethodsHaveIdempotentGuards()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowfoundationregistry.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read foundation registry source");

        // ガードパターンを持つべき ensure* メソッド
        // ensureUiStatePolicyManager は CompositionRoot に委譲するため除外
        // ensureBoardSyncPresenter は既存オブジェクトの更新もあるため除外
        const QStringList ensureMethods = {
            QStringLiteral("MainWindowFoundationRegistry::ensurePlayerInfoWiring()"),
            QStringLiteral("MainWindowFoundationRegistry::ensurePlayerInfoController()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureCommentCoordinator()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureUiNotificationService()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureEvaluationGraphController()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureMenuWiring()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureLanguageController()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureDockCreationService()"),
            QStringLiteral("MainWindowFoundationRegistry::ensurePositionEditController()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureAnalysisPresenter()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureJishogiController()"),
            QStringLiteral("MainWindowFoundationRegistry::ensureNyugyokuHandler()"),
        };

        for (const QString& method : std::as_const(ensureMethods)) {
            const auto range = findFunctionBody(lines, method);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method '%1' not found").arg(method)));

            const QString body = bodyText(lines, range.first, range.second);
            // 冪等性ガードパターン:
            // (a) "if (m_mw.m_xxx) return;" — 早期リターン
            // (b) "if (!m_mw.m_xxx)" — ガード付き生成（本体が if 内のみ）
            // (c) "if (m_mw.m_models.xxx) return;" — ネスト型メンバ
            const bool hasGuard = body.contains(QRegularExpression(
                QStringLiteral(R"(if\s*\(\s*!?\s*m_mw\.m_[\w.]+\s*\))")));
            QVERIFY2(hasGuard,
                      qPrintable(QStringLiteral("Method '%1' lacks idempotent guard (if (m_mw.m_*))")
                                     .arg(method)));
        }
    }

    /// GameSubRegistry 系の ensure* メソッドが冪等性ガードを持つこと
    void gameSubRegistryEnsureMethodsHaveIdempotentGuards()
    {
        // GameSubRegistry 本体（状態・コントローラ管理）
        {
            const QStringList lines = readSourceLines(
                QStringLiteral("src/app/gamesubregistry.cpp"));
            QVERIFY2(!lines.isEmpty(), "Failed to read gamesubregistry source");

            const QStringList ensureMethods = {
                QStringLiteral("GameSubRegistry::ensureTimeController()"),
                QStringLiteral("GameSubRegistry::ensureReplayController()"),
                QStringLiteral("GameSubRegistry::ensureGameStateController()"),
                QStringLiteral("GameSubRegistry::ensureGameStartCoordinator()"),
            };

            for (const QString& method : std::as_const(ensureMethods)) {
                const auto range = findFunctionBody(lines, method);
                QVERIFY2(range.first >= 0,
                          qPrintable(QStringLiteral("Method '%1' not found").arg(method)));

                const QString body = bodyText(lines, range.first, range.second);
                const bool hasGuard = body.contains(QRegularExpression(
                    QStringLiteral(R"(if\s*\(\s*!?\s*m_mw\.m_[\w.]+\s*\))")));
                QVERIFY2(hasGuard,
                          qPrintable(QStringLiteral("Method '%1' lacks idempotent guard")
                                         .arg(method)));
            }
        }

        // GameSessionSubRegistry（セッション管理）
        {
            const QStringList lines = readSourceLines(
                QStringLiteral("src/app/gamesessionsubregistry.cpp"));
            QVERIFY2(!lines.isEmpty(), "Failed to read gamesessionsubregistry source");

            const QStringList ensureMethods = {
                QStringLiteral("GameSessionSubRegistry::ensurePreStartCleanupHandler()"),
            };

            for (const QString& method : std::as_const(ensureMethods)) {
                const auto range = findFunctionBody(lines, method);
                QVERIFY2(range.first >= 0,
                          qPrintable(QStringLiteral("Method '%1' not found").arg(method)));

                const QString body = bodyText(lines, range.first, range.second);
                const bool hasGuard = body.contains(QRegularExpression(
                    QStringLiteral(R"(if\s*\(\s*!?\s*m_mw\.m_[\w.]+\s*\))")));
                QVERIFY2(hasGuard,
                          qPrintable(QStringLiteral("Method '%1' lacks idempotent guard")
                                         .arg(method)));
            }
        }

        // GameWiringSubRegistry（配線管理）
        {
            const QStringList lines = readSourceLines(
                QStringLiteral("src/app/gamewiringsubregistry.cpp"));
            QVERIFY2(!lines.isEmpty(), "Failed to read gamewiringsubregistry source");

            // ensureMatchCoordinatorWiring は二回目以降も Deps 更新が走るため除外
            // ensureTurnSyncBridge は null チェック後に条件付きで処理するため除外
            const QStringList ensureMethods = {
                QStringLiteral("GameWiringSubRegistry::ensureCsaGameWiring()"),
                QStringLiteral("GameWiringSubRegistry::ensureConsecutiveGamesController()"),
            };

            for (const QString& method : std::as_const(ensureMethods)) {
                const auto range = findFunctionBody(lines, method);
                QVERIFY2(range.first >= 0,
                          qPrintable(QStringLiteral("Method '%1' not found").arg(method)));

                const QString body = bodyText(lines, range.first, range.second);
                const bool hasGuard = body.contains(QRegularExpression(
                    QStringLiteral(R"(if\s*\(\s*!?\s*m_mw\.m_[\w.]+\s*\))")));
                QVERIFY2(hasGuard,
                          qPrintable(QStringLiteral("Method '%1' lacks idempotent guard")
                                         .arg(method)));
            }
        }
    }

    /// KifuSubRegistry の ensure* メソッドが冪等性ガードを持つこと
    void kifuSubRegistryEnsureMethodsHaveIdempotentGuards()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/kifusubregistry.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read kifusubregistry source");

        const QStringList ensureMethods = {
            QStringLiteral("KifuSubRegistry::ensureKifuExportController()"),
            QStringLiteral("KifuSubRegistry::ensureKifuLoadCoordinatorForLive()"),
            QStringLiteral("KifuSubRegistry::ensureGameRecordModel()"),
            QStringLiteral("KifuSubRegistry::ensureJosekiWiring()"),
        };

        for (const QString& method : std::as_const(ensureMethods)) {
            const auto range = findFunctionBody(lines, method);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method '%1' not found").arg(method)));

            const QString body = bodyText(lines, range.first, range.second);
            const bool hasGuard = body.contains(QRegularExpression(
                QStringLiteral(R"(if\s*\(\s*!?\s*m_mw\.m_[\w.]+\s*\))")));
            QVERIFY2(hasGuard,
                      qPrintable(QStringLiteral("Method '%1' lacks idempotent guard")
                                     .arg(method)));
        }
    }

    // ================================================================
    // 2. GameSessionOrchestrator の deref() null安全性
    // ================================================================

    /// deref() ヘルパーが存在し、nullptr を安全に扱うこと
    void gsoDerefHelperExists()
    {
        const QString source = readSourceFile(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!source.isEmpty(), "Failed to read gso source");

        // deref テンプレートが定義されていること
        QVERIFY2(source.contains(QStringLiteral("T* deref(T** pp)")),
                  "deref() helper template not found");

        // nullptr チェックがあること
        QVERIFY2(source.contains(QStringLiteral("pp ? *pp : nullptr")),
                  "deref() does not have null safety check");
    }

    /// deref() 呼び出し後に null チェックまたは早期リターンがあること
    void gsoDerefCallsFollowedByNullCheck()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gso source");

        // deref() を呼んでいるスロットメソッド群
        const QStringList methods = {
            QStringLiteral("GameSessionOrchestrator::initializeGame()"),
            QStringLiteral("GameSessionOrchestrator::handleResignation()"),
            QStringLiteral("GameSessionOrchestrator::handleBreakOffGame()"),
            QStringLiteral("GameSessionOrchestrator::movePieceImmediately()"),
            QStringLiteral("GameSessionOrchestrator::stopTsumeSearch()"),
            QStringLiteral("GameSessionOrchestrator::openWebsiteInExternalBrowser()"),
            QStringLiteral("GameSessionOrchestrator::onMatchGameEnded("),
            QStringLiteral("GameSessionOrchestrator::onGameOverStateChanged("),
            QStringLiteral("GameSessionOrchestrator::onRequestAppendGameOverMove("),
            QStringLiteral("GameSessionOrchestrator::onPreStartCleanupRequested()"),
            QStringLiteral("GameSessionOrchestrator::onApplyTimeControlRequested("),
            QStringLiteral("GameSessionOrchestrator::onGameStarted("),
            QStringLiteral("GameSessionOrchestrator::onConsecutiveStartRequested("),
            QStringLiteral("GameSessionOrchestrator::onConsecutiveGamesConfigured("),
            QStringLiteral("GameSessionOrchestrator::startNextConsecutiveGame()"),
            QStringLiteral("GameSessionOrchestrator::startNewShogiGame()"),
            QStringLiteral("GameSessionOrchestrator::invokeStartGame()"),
        };

        // null チェックパターン: "if (!varname)" or "if (varname)"
        const QRegularExpression nullCheckRe(QStringLiteral(R"(if\s*\(\s*!?\s*\w+\s*\))"));

        for (const QString& method : std::as_const(methods)) {
            const auto range = findFunctionBody(lines, method);
            if (range.first < 0)
                continue; // メソッドが存在しない場合はスキップ

            const QString body = bodyText(lines, range.first, range.second);

            // deref() を含むメソッドのみチェック
            if (!body.contains(QStringLiteral("deref(")))
                continue;

            // deref() 呼び出しごとに、その後の行に null チェックがあること
            for (int i = range.first; i <= range.second; ++i) {
                if (!lines[i].contains(QStringLiteral("deref(")))
                    continue;

                // deref 行から 6行以内に null チェックがあること
                // （間にコンテキスト構築コードが入るケースがあるため余裕を持たせる）
                // 検出パターン:
                //   if (!var) / if (var) — 通常の null チェック
                //   var && — 短絡評価ガード
                //   var ? — 三項演算子ガード
                bool hasNullCheck = false;

                // deref の結果を格納する変数名を抽出
                const QRegularExpression assignRe(QStringLiteral(R"(auto\*?\s+(\w+)\s*=\s*deref)"));
                const auto assignMatch = assignRe.match(lines[i]);
                const QString varName = assignMatch.hasMatch() ? assignMatch.captured(1) : QString();

                for (int j = i; j <= qMin(i + 6, range.second); ++j) {
                    if (nullCheckRe.match(lines[j]).hasMatch()
                        || lines[j].contains(QStringLiteral("&&"))) {
                        hasNullCheck = true;
                        break;
                    }
                    // 三項演算子パターン: "varName ?" (deref 結果の変数に対する null チェック)
                    if (!varName.isEmpty() && lines[j].contains(varName + QStringLiteral(" ?"))) {
                        hasNullCheck = true;
                        break;
                    }
                }

                QVERIFY2(hasNullCheck,
                          qPrintable(QStringLiteral("deref() at line %1 in '%2' lacks null check within 6 lines")
                                         .arg(i + 1)
                                         .arg(method)));
            }
        }
    }

    /// deref() の前に ensure コールバックが呼ばれること（遅延初期化パターン）
    void gsoEnsureCallbackBeforeDeref()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gso source");

        // ensure コールバック + deref のペアが正しい順序で呼ばれるべきメソッド
        struct EnsureDerefPair {
            QString method;
            QString ensureCallback;
            QString derefTarget;
        };

        const QList<EnsureDerefPair> pairs = {
            {QStringLiteral("GameSessionOrchestrator::initializeGame()"),
             QStringLiteral("ensureGameStartCoordinator"),
             QStringLiteral("deref(m_deps.gameStart)")},
            {QStringLiteral("GameSessionOrchestrator::handleResignation()"),
             QStringLiteral("ensureGameStateController"),
             QStringLiteral("deref(m_deps.gameStateController)")},
            {QStringLiteral("GameSessionOrchestrator::handleBreakOffGame()"),
             QStringLiteral("ensureGameStateController"),
             QStringLiteral("deref(m_deps.gameStateController)")},
            {QStringLiteral("GameSessionOrchestrator::openWebsiteInExternalBrowser()"),
             QStringLiteral("ensureDialogCoordinator"),
             QStringLiteral("deref(m_deps.dialogCoordinator)")},
        };

        for (const auto& pair : std::as_const(pairs)) {
            const auto range = findFunctionBody(lines, pair.method);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method '%1' not found").arg(pair.method)));

            int ensureLine = -1;
            int derefLine = -1;
            for (int i = range.first; i <= range.second; ++i) {
                if (lines[i].contains(pair.ensureCallback) && ensureLine < 0)
                    ensureLine = i;
                if (lines[i].contains(pair.derefTarget) && derefLine < 0)
                    derefLine = i;
            }

            QVERIFY2(ensureLine >= 0,
                      qPrintable(QStringLiteral("ensure callback '%1' not found in '%2'")
                                     .arg(pair.ensureCallback, pair.method)));
            QVERIFY2(derefLine >= 0,
                      qPrintable(QStringLiteral("deref '%1' not found in '%2'")
                                     .arg(pair.derefTarget, pair.method)));
            QVERIFY2(ensureLine < derefLine,
                      qPrintable(QStringLiteral("ensure callback (line %1) should come before deref (line %2) in '%3'")
                                     .arg(ensureLine + 1)
                                     .arg(derefLine + 1)
                                     .arg(pair.method)));
        }
    }

    // ================================================================
    // 3. ポインタ安全性
    // ================================================================

    /// GameSessionOrchestrator の m_deps.playMode がデリファレンス前にチェックされること
    void gsoPlayModeDereferenceHasGuard()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gso source");

        // *m_deps.playMode が使われている全行で、同じ行または直前行に
        // m_deps.playMode のチェックがあること
        for (int i = 0; i < int(lines.size()); ++i) {
            const QString& line = lines[i];

            // *m_deps.playMode のデリファレンスを検出
            if (!line.contains(QStringLiteral("*m_deps.playMode")))
                continue;

            // 同じ行に m_deps.playMode && ガードがあるか、
            // 直前の if 文で m_deps.playMode を確認しているか
            bool hasGuard = line.contains(QStringLiteral("m_deps.playMode &&"))
                            || line.contains(QStringLiteral("m_deps.playMode ?"));

            if (!hasGuard) {
                // 3行以内の前方を確認
                for (int j = qMax(0, i - 3); j < i; ++j) {
                    if (lines[j].contains(QStringLiteral("if")) && lines[j].contains(QStringLiteral("m_deps.playMode"))) {
                        hasGuard = true;
                        break;
                    }
                }
            }

            QVERIFY2(hasGuard,
                      qPrintable(QStringLiteral("*m_deps.playMode dereference at line %1 lacks null guard")
                                     .arg(i + 1)));
        }
    }

    /// GameSessionOrchestrator の state ポインタ（currentSelectedPly 等）が
    /// デリファレンス前にチェックされること
    void gsoStatePointerDereferenceHasGuard()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesessionorchestrator.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gso source");

        // *m_deps.currentSelectedPly, *m_deps.bottomIsP1 など
        const QStringList statePointers = {
            QStringLiteral("*m_deps.currentSelectedPly"),
            QStringLiteral("*m_deps.bottomIsP1"),
            QStringLiteral("*m_deps.startSfenStr"),
            QStringLiteral("*m_deps.currentSfenStr"),
        };

        for (int i = 0; i < int(lines.size()); ++i) {
            const QString& line = lines[i];

            for (const QString& ptr : std::as_const(statePointers)) {
                if (!line.contains(ptr))
                    continue;

                // 三項演算子ガード "m_deps.xxx ? *m_deps.xxx : default"
                // または if ガード "if (m_deps.xxx)"
                const QString baseName = ptr.mid(1); // "*" を除去
                bool hasGuard = line.contains(baseName + QStringLiteral(" ?"))
                                || line.contains(baseName + QStringLiteral(" &&"));

                if (!hasGuard) {
                    // if ブロック内のネストされたデリファレンスも検出するため
                    // 6行前まで確認する（else if 分岐等で距離が離れるケース）
                    for (int j = qMax(0, i - 6); j < i; ++j) {
                        if (lines[j].contains(QStringLiteral("if")) && lines[j].contains(baseName)) {
                            hasGuard = true;
                            break;
                        }
                    }
                }

                QVERIFY2(hasGuard,
                          qPrintable(QStringLiteral("%1 dereference at line %2 lacks null guard")
                                         .arg(ptr)
                                         .arg(i + 1)));
            }
        }
    }

    // ================================================================
    // 4. 二重実行防止パターン
    // ================================================================

    /// LifecyclePipeline の runShutdown が二重実行で安全であること（構造確認）
    void shutdownDoubleCallProtectionPattern()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowLifecyclePipeline::runShutdown()"));
        QVERIFY2(range.first >= 0, "runShutdown() function not found");

        const QString body = bodyText(lines, range.first, range.second);

        // パターン: ガードチェック → フラグ設定 → 処理本体
        // 最初の非コメント文がガードであること
        bool guardIsFirst = false;
        for (int i = range.first + 1; i <= range.second; ++i) {
            const QString trimmed = lines[i].trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("//")))
                continue;

            guardIsFirst = trimmed.contains(QStringLiteral("m_shutdownDone"));
            break;
        }
        QVERIFY2(guardIsFirst,
                  "First non-comment statement in runShutdown() should be the guard check");

        // エンジン終了前に null チェックがあること（m_match が nullptr の場合の防御）
        QVERIFY2(body.contains(QStringLiteral("if (m_mw.m_match)")),
                  "runShutdown() should check m_match before destroyEngines()");
    }

    /// initMatchCoordinator が二重呼び出しでダングリングポインタを防止すること
    void initMatchCoordinatorPreventsDoubleInit()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamewiringsubregistry.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gamewiringsubregistry source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("GameWiringSubRegistry::initMatchCoordinator()"));
        QVERIFY2(range.first >= 0, "initMatchCoordinator() not found");

        const QString body = bodyText(lines, range.first, range.second);

        // 初期化前に既存の m_match を nullptr に設定すること（ダングリング防止）
        QVERIFY2(body.contains(QStringLiteral("m_mw.m_match = nullptr")),
                  "initMatchCoordinator should set m_match = nullptr before re-initialization");

        // 依存チェック（GC/View）があること
        QVERIFY2(body.contains(QStringLiteral("m_mw.m_gameController"))
                     && body.contains(QStringLiteral("m_mw.m_shogiView")),
                  "initMatchCoordinator should check core dependencies before initialization");
    }

    // ================================================================
    // 5. レジストリのクロスコール順序
    // ================================================================

    /// GameSubRegistry::ensureGameStateController が UiStatePolicyManager を
    /// 先に初期化すること（依存順序の契約）
    void ensureGameStateControllerDependencyOrder()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/gamesubregistry.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read gamesubregistry source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("GameSubRegistry::ensureGameStateController()"));
        QVERIFY2(range.first >= 0, "ensureGameStateController not found");

        // ガード（early return）の位置
        int guardLine = -1;
        // UiStatePolicyManager 初期化の位置
        int policyLine = -1;

        for (int i = range.first; i <= range.second; ++i) {
            if (lines[i].contains(QStringLiteral("m_mw.m_gameStateController")) && lines[i].contains(QStringLiteral("return")) && guardLine < 0)
                guardLine = i;
            if (lines[i].contains(QStringLiteral("ensureUiStatePolicyManager")) && policyLine < 0)
                policyLine = i;
        }

        QVERIFY2(guardLine >= 0, "Idempotent guard not found in ensureGameStateController");
        QVERIFY2(policyLine >= 0, "ensureUiStatePolicyManager call not found in ensureGameStateController");

        // ガードの後に UiStatePolicyManager 初期化があること
        // （ガードがパスしたときのみ初期化するのが正しい）
        QVERIFY2(guardLine < policyLine,
                  qPrintable(QStringLiteral("Guard (line %1) should come before ensureUiStatePolicyManager (line %2)")
                                 .arg(guardLine + 1)
                                 .arg(policyLine + 1)));
    }

    /// ServiceRegistry::ensurePositionEditCoordinator が
    /// UiStatePolicyManager を先に初期化すること
    void ensurePositionEditCoordinatorDependencyOrder()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowboardregistry.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read board registry source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("MainWindowServiceRegistry::ensurePositionEditCoordinator()"));
        QVERIFY2(range.first >= 0, "ensurePositionEditCoordinator not found");

        int guardLine = -1;
        int policyLine = -1;

        for (int i = range.first; i <= range.second; ++i) {
            if (lines[i].contains(QStringLiteral("m_mw.m_posEditCoordinator")) && lines[i].contains(QStringLiteral("return")) && guardLine < 0)
                guardLine = i;
            if (lines[i].contains(QStringLiteral("ensureUiStatePolicyManager")) && policyLine < 0)
                policyLine = i;
        }

        QVERIFY2(guardLine >= 0, "Idempotent guard not found");
        QVERIFY2(policyLine >= 0, "ensureUiStatePolicyManager call not found");
        QVERIFY2(guardLine < policyLine,
                  "Guard should come before cross-registry call");
    }

    // ================================================================
    // 6. SFEN パース異常系ユニットテスト
    // ================================================================

    /// parseSfen が空文字列を拒否すること
    void sfenParseRejectsEmptyString()
    {
        const auto result = ShogiBoard::parseSfen(QString());
        QVERIFY2(!result.has_value(), "parseSfen should reject empty string");
    }

    /// parseSfen がフィールド数不足の文字列を拒否すること
    void sfenParseRejectsInsufficientParts()
    {
        // 3フィールド（手数なし）
        const auto r1 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject 3-part SFEN");

        // 1フィールドのみ
        const auto r2 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject 1-part SFEN");

        // 5フィールド（余分）
        const auto r3 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 extra"));
        QVERIFY2(!r3.has_value(), "parseSfen should reject 5-part SFEN");
    }

    /// parseSfen が不正な手番文字を拒否すること
    void sfenParseRejectsInvalidTurn()
    {
        // "x" は "b" でも "w" でもない
        const auto r1 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject invalid turn 'x'");

        // 空手番
        const auto r2 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL  - 1"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject empty turn field");
    }

    /// parseSfen が不正な手数を拒否すること
    void sfenParseRejectsInvalidMoveNumber()
    {
        // 手数 0
        const auto r1 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 0"));
        QVERIFY2(!r1.has_value(), "parseSfen should reject move number 0");

        // 手数が負数
        const auto r2 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - -5"));
        QVERIFY2(!r2.has_value(), "parseSfen should reject negative move number");

        // 手数が非数値
        const auto r3 = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - abc"));
        QVERIFY2(!r3.has_value(), "parseSfen should reject non-numeric move number");
    }

    /// setSfen が不正な入力でクラッシュしないこと
    void sfenSetSfenHandlesCorruptInput()
    {
        ShogiBoard board;

        // 正常な初期配置を設定してから不正入力を渡す
        const QString validSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        board.setSfen(validSfen);
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 空文字列 → 盤面は変わらない（クラッシュしない）
        board.setSfen(QString());
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // バイナリっぽいゴミデータ → クラッシュしない
        board.setSfen(QStringLiteral("\x00\x01\x02\x03"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 不正な駒文字を含む → クラッシュしない
        board.setSfen(QStringLiteral("XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX/XXXXXXXXX b - 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // ランク数不足 → クラッシュしない
        board.setSfen(QStringLiteral("9/9/9 b - 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        // 不正な駒台文字列 → クラッシュしない
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b ZZZ 1"));
        QCOMPARE(board.currentPlayer(), Turn::Black);
    }

    /// parseSfen が正常な入力を正しくパースすること（正常系の対照テスト）
    void sfenParseAcceptsValidInput()
    {
        // 標準初期局面
        const auto r1 = ShogiBoard::parseSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QVERIFY2(r1.has_value(), "parseSfen should accept valid initial SFEN");
        QCOMPARE(r1->turn, Turn::Black);
        QCOMPARE(r1->moveNumber, 1);
        QCOMPARE(r1->stand, QStringLiteral("-"));

        // 後手番・持ち駒あり
        const auto r2 = ShogiBoard::parseSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w 2P 10"));
        QVERIFY2(r2.has_value(), "parseSfen should accept valid SFEN with pieces in hand");
        QCOMPARE(r2->turn, Turn::White);
        QCOMPARE(r2->moveNumber, 10);
        QCOMPARE(r2->stand, QStringLiteral("2P"));
    }

    // ================================================================
    // 7. 設定 getter のデフォルト値契約テスト（ソースコード構造検証）
    // ================================================================

    /// Settings の各 getter が QSettings::value() のデフォルト引数を持つこと
    void settingsGettersHaveDefaultValues()
    {
        // 検証対象ファイルとデフォルト値パターン
        struct SettingsFile {
            QString path;
            QStringList expectedDefaults;
        };

        const QList<SettingsFile> files = {
            {QStringLiteral("src/services/appsettings.cpp"),
             {QStringLiteral("\"system\""),   // language
              QStringLiteral("QSize("),       // window sizes
              QStringLiteral(", 0)"),         // tab index default
              QStringLiteral(", true)")}},    // toolbar visible
            {QStringLiteral("src/services/gamesettings.cpp"),
             {QStringLiteral("QSize("),       // dialog sizes
              QStringLiteral(", 10)")}},      // font size default
        };

        for (const auto& sf : std::as_const(files)) {
            const QString source = readSourceFile(sf.path);
            QVERIFY2(!source.isEmpty(),
                      qPrintable(QStringLiteral("Failed to read %1").arg(sf.path)));

            // .value() 呼び出しにデフォルト引数が含まれること
            QVERIFY2(source.contains(QStringLiteral(".value(")),
                      qPrintable(QStringLiteral("%1 has no .value() calls").arg(sf.path)));

            for (const QString& pattern : std::as_const(sf.expectedDefaults)) {
                QVERIFY2(source.contains(pattern),
                          qPrintable(QStringLiteral("Default pattern '%1' not found in %2")
                                         .arg(pattern, sf.path)));
            }

            // 全ての .value() 呼び出しにデフォルト引数（第2引数）があること
            const QRegularExpression valueCallRe(
                QStringLiteral(R"(\.value\([^,)]+\))"));
            auto it = valueCallRe.globalMatch(source);
            int noDefaultCount = 0;
            while (it.hasNext()) {
                it.next();
                ++noDefaultCount;
            }
            QVERIFY2(noDefaultCount == 0,
                      qPrintable(QStringLiteral("%1 has %2 .value() call(s) without default argument")
                                     .arg(sf.path)
                                     .arg(noDefaultCount)));
        }
    }

    // ================================================================
    // 8. CSA クライアント 状態ガード契約テスト（ソースコード構造検証）
    // ================================================================

    /// CsaClient の状態依存メソッドが不正状態で早期リターンすること
    void csaClientStateGuardsExist()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/network/csaclient.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read csaclient source");

        // 各メソッドが状態チェックガードを持つこと
        struct MethodGuard {
            QString signature;
            QString expectedStateCheck;
        };

        const QList<MethodGuard> guards = {
            {QStringLiteral("CsaClient::connectToServer("),
             QStringLiteral("Disconnected")},
            {QStringLiteral("CsaClient::agree("),
             QStringLiteral("GameReady")},
            {QStringLiteral("CsaClient::sendMove("),
             QStringLiteral("InGame")},
            {QStringLiteral("CsaClient::reject("),
             QStringLiteral("GameReady")},
        };

        for (const auto& guard : std::as_const(guards)) {
            const auto range = findFunctionBody(lines, guard.signature);
            QVERIFY2(range.first >= 0,
                      qPrintable(QStringLiteral("Method '%1' not found").arg(guard.signature)));

            const QString body = bodyText(lines, range.first, range.second);

            // 状態チェック (if (m_connectionState != State) { ... return; }) が存在すること
            QVERIFY2(body.contains(QStringLiteral("m_connectionState"))
                         && body.contains(guard.expectedStateCheck),
                      qPrintable(QStringLiteral("Method '%1' lacks state guard for %2")
                                     .arg(guard.signature, guard.expectedStateCheck)));

            // errorOccurred シグナルまたは早期リターンがあること
            QVERIFY2(body.contains(QStringLiteral("errorOccurred"))
                         || body.contains(QStringLiteral("return")),
                      qPrintable(QStringLiteral("Method '%1' lacks error signal or early return")
                                     .arg(guard.signature)));
        }
    }

    // ================================================================
    // 9. USI 待機メソッドのタイムアウト保護契約テスト（ソースコード構造検証）
    // ================================================================

    /// USI の wait メソッドがタイムアウト保護を持つこと
    void usiWaitMethodsHaveTimeoutProtection()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/engine/usiprotocolhandler_wait.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read usi protocol handler wait source");

        // waitForResponseFlag 共通実装にタイムアウト機構があること
        {
            const auto range = findFunctionBody(
                lines, QStringLiteral("UsiProtocolHandler::waitForResponseFlag("));
            QVERIFY2(range.first >= 0, "waitForResponseFlag() not found");

            const QString body = bodyText(lines, range.first, range.second);
            QVERIFY2(body.contains(QStringLiteral("QTimer"))
                         || body.contains(QStringLiteral("timeout")),
                      "waitForResponseFlag should use timer-based timeout");
            QVERIFY2(body.contains(QStringLiteral("QEventLoop")),
                      "waitForResponseFlag should use QEventLoop for non-blocking wait");
        }

        // waitForBestMove にタイムアウト判定があること
        {
            const auto range = findFunctionBody(
                lines, QStringLiteral("UsiProtocolHandler::waitForBestMove("));
            QVERIFY2(range.first >= 0, "waitForBestMove() not found");

            const QString body = bodyText(lines, range.first, range.second);
            QVERIFY2(body.contains(QStringLiteral("elapsed"))
                         && body.contains(QStringLiteral("timeoutMs")),
                      "waitForBestMove should check elapsed time against timeout");
            QVERIFY2(body.contains(QStringLiteral("return false")),
                      "waitForBestMove should return false on timeout");
        }

        // 各 wait メソッドが [[nodiscard]] であること（ヘッダーの構造検証）
        {
            const QString header = readSourceFile(
                QStringLiteral("src/engine/usiprotocolhandler.h"));
            QVERIFY2(!header.isEmpty(), "Failed to read usi protocol handler header");

            const QStringList waitMethods = {
                QStringLiteral("waitForUsiOk"),
                QStringLiteral("waitForReadyOk"),
                QStringLiteral("waitForBestMove"),
                QStringLiteral("waitForBestMoveWithGrace"),
                QStringLiteral("keepWaitingForBestMove"),
            };

            for (const QString& method : std::as_const(waitMethods)) {
                QVERIFY2(header.contains(QStringLiteral("[[nodiscard]]"))
                             && header.contains(method),
                          qPrintable(QStringLiteral("Wait method '%1' should be [[nodiscard]]")
                                         .arg(method)));
            }
        }
    }

    // ================================================================
    // 10. LifecyclePipeline 二重初期化防止テスト（拡充）
    // ================================================================

    /// LifecyclePipeline の runStartup がオブジェクト生成を含み、
    /// runShutdown のガードフラグが初期値 false であること
    void lifecyclePipelineDoubleInitProtection()
    {
        const QStringList lines = readSourceLines(
            QStringLiteral("src/app/mainwindowlifecyclepipeline.cpp"));
        QVERIFY2(!lines.isEmpty(), "Failed to read pipeline source");

        // runStartup に基盤オブジェクト生成ステップがあること
        {
            const auto range = findFunctionBody(
                lines, QStringLiteral("MainWindowLifecyclePipeline::runStartup()"));
            QVERIFY2(range.first >= 0, "runStartup() not found");

            const QString body = bodyText(lines, range.first, range.second);
            QVERIFY2(body.contains(QStringLiteral("createFoundation"))
                         || body.contains(QStringLiteral("make_unique"))
                         || body.contains(QStringLiteral("setupUi")),
                      "runStartup should create foundation objects");
        }

        // m_shutdownDone の初期値が false であること（ヘッダーの構造検証）
        {
            const QString header = readSourceFile(
                QStringLiteral("src/app/mainwindowlifecyclepipeline.h"));
            QVERIFY2(!header.isEmpty(), "Failed to read pipeline header");

            QVERIFY2(header.contains(QStringLiteral("m_shutdownDone"))
                         && header.contains(QStringLiteral("false")),
                      "m_shutdownDone should be initialized to false in header");
        }
    }
};

QTEST_MAIN(TestAppErrorHandling)

#include "tst_app_error_handling.moc"
