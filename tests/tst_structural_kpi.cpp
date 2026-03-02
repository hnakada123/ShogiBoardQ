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

    struct FileInfo {
        QString relPath;
        int lines;
    };

    static QList<FileInfo> collectSourceFiles()
    {
        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        QDirIterator it(srcPath, {"*.cpp", "*.h"}, QDir::Files, QDirIterator::Subdirectories);

        QList<FileInfo> result;
        while (it.hasNext()) {
            it.next();
            const int lines = countLines(it.filePath());
            if (lines >= 0) {
                result.append({QDir(sourceDir).relativeFilePath(it.filePath()), lines});
            }
        }
        return result;
    }

    static int countFilesOverLines(const QList<FileInfo> &files, int threshold)
    {
        int count = 0;
        for (const auto &f : files) {
            if (f.lines > threshold)
                ++count;
        }
        return count;
    }

    // 既知の例外リスト: ファイルパス(src/からの相対) → 現在の行数上限
    // リファクタリングでファイルが縮小されたら、この値も下げること。
    // 例外リスト外のファイルが600行を超えたらテスト失敗となる。
    // 新しい例外を追加する場合は、削減計画の Issue 番号を同時に記載すること。
    // 2026-03-01 全ファイル600行以下達成。例外リストは空。
    static QMap<QString, int> knownLargeFiles()
    {
        return {};
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
        constexpr int maxFriendClasses = 7;

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
        constexpr int maxEnsureMethods = 14;

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
        const int kMaxLargeFiles = 0;        // 全ファイル600行以下達成
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
        const int kMaxDeleteCount = 1; // 実測値: flowlayout.cpp の1件のみ

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
    //    複数行にまたがる connect(..., [...]) も検出する
    // ================================================================
    void lambdaConnectCount()
    {
        const int kMaxLambdaConnects = 0; // 実測値: 0件

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        // connect( にマッチ（disconnect( は除外）
        const QRegularExpression connectPattern(QStringLiteral(R"((?<!dis)connect\s*\()"));
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
            int parenDepth = 0;
            QString buffer;
            bool inConnect = false;

            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (commentPattern.match(line).hasMatch())
                    continue;

                if (!inConnect && connectPattern.match(line).hasMatch()) {
                    inConnect = true;
                    buffer.clear();
                    parenDepth = 0;
                }

                if (inConnect) {
                    buffer += line + QLatin1Char('\n');
                    for (QChar ch : line) {
                        if (ch == QLatin1Char('(')) ++parenDepth;
                        if (ch == QLatin1Char(')')) --parenDepth;
                    }
                    if (parenDepth <= 0) {
                        if (lambdaPattern.match(buffer).hasMatch()) {
                            ++fileCount;
                        }
                        inConnect = false;
                        parenDepth = 0;
                    }
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
        constexpr int maxPublicSlots = 13;

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

    // ================================================================
    // i) サブレジストリ ensure* 上限テスト
    //    各サブレジストリの ensure* メソッド数が上限以下であることを検証
    // ================================================================
    void subRegistryEnsureLimits()
    {
        struct RegistryLimit {
            const char* headerFile;
            const char* name;
            int maxEnsure;
        };

        const RegistryLimit registries[] = {
            {"src/app/mainwindowfoundationregistry.h", "FoundationRegistry", 15},
            {"src/app/gamesubregistry.h",              "GameSubRegistry",     6},
            {"src/app/gamesessionsubregistry.h",       "GameSessionSubReg",   6},
            {"src/app/gamewiringsubregistry.h",        "GameWiringSubReg",    4},
            {"src/app/kifusubregistry.h",              "KifuSubRegistry",     8},
        };

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QRegularExpression pattern(QStringLiteral(R"(^\s*void\s+ensure)"));

        QStringList violations;

        for (const auto& reg : registries) {
            const QString headerPath = sourceDir + QStringLiteral("/") + QString::fromLatin1(reg.headerFile);
            const int count = countMatchingLines(headerPath, pattern);

            QVERIFY2(count >= 0,
                     qPrintable(QStringLiteral("Failed to read %1").arg(QString::fromLatin1(reg.headerFile))));

            qDebug().noquote()
                << QStringLiteral("KPI: %1_ensure = %2 (limit: %3)")
                       .arg(QString::fromLatin1(reg.name))
                       .arg(count)
                       .arg(reg.maxEnsure);

            if (count > reg.maxEnsure) {
                violations.append(
                    QStringLiteral("%1 ensure* count: %2 (limit: %3)")
                        .arg(QString::fromLatin1(reg.name))
                        .arg(count)
                        .arg(reg.maxEnsure));
            }
        }

        if (!violations.isEmpty()) {
            QString msg = QStringLiteral("Sub-registry ensure* limit violations:\n");
            for (const auto &v : std::as_const(violations))
                msg += QStringLiteral("  ") + v + QStringLiteral("\n");
            QFAIL(qPrintable(msg));
        }
    }

    // ================================================================
    // j) 550行超ファイル数の上限チェック
    //    src/ 配下の .cpp/.h で550行を超えるファイルの総数を監視
    // ================================================================
    void filesOver550()
    {
        constexpr int kMaxFilesOver550 = 10; // 実測値: 10 (2026-03-02, ISSUE-013: 3ファイル分割)
        constexpr int kThreshold = 550;

        const auto files = collectSourceFiles();
        const int count = countFilesOverLines(files, kThreshold);

        qDebug().noquote()
            << QStringLiteral("KPI: files_over_550 = %1 (limit: %2)")
                   .arg(count)
                   .arg(kMaxFilesOver550);

        if (count > 0) {
            qDebug().noquote() << "  Files over 550 lines:";
            for (const auto &f : files) {
                if (f.lines > kThreshold)
                    qDebug().noquote() << QStringLiteral("    %1: %2 lines").arg(f.relPath).arg(f.lines);
            }
        }

        QVERIFY2(count <= kMaxFilesOver550,
                 qPrintable(QStringLiteral("Too many files over %1 lines: %2 (limit: %3)")
                                .arg(kThreshold)
                                .arg(count)
                                .arg(kMaxFilesOver550)));
    }

    // ================================================================
    // k) 500行超ファイル数の上限チェック
    //    src/ 配下の .cpp/.h で500行を超えるファイルの総数を監視
    // ================================================================
    void filesOver500()
    {
        constexpr int kMaxFilesOver500 = 31; // 実測値: 31 (2026-03-01)
        constexpr int kThreshold = 500;

        const auto files = collectSourceFiles();
        const int count = countFilesOverLines(files, kThreshold);

        qDebug().noquote()
            << QStringLiteral("KPI: files_over_500 = %1 (limit: %2)")
                   .arg(count)
                   .arg(kMaxFilesOver500);

        QVERIFY2(count <= kMaxFilesOver500,
                 qPrintable(QStringLiteral("Too many files over %1 lines: %2 (limit: %3)")
                                .arg(kThreshold)
                                .arg(count)
                                .arg(kMaxFilesOver500)));
    }

    // ================================================================
    // l) mainwindow.cpp の include 数上限チェック
    //    依存削減の追跡用
    // ================================================================
    void mainWindowIncludeCount()
    {
        constexpr int kMaxIncludes = 36; // 実測値: 36 (2026-03-02)

        const QString filePath =
            QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/app/mainwindow.cpp");

        // #include 行をカウント（コメント行を除く）
        const QRegularExpression includePattern(QStringLiteral(R"(^\s*#\s*include\s)"));
        const int count = countMatchingLines(filePath, includePattern);

        QVERIFY2(count >= 0, "Failed to read mainwindow.cpp");

        qDebug().noquote()
            << QStringLiteral("KPI: mainwindow_include_count = %1 (limit: %2)")
                   .arg(count)
                   .arg(kMaxIncludes);

        QVERIFY2(count <= kMaxIncludes,
                 qPrintable(QStringLiteral("mainwindow.cpp include count: %1 (limit: %2)")
                                .arg(count)
                                .arg(kMaxIncludes)));
    }

    // ================================================================
    // m) 長関数の監視（ゲートではなくログ出力のみ）
    //    メソッド定義開始行から次のメソッド定義開始行までの行数を簡易推定
    // ================================================================
    void longFunctionMonitor()
    {
        constexpr int kFunctionLengthWarning = 100;

        const QString sourceDir = QStringLiteral(SOURCE_DIR);
        const QString srcPath = sourceDir + QStringLiteral("/src");

        // ClassName::methodName( パターンでメソッド定義開始を検出
        const QRegularExpression methodDefPattern(
            QStringLiteral(R"(^\w[\w:]*::\w+\s*\()"));
        const QRegularExpression commentPattern(QStringLiteral(R"(^\s*//)"));

        QDirIterator it(srcPath, {"*.cpp"}, QDir::Files, QDirIterator::Subdirectories);

        int longFunctionCount = 0;
        QStringList longFunctions;

        while (it.hasNext()) {
            it.next();
            QFile file(it.filePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            const QString relPath = QDir(sourceDir).relativeFilePath(it.filePath());

            QString currentMethod;
            int methodStartLine = 0;
            int lineNum = 0;

            while (!in.atEnd()) {
                const QString line = in.readLine();
                ++lineNum;

                if (commentPattern.match(line).hasMatch())
                    continue;

                if (methodDefPattern.match(line).hasMatch()) {
                    // 前のメソッドの行数を確定
                    if (!currentMethod.isEmpty()) {
                        const int length = lineNum - methodStartLine;
                        if (length > kFunctionLengthWarning) {
                            ++longFunctionCount;
                            longFunctions.append(
                                QStringLiteral("  %1:%2 %3 (%4 lines)")
                                    .arg(relPath)
                                    .arg(methodStartLine)
                                    .arg(currentMethod)
                                    .arg(length));
                        }
                    }
                    // 新しいメソッドの開始を記録
                    const auto parenPos = line.indexOf(QLatin1Char('('));
                    currentMethod = (parenPos > 0) ? line.left(parenPos).trimmed() : line.trimmed();
                    methodStartLine = lineNum;
                }
            }

            // 最後のメソッド
            if (!currentMethod.isEmpty()) {
                const int length = lineNum - methodStartLine + 1;
                if (length > kFunctionLengthWarning) {
                    ++longFunctionCount;
                    longFunctions.append(
                        QStringLiteral("  %1:%2 %3 (%4 lines)")
                            .arg(relPath)
                            .arg(methodStartLine)
                            .arg(currentMethod)
                            .arg(length));
                }
            }
        }

        qDebug().noquote()
            << QStringLiteral("KPI: long_functions_over_%1 = %2 (monitoring only)")
                   .arg(kFunctionLengthWarning)
                   .arg(longFunctionCount);

        if (!longFunctions.isEmpty()) {
            qDebug().noquote() << "  Long functions (>" << kFunctionLengthWarning << "lines):";
            for (const auto &f : std::as_const(longFunctions))
                qDebug().noquote() << f;
        }

        // 監視のみ - QVERIFY は使わない
    }
};

QTEST_MAIN(TestStructuralKpi)
#include "tst_structural_kpi.moc"
