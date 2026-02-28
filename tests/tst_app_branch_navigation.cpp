/// @file tst_app_branch_navigation.cpp
/// @brief BranchNavigationWiring の構造的契約テスト
///
/// BranchNavigationWiring は多数のポインタのポインタ依存を持つため、
/// 単体インスタンス化ではなくソース解析テストで構造的契約を検証する。

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestAppBranchNavigation : public QObject
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

    static const QString kHeader;
    static const QString kSource;

    static QString headerText()
    {
        return readSourceFile(kHeader);
    }

    static QStringList sourceLines()
    {
        return readSourceLines(kSource);
    }

private slots:
    // ================================================================
    // ヘッダー契約テスト
    // ================================================================

    /// BranchNavigationWiring が QObject を継承していること
    void inheritsQObject()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");
        QVERIFY2(hdr.contains(QStringLiteral("public QObject")),
                 "BranchNavigationWiring does not inherit QObject");
    }

    /// Deps 構造体に必要なダブルポインタが宣言されていること
    void depsHasRequiredDoublePointers()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredPointers = {
            QStringLiteral("KifuBranchTree**"),
            QStringLiteral("KifuNavigationState**"),
            QStringLiteral("KifuNavigationController**"),
            QStringLiteral("KifuDisplayCoordinator**"),
            QStringLiteral("BranchTreeWidget**"),
            QStringLiteral("LiveGameSession**"),
        };

        for (const QString& ptr : requiredPointers) {
            QVERIFY2(hdr.contains(ptr),
                      qPrintable(QStringLiteral("Deps missing double pointer: %1").arg(ptr)));
        }
    }

    /// Deps 構造体に必要な直接ポインタが宣言されていること
    void depsHasRequiredDirectPointers()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredPointers = {
            QStringLiteral("RecordPane*"),
            QStringLiteral("EngineAnalysisTab*"),
            QStringLiteral("GameRecordModel*"),
            QStringLiteral("ShogiGameController*"),
        };

        for (const QString& ptr : requiredPointers) {
            QVERIFY2(hdr.contains(ptr),
                      qPrintable(QStringLiteral("Deps missing pointer: %1").arg(ptr)));
        }
    }

    /// 必要なシグナルが宣言されていること
    void hasRequiredSignals()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredSignals = {
            QStringLiteral("boardWithHighlightsRequired"),
            QStringLiteral("boardSfenChanged"),
            QStringLiteral("branchNodeHandled"),
        };

        for (const QString& sig : requiredSignals) {
            QVERIFY2(hdr.contains(sig),
                      qPrintable(QStringLiteral("Missing signal: %1").arg(sig)));
        }
    }

    /// 必要な public slots が宣言されていること
    void hasRequiredSlots()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");

        const QStringList requiredSlots = {
            QStringLiteral("onBranchTreeBuilt"),
            QStringLiteral("onBranchNodeActivated"),
            QStringLiteral("onBranchTreeResetForNewGame"),
        };

        for (const QString& slot : requiredSlots) {
            QVERIFY2(hdr.contains(slot),
                      qPrintable(QStringLiteral("Missing slot: %1").arg(slot)));
        }
    }

    /// ensureCommentCoordinator コールバックが Deps に存在すること
    void depsHasEnsureCommentCoordinatorCallback()
    {
        const QString hdr = headerText();
        QVERIFY2(!hdr.isEmpty(), "Failed to read header");
        QVERIFY2(hdr.contains(QStringLiteral("ensureCommentCoordinator")),
                 "Deps missing ensureCommentCoordinator callback");
    }

    // ================================================================
    // initialize() 契約テスト
    // ================================================================

    /// initialize() が createModels と wireSignals を呼ぶこと
    void initializeCallsCreateModelsAndWireSignals()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::initialize()"));
        QVERIFY2(range.first >= 0, "initialize() function not found");

        const QString body = bodyText(lines, range);
        QVERIFY2(body.contains(QStringLiteral("createModels")),
                 "initialize() does not call createModels()");
        QVERIFY2(body.contains(QStringLiteral("wireSignals")),
                 "initialize() does not call wireSignals()");
    }

    // ================================================================
    // createModels() 契約テスト
    // ================================================================

    /// createModels() が4つのオブジェクトを生成すること
    void createModelsCreatesRequiredObjects()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::createModels()"));
        QVERIFY2(range.first >= 0, "createModels() function not found");

        const QString body = bodyText(lines, range);

        // 分岐ツリー生成
        QVERIFY2(body.contains(QStringLiteral("KifuBranchTree")),
                 "createModels() does not create KifuBranchTree");
        // ナビゲーション状態
        QVERIFY2(body.contains(QStringLiteral("KifuNavigationState")),
                 "createModels() does not create KifuNavigationState");
        // ナビゲーションコントローラ
        QVERIFY2(body.contains(QStringLiteral("KifuNavigationController")),
                 "createModels() does not create KifuNavigationController");
        // ライブゲームセッション
        QVERIFY2(body.contains(QStringLiteral("LiveGameSession")),
                 "createModels() does not create LiveGameSession");
    }

    /// createModels() がツリーとナビ状態を関連付けること
    void createModelsSetsTreeOnNavState()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::createModels()"));
        QVERIFY2(range.first >= 0, "createModels() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setTree")),
                 "createModels() does not call setTree on navigation state");
        QVERIFY2(body.contains(QStringLiteral("setTreeAndState")),
                 "createModels() does not call setTreeAndState on navigation controller");
    }

    // ================================================================
    // onBranchTreeBuilt() 契約テスト
    // ================================================================

    /// onBranchTreeBuilt() がナビ状態とツリーウィジェットを更新すること
    void onBranchTreeBuiltUpdatesNavAndWidget()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchTreeBuilt()"));
        QVERIFY2(range.first >= 0, "onBranchTreeBuilt() function not found");

        const QString body = bodyText(lines, range);

        // ナビ状態にツリーを再設定
        QVERIFY2(body.contains(QStringLiteral("setTree")),
                 "onBranchTreeBuilt() does not call setTree");
        // 表示コーディネーターに変更通知
        QVERIFY2(body.contains(QStringLiteral("onTreeChanged")),
                 "onBranchTreeBuilt() does not notify display coordinator");
    }

    /// onBranchTreeBuilt() が GameRecordModel を更新すること
    void onBranchTreeBuiltUpdatesGameRecordModel()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchTreeBuilt()"));
        QVERIFY2(range.first >= 0, "onBranchTreeBuilt() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setBranchTree")),
                 "onBranchTreeBuilt() does not call setBranchTree on GameRecordModel");
        QVERIFY2(body.contains(QStringLiteral("setNavigationState")),
                 "onBranchTreeBuilt() does not call setNavigationState on GameRecordModel");
    }

    // ================================================================
    // onBranchNodeActivated() 契約テスト
    // ================================================================

    /// onBranchNodeActivated() が navCtl に委譲すること
    void onBranchNodeActivatedDelegatesToNavController()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchNodeActivated("));
        QVERIFY2(range.first >= 0, "onBranchNodeActivated() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("handleBranchNodeActivated")),
                 "onBranchNodeActivated() does not delegate to handleBranchNodeActivated");
    }

    /// onBranchNodeActivated() が navCtl の null チェックを行うこと
    void onBranchNodeActivatedChecksNull()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchNodeActivated("));
        QVERIFY2(range.first >= 0, "onBranchNodeActivated() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("nullptr")),
                 "onBranchNodeActivated() does not check for null");
    }

    // ================================================================
    // onBranchTreeResetForNewGame() 契約テスト
    // ================================================================

    /// onBranchTreeResetForNewGame() がライブセッションを破棄すること
    void onResetDiscardsLiveSession()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchTreeResetForNewGame()"));
        QVERIFY2(range.first >= 0, "onBranchTreeResetForNewGame() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("discard")),
                 "onBranchTreeResetForNewGame() does not discard live session");
    }

    /// onBranchTreeResetForNewGame() がナビ状態をリセットすること
    void onResetResetsNavigationState()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchTreeResetForNewGame()"));
        QVERIFY2(range.first >= 0, "onBranchTreeResetForNewGame() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setCurrentNode")),
                 "onBranchTreeResetForNewGame() does not reset current node");
        QVERIFY2(body.contains(QStringLiteral("resetPreferredLineIndex")),
                 "onBranchTreeResetForNewGame() does not reset preferred line index");
        QVERIFY2(body.contains(QStringLiteral("goToRoot")),
                 "onBranchTreeResetForNewGame() does not go to root");
    }

    /// onBranchTreeResetForNewGame() が分岐ツリーの SFEN をリセットすること
    void onResetSetsRootSfen()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::onBranchTreeResetForNewGame()"));
        QVERIFY2(range.first >= 0, "onBranchTreeResetForNewGame() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setRootSfen")),
                 "onBranchTreeResetForNewGame() does not call setRootSfen");
    }

    // ================================================================
    // シグナル転送契約テスト
    // ================================================================

    /// connectDisplaySignals() が boardWithHighlightsRequired を転送すること
    void connectsDisplaySignalForBoardWithHighlights()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"));
        QVERIFY2(range.first >= 0, "connectDisplaySignals() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("boardWithHighlightsRequired")),
                 "connectDisplaySignals() does not forward boardWithHighlightsRequired");
    }

    /// connectDisplaySignals() が boardSfenChanged を転送すること
    void connectsDisplaySignalForBoardSfenChanged()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"));
        QVERIFY2(range.first >= 0, "connectDisplaySignals() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("boardSfenChanged")),
                 "connectDisplaySignals() does not forward boardSfenChanged");
    }

    /// connectDisplaySignals() が branchNodeHandled を転送すること
    void connectsDisplaySignalForBranchNodeHandled()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"));
        QVERIFY2(range.first >= 0, "connectDisplaySignals() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("branchNodeHandled")),
                 "connectDisplaySignals() does not forward branchNodeHandled");
    }

    /// connectDisplaySignals() がコメント更新を CommentCoordinator に接続すること
    void connectsCommentUpdateSignal()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"));
        QVERIFY2(range.first >= 0, "connectDisplaySignals() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("commentUpdateRequired")),
                 "connectDisplaySignals() does not connect commentUpdateRequired");
        QVERIFY2(body.contains(QStringLiteral("CommentCoordinator")),
                 "connectDisplaySignals() does not reference CommentCoordinator");
    }

    /// connectDisplaySignals() が遅延初期化コールバックを呼ぶこと
    void connectsDisplaySignalsCallsEnsureCommentCoordinator()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"));
        QVERIFY2(range.first >= 0, "connectDisplaySignals() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("ensureCommentCoordinator")),
                 "connectDisplaySignals() does not call ensureCommentCoordinator callback");
    }

    // ================================================================
    // configureDisplayCoordinator() 契約テスト
    // ================================================================

    /// configureDisplayCoordinator() がモデルとウィジェットを設定すること
    void configureDisplayCoordinatorSetsModelsAndWidgets()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::configureDisplayCoordinator()"));
        QVERIFY2(range.first >= 0, "configureDisplayCoordinator() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setRecordPane")),
                 "configureDisplayCoordinator() does not call setRecordPane");
        QVERIFY2(body.contains(QStringLiteral("setRecordModel")),
                 "configureDisplayCoordinator() does not call setRecordModel");
        QVERIFY2(body.contains(QStringLiteral("setBranchModel")),
                 "configureDisplayCoordinator() does not call setBranchModel");
    }

    /// configureDisplayCoordinator() が wireSignals を呼ぶこと
    void configureDisplayCoordinatorCallsWireSignals()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::configureDisplayCoordinator()"));
        QVERIFY2(range.first >= 0, "configureDisplayCoordinator() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("wireSignals()")),
                 "configureDisplayCoordinator() does not call dc->wireSignals()");
    }

    /// configureDisplayCoordinator() が盤面SFENプロバイダを設定すること
    void configureDisplayCoordinatorSetsBoardSfenProvider()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const auto range = findFunctionBody(
            lines, QStringLiteral("BranchNavigationWiring::configureDisplayCoordinator()"));
        QVERIFY2(range.first >= 0, "configureDisplayCoordinator() function not found");

        const QString body = bodyText(lines, range);

        QVERIFY2(body.contains(QStringLiteral("setBoardSfenProvider")),
                 "configureDisplayCoordinator() does not set board SFEN provider");
    }

    // ================================================================
    // 全体構造テスト
    // ================================================================

    /// ソースファイルに全 private メソッドの実装が存在すること
    void allPrivateMethodsImplemented()
    {
        const QStringList lines = sourceLines();
        QVERIFY2(!lines.isEmpty(), "Failed to read source");

        const QString source = lines.join(QLatin1Char('\n'));
        const QStringList expectedImpls = {
            QStringLiteral("BranchNavigationWiring::createModels()"),
            QStringLiteral("BranchNavigationWiring::wireSignals()"),
            QStringLiteral("BranchNavigationWiring::configureDisplayCoordinator()"),
            QStringLiteral("BranchNavigationWiring::connectDisplaySignals()"),
        };

        for (const QString& impl : expectedImpls) {
            QVERIFY2(source.contains(impl),
                      qPrintable(
                          QStringLiteral("Implementation of '%1' not found in source").arg(impl)));
        }
    }
};

const QString TestAppBranchNavigation::kHeader =
    QStringLiteral("src/ui/wiring/branchnavigationwiring.h");
const QString TestAppBranchNavigation::kSource =
    QStringLiteral("src/ui/wiring/branchnavigationwiring.cpp");

QTEST_MAIN(TestAppBranchNavigation)

#include "tst_app_branch_navigation.moc"
