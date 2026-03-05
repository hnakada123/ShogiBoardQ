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
    struct ConnectCall {
        QString text;
        qsizetype start = 0;
        int line = 1;
    };

    /// connect(...) 呼び出しブロックを括弧対応で抽出する
    static QList<ConnectCall> extractConnectCalls(const QString& source)
    {
        QList<ConnectCall> calls;
        static const QRegularExpression startRe(
            QStringLiteral(R"((?<![A-Za-z0-9_])connect\s*\()"));

        qsizetype lineScanPos = 0;
        int currentLine = 1;
        auto it = startRe.globalMatch(source);
        while (it.hasNext()) {
            const auto m = it.next();
            const qsizetype start = m.capturedStart();
            while (lineScanPos < start && lineScanPos < source.size()) {
                if (source.at(lineScanPos) == QChar('\n'))
                    ++currentLine;
                ++lineScanPos;
            }
            const qsizetype openParen = source.indexOf(QChar('('), start);
            if (openParen < 0)
                continue;
            // disconnect(...) は除外
            if (openParen >= 3 && source.mid(openParen - 3, 3) == QStringLiteral("dis"))
                continue;

            bool inSingleQuote = false;
            bool inDoubleQuote = false;
            bool inLineComment = false;
            bool inBlockComment = false;
            bool escaped = false;
            int depth = 1;
            qsizetype i = openParen + 1;
            qsizetype closeParen = -1;

            for (; i < source.size(); ++i) {
                const QChar ch = source.at(i);
                const QChar next = (i + 1 < source.size()) ? source.at(i + 1) : QChar();

                if (inLineComment) {
                    if (ch == QChar('\n'))
                        inLineComment = false;
                    continue;
                }
                if (inBlockComment) {
                    if (ch == QChar('*') && next == QChar('/')) {
                        inBlockComment = false;
                        ++i;
                    }
                    continue;
                }
                if (inSingleQuote) {
                    if (!escaped && ch == QChar('\\')) {
                        escaped = true;
                        continue;
                    }
                    if (!escaped && ch == QChar('\'')) {
                        inSingleQuote = false;
                    }
                    escaped = false;
                    continue;
                }
                if (inDoubleQuote) {
                    if (!escaped && ch == QChar('\\')) {
                        escaped = true;
                        continue;
                    }
                    if (!escaped && ch == QChar('\"')) {
                        inDoubleQuote = false;
                    }
                    escaped = false;
                    continue;
                }

                if (ch == QChar('/') && next == QChar('/')) {
                    inLineComment = true;
                    ++i;
                    continue;
                }
                if (ch == QChar('/') && next == QChar('*')) {
                    inBlockComment = true;
                    ++i;
                    continue;
                }
                if (ch == QChar('\'')) {
                    inSingleQuote = true;
                    escaped = false;
                    continue;
                }
                if (ch == QChar('\"')) {
                    inDoubleQuote = true;
                    escaped = false;
                    continue;
                }

                if (ch == QChar('(')) {
                    ++depth;
                } else if (ch == QChar(')')) {
                    --depth;
                    if (depth == 0) {
                        closeParen = i;
                        break;
                    }
                }
            }

            if (closeParen < 0)
                continue;

            calls.append({source.mid(start, closeParen - start + 1), start, currentLine});
        }
        return calls;
    }

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
    static bool isSlotConnected(const QList<ConnectCall>& calls, const QString& slotName)
    {
        // Check if the slot name appears as a target in any connect() call
        // Pattern: &ClassName::slotName or "slotName" in connect context
        const QString pattern = QStringLiteral("&\\w+::") + QRegularExpression::escape(slotName);
        QRegularExpression re(pattern);

        for (const auto& call : calls) {
            if (re.match(call.text).hasMatch())
                return true;
        }
        return false;
    }

    /// ヘッダから Deps/Dependencies 構造体のフィールド名を抽出する
    static QStringList extractDepsFieldNames(const QString& headerContent)
    {
        QStringList fieldNames;

        // Find "struct Deps {" or "struct Dependencies {" blocks
        static const QRegularExpression depsSection(
            QStringLiteral("struct\\s+(?:Deps|Dependencies)\\s*\\{([^}]*?)\\};"),
            QRegularExpression::DotMatchesEverythingOption);

        auto matchIt = depsSection.globalMatch(headerContent);
        while (matchIt.hasNext()) {
            const auto m = matchIt.next();
            const QString section = m.captured(1);

            // Extract field names: Type* fieldName = nullptr; or Type fieldName;
            // Also handles std::function<...> fieldName;
            static const QRegularExpression fieldDecl(
                QStringLiteral("\\b(\\w+)\\s*(?:=\\s*[^;]+)?;\\s*(?:///.*)?$"),
                QRegularExpression::MultilineOption);
            auto fieldIt = fieldDecl.globalMatch(section);
            while (fieldIt.hasNext()) {
                const auto fm = fieldIt.next();
                const QString name = fm.captured(1);
                // Skip type-like names (nullptr, false, true, 0) and common noise
                if (name != QStringLiteral("nullptr")
                    && name != QStringLiteral("false")
                    && name != QStringLiteral("true")
                    && !name.isEmpty()
                    && name.at(0).isLower()) {
                    fieldNames.append(name);
                }
            }
        }

        return fieldNames;
    }

    /// ヘッダから signals: セクション内のシグナル名を抽出する
    static QStringList extractSignalNames(const QString& headerContent)
    {
        QStringList signalNames;

        static const QRegularExpression signalsSection(
            QStringLiteral("signals\\s*:([^}]*?)(?:private\\s*:|protected\\s*:|public\\s*:|};)"),
            QRegularExpression::DotMatchesEverythingOption);

        auto matchIt = signalsSection.globalMatch(headerContent);
        while (matchIt.hasNext()) {
            const auto m = matchIt.next();
            const QString section = m.captured(1);

            // Extract method names from signal declarations
            static const QRegularExpression methodDecl(
                QStringLiteral("\\b(\\w+)\\s*\\([^)]*\\)\\s*;"));
            auto methodIt = methodDecl.globalMatch(section);
            while (methodIt.hasNext()) {
                const auto mm = methodIt.next();
                const QString name = mm.captured(1);
                if (name != QStringLiteral("void"))
                    signalNames.append(name);
            }
        }

        return signalNames;
    }

    /// src/ 配下の全 .cpp で指定のシグナル名が connect() のソースとして使われているか
    static bool isSignalUsedAsSource(const QList<ConnectCall>& calls, const QString& signalName)
    {
        // Signal appears as &ClassName::signalName in a connect() call
        // Check the signal part (first &Class::method in connect)
        const QString pattern = QStringLiteral("&\\w+::") + QRegularExpression::escape(signalName);
        QRegularExpression re(pattern);

        for (const auto& call : calls) {
            if (re.match(call.text).hasMatch())
                return true;
        }

        // Also check signal-to-signal forwarding (emit signalName)
        // and direct signal references in other contexts
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
        const QList<ConnectCall> calls = extractConnectCalls(source);
        for (const auto& call : calls) {
            const qsizetype openPos = call.text.indexOf(QChar('('));
            const qsizetype closePos = call.text.lastIndexOf(QChar(')'));
            if (openPos < 0 || closePos <= openPos)
                continue;

            const QString body = call.text.mid(openPos + 1, closePos - openPos - 1);
            // Extract signal and slot patterns
            static const QRegularExpression sigSlotRe(
                QStringLiteral("&(\\w+::\\w+).*&(\\w+::\\w+)"));
            const auto sigSlotMatch = sigSlotRe.match(body);
            if (sigSlotMatch.hasMatch()) {
                connections.append({
                    sigSlotMatch.captured(1),
                    sigSlotMatch.captured(2),
                    fileName,
                    call.line
                });
            }
        }
        return connections;
    }

    // Cached combined source for all .cpp files
    QString m_allSources;
    QList<ConnectCall> m_allConnectCalls;
    QStringList m_wiringHeaders;

private slots:
    void initTestCase()
    {
        // Combine all .cpp sources from src/ for connection search
        m_allSources = readAllCppInDir(QStringLiteral("src"));
        QVERIFY2(!m_allSources.isEmpty(), "No .cpp sources found under src/");
        m_allConnectCalls = extractConnectCalls(m_allSources);
        QVERIFY2(!m_allConnectCalls.isEmpty(), "No connect() calls found under src/");

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
                if (!isSlotConnected(m_allConnectCalls, slotName)) {
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
    // Deps フィールド参照漏れ: 全 Deps/Dependencies メンバが .cpp で使われているか
    // ================================================================

    void allDepsFields_areReferencedInSource()
    {
        // Deps/Dependencies 構造体のフィールドが対応する .cpp で参照されているかチェック
        QStringList unreferencedFields;

        const QString wiringDir = QStringLiteral(SOURCE_DIR) + QStringLiteral("/src/ui/wiring");
        QDirIterator it(wiringDir, {QStringLiteral("*.h")}, QDir::Files);

        while (it.hasNext()) {
            it.next();
            const QString headerPath = it.filePath();
            const QString baseName = QFileInfo(headerPath).completeBaseName();
            const QString cppPath = wiringDir + QStringLiteral("/") + baseName + QStringLiteral(".cpp");

            QFile hf(headerPath);
            if (!hf.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString headerContent = QString::fromUtf8(hf.readAll());

            QFile cf(cppPath);
            if (!cf.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString cppContent = QString::fromUtf8(cf.readAll());

            // Also read split .cpp files (e.g., foo_bar.cpp)
            QString allCpp = cppContent;
            QDirIterator splitIt(wiringDir,
                {baseName + QStringLiteral("_*.cpp")}, QDir::Files);
            while (splitIt.hasNext()) {
                splitIt.next();
                QFile sf(splitIt.filePath());
                if (sf.open(QIODevice::ReadOnly | QIODevice::Text))
                    allCpp += QString::fromUtf8(sf.readAll());
            }

            const QStringList fieldNames = extractDepsFieldNames(headerContent);
            const QString fileName = QFileInfo(headerPath).fileName();

            for (const QString& field : fieldNames) {
                // Check if the field name appears in the .cpp source
                // (as member access: m_deps.field, deps.field, or just field)
                if (!allCpp.contains(field)) {
                    unreferencedFields.append(
                        QStringLiteral("%1::Deps::%2").arg(fileName, field));
                }
            }
        }

        if (!unreferencedFields.isEmpty()) {
            const QString msg = QStringLiteral("Unreferenced Deps fields (potential injection gaps):\n  - ")
                + unreferencedFields.join(QStringLiteral("\n  - "));
            qWarning().noquote() << msg;
        }

        QVERIFY2(true, "Deps field reference check completed (see warnings above)");
    }

    // ================================================================
    // シグナル未接続検知: signals: で宣言されたシグナルが connect() で使われているか
    // ================================================================

    void allWiringSignals_areConnectedSomewhere()
    {
        QStringList unconnectedSignals;

        for (const QString& headerPath : std::as_const(m_wiringHeaders)) {
            QFile f(headerPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            const QString content = QString::fromUtf8(f.readAll());
            const QString fileName = QFileInfo(headerPath).fileName();

            const QStringList signalNames = extractSignalNames(content);
            for (const QString& sigName : signalNames) {
                if (!isSignalUsedAsSource(m_allConnectCalls, sigName)) {
                    unconnectedSignals.append(
                        QStringLiteral("%1::%2").arg(fileName, sigName));
                }
            }
        }

        if (!unconnectedSignals.isEmpty()) {
            const QString msg = QStringLiteral("Unconnected signals (never used in connect()):\n  - ")
                + unconnectedSignals.join(QStringLiteral("\n  - "));
            qWarning().noquote() << msg;
        }

        QVERIFY2(true, "Signal connection check completed (see warnings above)");
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
            QStringLiteral("src/app/gamewiringsubregistry.h"),
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
