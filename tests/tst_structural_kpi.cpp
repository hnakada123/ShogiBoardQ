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
    static QMap<QString, int> knownLargeFiles()
    {
        return {
            {"src/kifu/gamerecordmodel.cpp", 1729},
            {"src/network/csagamecoordinator.cpp", 1569},
            {"src/kifu/formats/ki2tosfenconverter.cpp", 1549},
            {"src/game/matchcoordinator.cpp", 1444},
            {"src/ui/coordinators/kifudisplaycoordinator.cpp", 1412},
            {"src/widgets/engineanalysistab.cpp", 1378},
            {"src/kifu/formats/kiftosfenconverter.cpp", 582},
            {"src/kifu/kifuloadcoordinator.cpp", 1263},
            {"src/game/gamestartcoordinator.cpp", 1181},
            {"src/analysis/analysisflowcontroller.cpp", 1133},
            {"src/engine/usi.cpp", 1084},
            {"src/widgets/evaluationchartwidget.cpp", 1018},
            {"src/kifu/kifuexportcontroller.cpp", 933},
            {"src/core/fmvlegalcore.cpp", 930},
            {"src/kifu/formats/usentosfenconverter.cpp", 910},
            {"src/engine/usiprotocolhandler.cpp", 902},
            {"src/dialogs/pvboarddialog.cpp", 884},
            {"src/dialogs/engineregistrationdialog.cpp", 844},
            {"src/dialogs/josekimovedialog.cpp", 840},
            {"src/core/shogiboard.cpp", 820},
            {"src/views/shogiview_draw.cpp", 808},
            {"src/views/shogiview.cpp", 802},
            {"src/dialogs/changeenginesettingsdialog.cpp", 802},
            {"src/kifu/formats/csaexporter.cpp", 758},
            {"src/kifu/formats/jkfexporter.cpp", 735},
            {"src/views/shogiview_labels.cpp", 704},
            {"src/network/csaclient.cpp", 693},
            {"src/engine/shogiengineinfoparser.cpp", 689},
            {"src/widgets/recordpane.cpp", 677},
            {"src/kifu/formats/usitosfenconverter.cpp", 675},
            {"src/dialogs/tsumeshogigeneratordialog.cpp", 669},
            {"src/game/shogigamecontroller.cpp", 664},
            {"src/navigation/kifunavigationcontroller.cpp", 642},
            {"src/dialogs/startgamedialog.cpp", 640},
            {"src/widgets/considerationtabmanager.cpp", 634},
            {"src/widgets/branchtreemanager.cpp", 632},
            {"src/game/matchcoordinator.h", 630},
            {"src/app/mainwindowgameregistry.cpp", 592},
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
        constexpr int maxFriendClasses = 2;

        const QRegularExpression pattern(QStringLiteral(R"(^\s*friend\s+class\s+)"));
        const int count = countMatchingLines(headerPath, pattern);

        QVERIFY2(count >= 0, "Failed to read mainwindow.h");
        QVERIFY2(count <= maxFriendClasses,
                 qPrintable(QStringLiteral("MainWindow friend class count: %1 (limit: %2)")
                                .arg(count)
                                .arg(maxFriendClasses)));

        if (count < maxFriendClasses) {
            qDebug().noquote()
                << QStringLiteral("MainWindow friend classes reduced to %1 (limit: %2) "
                                  "-> consider lowering the limit")
                       .arg(count)
                       .arg(maxFriendClasses);
        }
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
        QVERIFY2(count <= maxEnsureMethods,
                 qPrintable(QStringLiteral("MainWindow ensure* count: %1 (limit: %2)")
                                .arg(count)
                                .arg(maxEnsureMethods)));

        if (count < maxEnsureMethods) {
            qDebug().noquote()
                << QStringLiteral("MainWindow ensure* methods reduced to %1 (limit: %2) "
                                  "-> consider lowering the limit")
                       .arg(count)
                       .arg(maxEnsureMethods);
        }
    }

    // ================================================================
    // d) MainWindow public slots メソッド数上限テスト
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

        QVERIFY2(slotCount <= maxPublicSlots,
                 qPrintable(QStringLiteral("MainWindow public slots: %1 (limit: %2)")
                                .arg(slotCount)
                                .arg(maxPublicSlots)));

        qDebug().noquote()
            << QStringLiteral("MainWindow public slots: %1 (limit: %2)")
                   .arg(slotCount)
                   .arg(maxPublicSlots);
    }
};

QTEST_MAIN(TestStructuralKpi)
#include "tst_structural_kpi.moc"
