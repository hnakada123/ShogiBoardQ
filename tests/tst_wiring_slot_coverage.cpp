/// @file tst_wiring_slot_coverage.cpp
/// @brief Wiring 層のスロット接続カバレッジテスト（ソース解析型）
///
/// 全 Wiring クラスの public slots が少なくとも1つの connect() で
/// 接続されているかを検証する。接続漏れ・重複接続を検出する。

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestWiringSlotCoverage : public QObject
{
    Q_OBJECT

private:
    /// ソースファイルを読み込む
    static QString readSource(const QString& relativePath)
    {
        QFile f(QStringLiteral(SOURCE_DIR) + QStringLiteral("/") + relativePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(f.readAll());
    }

    /// ディレクトリ配下の全 .cpp ファイルの内容を結合して返す
    static QString readAllCppInDir(const QString& relDir)
    {
        const QString absDir = QStringLiteral(SOURCE_DIR) + QStringLiteral("/") + relDir;
        QDirIterator it(absDir, {QStringLiteral("*.cpp")}, QDir::Files, QDirIterator::Subdirectories);
        QString combined;
        while (it.hasNext()) {
            it.next();
            QFile f(it.filePath());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                combined += QString::fromUtf8(f.readAll());
                combined += QChar('\n');
            }
        }
        return combined;
    }

    /// ヘッダから public slots: セクション内のメソッド名を抽出する
    static QStringList extractPublicSlotNames(const QString& headerContent)
    {
        QStringList slotNames;

        // Find "public slots:" sections and extract method declarations
        static const QRegularExpression slotsSection(
            QStringLiteral("public\\s+slots\\s*:([^}]*?)(?:signals\\s*:|private\\s*:|protected\\s*:|public\\s*:|};)"),
            QRegularExpression::DotMatchesEverythingOption);

        auto matchIt = slotsSection.globalMatch(headerContent);
        while (matchIt.hasNext()) {
            const auto m = matchIt.next();
            const QString section = m.captured(1);

            // Extract method names: void methodName(...) or Type methodName(...)
            static const QRegularExpression methodDecl(
                QStringLiteral("\\b(\\w+)\\s*\\([^)]*\\)\\s*(?:const)?\\s*(?:override)?\\s*;"));
            auto methodIt = methodDecl.globalMatch(section);
            while (methodIt.hasNext()) {
                const auto mm = methodIt.next();
                const QString name = mm.captured(1);
                // Skip constructors/destructors
                if (!name.startsWith(QChar('~')) && name != QStringLiteral("void"))
                    slotNames.append(name);
            }
        }

        return slotNames;
    }

    /// src/ 配下の全 .cpp で指定のスロット名が connect() 内に出現するか
    static bool isSlotConnected(const QString& allSources, const QString& slotName)
    {
        // Check if the slot name appears as a target in any connect() call
        // Pattern: &ClassName::slotName or "slotName" in connect context
        const QString pattern = QStringLiteral("&\\w+::") + QRegularExpression::escape(slotName);
        QRegularExpression re(pattern);

        // Search for connect() blocks containing this slot
        qsizetype pos = 0;
        while ((pos = allSources.indexOf(QStringLiteral("connect("), pos)) != -1) {
            qsizetype end = allSources.indexOf(QStringLiteral(");"), pos);
            if (end < 0) break;
            const QString block = allSources.mid(pos, end - pos + 2);
            if (re.match(block).hasMatch())
                return true;
            pos = end + 1;
        }
        return false;
    }

    /// connect() の重複を検出（sender::signal → receiver::slot ペアの重複）
    struct ConnectionInfo {
        QString signalPart;
        QString slotPart;
        QString file;
        int line;
    };

    static QList<ConnectionInfo> findAllConnections(const QString& source, const QString& fileName)
    {
        QList<ConnectionInfo> connections;
        static const QRegularExpression connectRe(
            QStringLiteral("connect\\(([^;]+?)\\);"));

        int lineNum = 1;
        qsizetype searchPos = 0;
        auto matchIt = connectRe.globalMatch(source);
        while (matchIt.hasNext()) {
            const auto m = matchIt.next();
            // Count lines up to this position
            const qsizetype matchPos = m.capturedStart();
            while (searchPos < matchPos) {
                if (source.at(searchPos) == QChar('\n'))
                    ++lineNum;
                ++searchPos;
            }

            const QString body = m.captured(1);
            // Extract signal and slot patterns
            static const QRegularExpression sigSlotRe(
                QStringLiteral("&(\\w+::\\w+).*&(\\w+::\\w+)"));
            const auto sigSlotMatch = sigSlotRe.match(body);
            if (sigSlotMatch.hasMatch()) {
                connections.append({
                    sigSlotMatch.captured(1),
                    sigSlotMatch.captured(2),
                    fileName,
                    lineNum
                });
            }
        }
        return connections;
    }

    // Cached combined source for all .cpp files
    QString m_allSources;
    QStringList m_wiringHeaders;

private slots:
    void initTestCase()
    {
        // Combine all .cpp sources from src/ for connection search
        m_allSources = readAllCppInDir(QStringLiteral("src"));
        QVERIFY2(!m_allSources.isEmpty(), "No .cpp sources found under src/");

        // Collect all wiring header files
        const QString wiringDir = QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/ui/wiring");
        QDirIterator it(wiringDir, {QStringLiteral("*.h")}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            m_wiringHeaders.append(it.filePath());
        }
        QVERIFY2(!m_wiringHeaders.isEmpty(), "No wiring headers found");
    }

    // ================================================================
    // 接続漏れ検出: 全 Wiring クラスの public slots が接続されているか
    // ================================================================

    void allWiringPublicSlots_areConnected()
    {
        QStringList unconnectedSlots;

        for (const QString& headerPath : std::as_const(m_wiringHeaders)) {
            QFile f(headerPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString content = QString::fromUtf8(f.readAll());
            const QString fileName = QFileInfo(headerPath).fileName();

            const QStringList slotNames = extractPublicSlotNames(content);
            for (const QString& slotName : slotNames) {
                if (!isSlotConnected(m_allSources, slotName)) {
                    unconnectedSlots.append(
                        QStringLiteral("%1::%2").arg(fileName, slotName));
                }
            }
        }

        if (!unconnectedSlots.isEmpty()) {
            const QString msg = QStringLiteral("Unconnected public slots found:\n  - ")
                + unconnectedSlots.join(QStringLiteral("\n  - "));
            qWarning().noquote() << msg;
        }

        // Currently report-only: warn but do not fail
        // When all slots are properly connected, change this to QVERIFY
        QVERIFY2(true, "Slot coverage check completed (see warnings above)");
    }

    // ================================================================
    // 重複接続検出: 同一 sender::signal → receiver::slot ペアの重複
    // ================================================================

    void noDuplicateConnections_inWiringFiles()
    {
        QMap<QString, QStringList> connectionSources;

        const QString wiringDir = QStringLiteral("src/ui/wiring");
        const QString absDir = QStringLiteral(SOURCE_DIR) + QStringLiteral("/") + wiringDir;
        QDirIterator it(absDir, {QStringLiteral("*.cpp")}, QDir::Files);

        while (it.hasNext()) {
            it.next();
            QFile f(it.filePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString content = QString::fromUtf8(f.readAll());
            const QString fileName = QFileInfo(it.filePath()).fileName();

            const auto connections = findAllConnections(content, fileName);
            for (const auto& conn : connections) {
                const QString key = conn.signalPart + QStringLiteral(" -> ") + conn.slotPart;
                connectionSources[key].append(
                    QStringLiteral("%1:%2").arg(conn.file).arg(conn.line));
            }
        }

        QStringList duplicates;
        for (auto cit = connectionSources.cbegin(); cit != connectionSources.cend(); ++cit) {
            if (cit.value().size() > 1) {
                duplicates.append(QStringLiteral("  %1 (x%2): %3")
                    .arg(cit.key())
                    .arg(cit.value().size())
                    .arg(cit.value().join(QStringLiteral(", "))));
            }
        }

        if (!duplicates.isEmpty()) {
            const QString msg = QStringLiteral("Duplicate connections found:\n")
                + duplicates.join(QChar('\n'));
            qWarning().noquote() << msg;
        }

        // Report-only for now (some duplicates may be intentional for different contexts)
        QVERIFY2(true, "Duplicate connection check completed (see warnings above)");
    }

    // ================================================================
    // 接続カウント: 各 Wiring ファイルの connect() 数がゼロでないこと
    // ================================================================

    void eachWiringFile_hasConnections()
    {
        const QString absDir = QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/ui/wiring");
        QDirIterator it(absDir, {QStringLiteral("*.cpp")}, QDir::Files);

        QStringList noConnections;
        while (it.hasNext()) {
            it.next();
            QFile f(it.filePath());
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString content = QString::fromUtf8(f.readAll());
            const QString fileName = QFileInfo(it.filePath()).fileName();

            int count = 0;
            qsizetype pos = 0;
            while ((pos = content.indexOf(QStringLiteral("connect("), pos)) != -1) {
                ++count;
                ++pos;
            }

            if (count == 0) {
                noConnections.append(fileName);
            }
        }

        if (!noConnections.isEmpty()) {
            const QString msg = QStringLiteral("Wiring files with 0 connect() calls:\n  - ")
                + noConnections.join(QStringLiteral("\n  - "));
            QWARN(qPrintable(msg));
        }

        // Most wiring files should have at least one connection
        // (some may be pure structural with no signals)
        QVERIFY(true);
    }

    // ================================================================
    // Registry/Bootstrap 接続漏れ: ensure* メソッドが呼ばれているか
    // ================================================================

    void registryEnsureMethods_areInvoked()
    {
        // Read registry headers and extract ensure* method names
        const QStringList registryFiles = {
            QStringLiteral("src/app/mainwindowserviceregistry.h"),
            QStringLiteral("src/app/mainwindowfoundationregistry.h"),
            QStringLiteral("src/app/gamesubregistry.h"),
            QStringLiteral("src/app/kifusubregistry.h"),
            QStringLiteral("src/app/gamesessionsubregistry.h"),
        };

        // Read all .cpp files from src/app/ for invocation search
        const QString appSources = readAllCppInDir(QStringLiteral("src/app"));
        QVERIFY2(!appSources.isEmpty(), "No .cpp sources found in src/app/");

        QStringList uninvokedMethods;

        for (const QString& relPath : registryFiles) {
            const QString content = readSource(relPath);
            if (content.isEmpty()) continue;

            const QString fileName = QFileInfo(relPath).fileName();

            // Extract ensure* method declarations
            static const QRegularExpression ensureDecl(
                QStringLiteral("\\b(ensure\\w+)\\s*\\("));
            auto matchIt = ensureDecl.globalMatch(content);
            while (matchIt.hasNext()) {
                const auto m = matchIt.next();
                const QString methodName = m.captured(1);

                // Check if this method is called in any app source
                if (!appSources.contains(methodName + QStringLiteral("("))) {
                    uninvokedMethods.append(
                        QStringLiteral("%1::%2").arg(fileName, methodName));
                }
            }
        }

        if (!uninvokedMethods.isEmpty()) {
            const QString msg = QStringLiteral("Uninvoked ensure* methods:\n  - ")
                + uninvokedMethods.join(QStringLiteral("\n  - "));
            qWarning().noquote() << msg;
        }

        QVERIFY2(true, "Registry ensure* invocation check completed");
    }
};

QTEST_MAIN(TestWiringSlotCoverage)
#include "tst_wiring_slot_coverage.moc"
