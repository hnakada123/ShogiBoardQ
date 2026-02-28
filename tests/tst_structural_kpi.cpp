#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMap>
#include <QTextStream>

// CMAKE_SOURCE_DIR is defined via target_compile_definitions
#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestStructuralKpi : public QObject
{
    Q_OBJECT

private:
    static int countLines(const QString &filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;
        int count = 0;
        while (!file.atEnd()) {
            file.readLine();
            ++count;
        }
        return count;
    }

    static int countMatchingLines(const QString &filePath, const QRegularExpression &pattern)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;
        QTextStream in(&file);
        int count = 0;
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (pattern.match(line).hasMatch())
                ++count;
        }
        return count;
    }

    // 既知の例外リスト: ファイルパス(src/からの相対) → 現在の行数上限
    // リファクタリングでファイルが縮小されたら、この値も下げること。
    // 例外リスト外のファイルが600行を超えたらテスト失敗となる。
    // 新しい例外を追加する場合は、削減計画の Issue 番号を同時に記載すること。
    // 2026-02-28 実測値と一致を確認済み。
    static QMap<QString, int> knownLargeFiles()
    {
        return {
            // --- 1000行超 → 解消済み ---
            {"src/engine/usi.cpp", 781},                       // ISSUE-053 USIエンジン分割（1084→781）
            {"src/widgets/evaluationchartwidget.cpp", 635},    // ISSUE-054 評価グラフ描画分離（1018→635）
            // --- 900行超 ---
            {"src/kifu/kifuexportcontroller.cpp", 654},  // ISSUE-055 棋譜エクスポート形式分離（933→654）
            {"src/core/fmvlegalcore.cpp", 930},          // TODO: ISSUE-056 合法手生成テーブル化
            {"src/engine/usiprotocolhandler.cpp", 714},   // ISSUE-053 USIエンジン分割（902→714）
            // --- 800行超 ---
            {"src/dialogs/pvboarddialog.cpp", 418},               // ISSUE-057 ダイアログ分割（884→418）
            {"src/dialogs/engineregistrationdialog.cpp", 842},    // TODO: ISSUE-057 ダイアログ分割
            {"src/dialogs/josekimovedialog.cpp", 838},            // TODO: ISSUE-057 ダイアログ分割
            {"src/core/shogiboard.cpp", 822},                     // TODO: ISSUE-056 コアロジック分割
            {"src/views/shogiview_draw.cpp", 808},                // TODO: ISSUE-058 描画ロジック分離
            {"src/dialogs/changeenginesettingsdialog.cpp", 804},  // TODO: ISSUE-057 ダイアログ分割
            {"src/views/shogiview.cpp", 802},                     // TODO: ISSUE-058 描画ロジック分離
            {"src/network/csagamecoordinator.cpp", 555},          // ISSUE-059 CSA通信分割（798→555）
            {"src/game/matchcoordinator.cpp", 789},               // TODO: ISSUE-060 MC責務移譲継続
            // --- 700行超 ---
            {"src/kifu/formats/csaexporter.cpp", 758},                // TODO: ISSUE-055 棋譜エクスポート形式分離
            {"src/kifu/formats/jkfexporter.cpp", 735},                // TODO: ISSUE-055 棋譜エクスポート形式分離
            {"src/game/gamestartcoordinator.cpp", 727},               // TODO: ISSUE-060 ゲーム管理分割
            {"src/ui/coordinators/kifudisplaycoordinator.cpp", 720},  // TODO: ISSUE-061 UIコーディネータ分割
            {"src/views/shogiview_labels.cpp", 704},                  // TODO: ISSUE-058 描画ロジック分離
            // --- 600行超 ---
            {"src/network/csaclient.cpp", 693},                    // TODO: ISSUE-059 CSA通信分割
            {"src/engine/shogiengineinfoparser.cpp", 689},         // TODO: ISSUE-053 USIエンジン分割
            {"src/widgets/recordpane.cpp", 430},                   // ISSUE-062 ウィジェット分割（677→430）
            {"src/analysis/analysisflowcontroller.cpp", 676},        // ISSUE-052 解析フロー分割（1133→676）
            {"src/dialogs/tsumeshogigeneratordialog.cpp", 657},    // TODO: ISSUE-057 ダイアログ分割
            {"src/game/shogigamecontroller.cpp", 652},             // TODO: ISSUE-060 ゲーム管理分割
            {"src/navigation/kifunavigationcontroller.cpp", 642},  // TODO: ISSUE-063 ナビゲーション分割
            {"src/kifu/formats/parsecommon.cpp", 641},             // TODO: ISSUE-055 棋譜フォーマット分割
            {"src/kifu/kifuapplyservice.cpp", 638},                // TODO: ISSUE-055 棋譜サービス分割
            {"src/dialogs/startgamedialog.cpp", 638},              // TODO: ISSUE-057 ダイアログ分割
            {"src/widgets/considerationtabmanager.cpp", 634},      // TODO: ISSUE-062 ウィジェット分割
            {"src/widgets/branchtreemanager.cpp", 632},            // TODO: ISSUE-062 ウィジェット分割
            {"src/widgets/engineanalysistab.cpp", 607},            // TODO: ISSUE-062 ウィジェット分割
        };
    }

private slots:
    // ================================================================
    // a) ファイル行数上限テスト
    //    - src/ 配下の .cpp/.h ファイルが 600行以下であることを検証
    //    - 既知の例外リストにあるファイルは、登録された上限値で検証
    // ================================================================
    void fileLineLimits()
    {
        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");
        constexpr int defaultLimit = 600;

        const QMap<QString, int> exceptions = knownLargeFiles();

        QDirIterator it(srcPath, {"*.cpp", "*.h"}, QDir::Files, QDirIterator::Subdirectories);

        QStringList violations;
        QStringList shrunkFiles;  // 例外リストより縮小されたファイル

        while (it.hasNext()) {
            it.next();
            const QString absPath = it.filePath();
            const QString relPath = QDir(sourceDir).relativeFilePath(absPath);
            const int lines = countLines(absPath);

            if (lines < 0)
                continue;

            auto exIt = exceptions.find(relPath);
            if (exIt != exceptions.end()) {
                // 既知の例外: 登録された上限を超えていないか
                const int allowedLimit = exIt.value();
                if (lines > allowedLimit) {
                    violations.append(
                        QStringLiteral("%1: %2 lines (allowed: %3)")
                            .arg(relPath)
                            .arg(lines)
                            .arg(allowedLimit));
                }
                // 縮小された場合は報告（上限値の更新を促す）
                if (lines < allowedLimit - 10) {
                    shrunkFiles.append(
                        QStringLiteral("%1: %2 lines (allowed: %3) -> update to %2")
                            .arg(relPath)
                            .arg(lines)
                            .arg(allowedLimit));
                }
            } else {
                // 例外リスト外: デフォルト上限を超えていないか
                if (lines > defaultLimit) {
                    violations.append(
                        QStringLiteral("%1: %2 lines (limit: %3, not in exception list)")
                            .arg(relPath)
                            .arg(lines)
                            .arg(defaultLimit));
                }
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: exception_file_count = %1").arg(exceptions.size());

        if (!shrunkFiles.isEmpty()) {
            qDebug().noquote() << "\n=== Files shrunk below their exception limit (update knownLargeFiles) ===";
            for (const auto &msg : std::as_const(shrunkFiles))
                qDebug().noquote() << "  " << msg;
        }

        if (!violations.isEmpty()) {
            QString msg = QStringLiteral("File line limit violations:\n");
            for (const auto &v : std::as_const(violations))
                msg += QStringLiteral("  ") + v + QStringLiteral("\n");
            QFAIL(qPrintable(msg));
        }
    }

    // ================================================================
    // b) MainWindow friend class 上限テスト
    // ================================================================
    void mainWindowFriendClassLimit()
    {
        const QString headerPath =
            QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/app/mainwindow.h");
        constexpr int maxFriendClasses = 3;

        const QRegularExpression pattern(QStringLiteral(R"(^\s*friend\s+class\s+)"));
        const int count = countMatchingLines(headerPath, pattern);

        QVERIFY2(count >= 0, "Failed to read mainwindow.h");

        qDebug().noquote()
            << QStringLiteral("KPI: mw_friend_classes = %1 (limit: %2)")
                   .arg(count)
                   .arg(maxFriendClasses);

        QVERIFY2(count <= maxFriendClasses,
                 qPrintable(QStringLiteral("MainWindow friend class count: %1 (limit: %2)")
                                .arg(count)
                                .arg(maxFriendClasses)));
    }

    // ================================================================
    // c) MainWindow ensure* 上限テスト
    // ================================================================
    void mainWindowEnsureLimit()
    {
        const QString headerPath =
            QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/app/mainwindow.h");
        constexpr int maxEnsureMethods = 0;

        const QRegularExpression pattern(QStringLiteral(R"(^\s*void\s+ensure)"));
        const int count = countMatchingLines(headerPath, pattern);

        QVERIFY2(count >= 0, "Failed to read mainwindow.h");

        qDebug().noquote()
            << QStringLiteral("KPI: mw_ensure_methods = %1 (limit: %2)")
                   .arg(count)
                   .arg(maxEnsureMethods);

        QVERIFY2(count <= maxEnsureMethods,
                 qPrintable(QStringLiteral("MainWindow ensure* count: %1 (limit: %2)")
                                .arg(count)
                                .arg(maxEnsureMethods)));
    }

    // ================================================================
    // c-2) ServiceRegistry ensure* 上限テスト
    // ================================================================
    void serviceRegistryEnsureLimit()
    {
        const QString headerPath =
            QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/app/mainwindowserviceregistry.h");
        constexpr int maxEnsureMethods = 35;

        const QRegularExpression pattern(QStringLiteral(R"(^\s*void\s+ensure)"));
        const int count = countMatchingLines(headerPath, pattern);

        QVERIFY2(count >= 0, "Failed to read mainwindowserviceregistry.h");

        qDebug().noquote()
            << QStringLiteral("KPI: sr_ensure_methods = %1 (limit: %2)")
                   .arg(count)
                   .arg(maxEnsureMethods);

        QVERIFY2(count <= maxEnsureMethods,
                 qPrintable(QStringLiteral("ServiceRegistry ensure* count: %1 (limit: %2)")
                                .arg(count)
                                .arg(maxEnsureMethods)));
    }

    // ================================================================
    // d) 1000行超ファイル数の上限チェック
    //    src/ 配下の .cpp/.h で1000行を超えるファイルの総数を監視
    // ================================================================
    void largeFileCount()
    {
        const int kMaxLargeFiles = 1;        // 現状0 + マージン1
        const int kLargeFileThreshold = 1000;

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        QDirIterator it(srcPath, {"*.cpp", "*.h"}, QDir::Files, QDirIterator::Subdirectories);

        QStringList largeFiles;
        while (it.hasNext()) {
            it.next();
            const int lines = countLines(it.filePath());
            if (lines > kLargeFileThreshold) {
                largeFiles.append(
                    QStringLiteral("%1: %2 lines")
                        .arg(QDir(sourceDir).relativeFilePath(it.filePath()))
                        .arg(lines));
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: files_over_1000 = %1 (limit: %2)")
                   .arg(largeFiles.size())
                   .arg(kMaxLargeFiles);

        if (!largeFiles.isEmpty()) {
            qDebug().noquote() << "  Large files:";
            for (const auto &f : std::as_const(largeFiles))
                qDebug().noquote() << "    " << f;
        }

        QVERIFY2(largeFiles.size() <= kMaxLargeFiles,
                 qPrintable(QStringLiteral("Too many large files (>%1 lines): %2 (limit: %3)")
                                .arg(kLargeFileThreshold)
                                .arg(largeFiles.size())
                                .arg(kMaxLargeFiles)));
    }

    // ================================================================
    // e) delete 件数の上限チェック
    //    src/ 配下の .cpp で delete 文の出現数を監視
    // ================================================================
    void deleteCount()
    {
        const int kMaxDeleteCount = 3; // 現状1 + マージン2

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        // delete 文にマッチ（コメント行・#include行は除外）
        const QRegularExpression deletePattern(QStringLiteral(R"(\bdelete\b)"));
        const QRegularExpression commentPattern(QStringLiteral(R"(^\s*//)"));
        const QRegularExpression includePattern(QStringLiteral(R"(^\s*#include)"));

        QDirIterator it(srcPath, {"*.cpp"}, QDir::Files, QDirIterator::Subdirectories);

        int totalCount = 0;
        QStringList details;

        while (it.hasNext()) {
            it.next();
            QFile file(it.filePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            int fileCount = 0;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (commentPattern.match(line).hasMatch())
                    continue;
                if (includePattern.match(line).hasMatch())
                    continue;
                if (deletePattern.match(line).hasMatch())
                    ++fileCount;
            }

            if (fileCount > 0) {
                totalCount += fileCount;
                details.append(
                    QStringLiteral("%1: %2")
                        .arg(QDir(sourceDir).relativeFilePath(it.filePath()))
                        .arg(fileCount));
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: delete_count = %1 (limit: %2)")
                   .arg(totalCount)
                   .arg(kMaxDeleteCount);
        for (const auto &d : std::as_const(details))
            qDebug().noquote() << "    " << d;

        QVERIFY2(totalCount <= kMaxDeleteCount,
                 qPrintable(QStringLiteral("Too many delete statements: %1 (limit: %2)")
                                .arg(totalCount)
                                .arg(kMaxDeleteCount)));
    }

    // ================================================================
    // f) lambda connect 件数の上限チェック
    //    src/ 配下の .cpp で connect() にラムダを使用している箇所を監視
    // ================================================================
    void lambdaConnectCount()
    {
        const int kMaxLambdaConnects = 1; // 現状0 + マージン1

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        // connect( を含む行で [ も含む行（ラムダキャプチャの典型パターン）
        const QRegularExpression connectPattern(QStringLiteral(R"(connect\s*\()"));
        const QRegularExpression lambdaPattern(QStringLiteral(R"(\[)"));
        const QRegularExpression commentPattern(QStringLiteral(R"(^\s*//)"));

        QDirIterator it(srcPath, {"*.cpp"}, QDir::Files, QDirIterator::Subdirectories);

        int totalCount = 0;
        QStringList details;

        while (it.hasNext()) {
            it.next();
            QFile file(it.filePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            int fileCount = 0;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (commentPattern.match(line).hasMatch())
                    continue;
                if (connectPattern.match(line).hasMatch()
                    && lambdaPattern.match(line).hasMatch()) {
                    ++fileCount;
                }
            }

            if (fileCount > 0) {
                totalCount += fileCount;
                details.append(
                    QStringLiteral("%1: %2")
                        .arg(QDir(sourceDir).relativeFilePath(it.filePath()))
                        .arg(fileCount));
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: lambda_connect_count = %1 (limit: %2)")
                   .arg(totalCount)
                   .arg(kMaxLambdaConnects);
        for (const auto &d : std::as_const(details))
            qDebug().noquote() << "    " << d;

        QVERIFY2(totalCount <= kMaxLambdaConnects,
                 qPrintable(QStringLiteral("Too many lambda connects: %1 (limit: %2)")
                                .arg(totalCount)
                                .arg(kMaxLambdaConnects)));
    }

    // ================================================================
    // g) 旧式 SIGNAL/SLOT マクロ件数の上限チェック
    //    src/ 配下の .cpp で SIGNAL( または SLOT( の出現数を監視
    // ================================================================
    void oldStyleConnectionCount()
    {
        const int kMaxOldStyleConnections = 0;

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        // SIGNAL( または SLOT( にマッチ（コメント行は除外）
        const QRegularExpression macroPattern(QStringLiteral(R"(\b(SIGNAL|SLOT)\s*\()"));
        const QRegularExpression commentPattern(QStringLiteral(R"(^\s*//)"));

        QDirIterator it(srcPath, {"*.cpp"}, QDir::Files, QDirIterator::Subdirectories);

        int totalCount = 0;
        QStringList details;

        while (it.hasNext()) {
            it.next();
            QFile file(it.filePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            int fileCount = 0;
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (commentPattern.match(line).hasMatch())
                    continue;
                if (macroPattern.match(line).hasMatch())
                    ++fileCount;
            }

            if (fileCount > 0) {
                totalCount += fileCount;
                details.append(
                    QStringLiteral("%1: %2")
                        .arg(QDir(sourceDir).relativeFilePath(it.filePath()))
                        .arg(fileCount));
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: old_style_connections = %1 (limit: %2)")
                   .arg(totalCount)
                   .arg(kMaxOldStyleConnections);
        for (const auto &d : std::as_const(details))
            qDebug().noquote() << "    " << d;

        QVERIFY2(totalCount <= kMaxOldStyleConnections,
                 qPrintable(QStringLiteral("Too many old-style SIGNAL/SLOT macros: %1 (limit: %2)")
                                .arg(totalCount)
                                .arg(kMaxOldStyleConnections)));
    }

    // ================================================================
    // h) MainWindow public slots メソッド数上限テスト
    //    public slots: セクション内のメソッド宣言数をカウント
    // ================================================================
    void mainWindowPublicSlotsLimit()
    {
        const QString headerPath =
            QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/app/mainwindow.h");
        constexpr int maxPublicSlots = 50;

        QFile file(headerPath);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                 "Failed to open mainwindow.h");

        QTextStream in(&file);
        bool inPublicSlots = false;
        int slotCount = 0;
        const QRegularExpression methodPattern(QStringLiteral(R"(^\s+void\s+\w+\()"));
        const QRegularExpression sectionPattern(
            QStringLiteral(R"(^(public|private|protected|signals)\s*:)"));

        while (!in.atEnd()) {
            const QString line = in.readLine();

            if (line.contains(QStringLiteral("public slots:"))) {
                inPublicSlots = true;
                continue;
            }

            if (inPublicSlots) {
                if (sectionPattern.match(line).hasMatch()) {
                    inPublicSlots = false;
                    continue;
                }
                if (methodPattern.match(line).hasMatch())
                    ++slotCount;
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: mw_public_slots = %1 (limit: %2)")
                   .arg(slotCount)
                   .arg(maxPublicSlots);

        QVERIFY2(slotCount <= maxPublicSlots,
                 qPrintable(QStringLiteral("MainWindow public slots: %1 (limit: %2)")
                                .arg(slotCount)
                                .arg(maxPublicSlots)));
    }
};

QTEST_MAIN(TestStructuralKpi)
#include "tst_structural_kpi.moc"
