#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTextStream>

// CMAKE_SOURCE_DIR is defined via target_compile_definitions
#ifndef SOURCE_DIR
#error "SOURCE_DIR must be defined by CMake"
#endif

class TestLayerDependencies : public QObject
{
    Q_OBJECT

private:
    struct LayerRule {
        QString sourceDir;
        QStringList forbiddenDirs;
    };

    static QList<LayerRule> layerRules()
    {
        return {
            {QStringLiteral("src/core"),
             {QStringLiteral("app/"), QStringLiteral("ui/"), QStringLiteral("dialogs/"),
              QStringLiteral("widgets/"), QStringLiteral("views/")}},
            {QStringLiteral("src/kifu/formats"),
             {QStringLiteral("app/"), QStringLiteral("dialogs/"), QStringLiteral("widgets/"),
              QStringLiteral("views/")}},
            {QStringLiteral("src/engine"),
             {QStringLiteral("app/"), QStringLiteral("dialogs/"), QStringLiteral("widgets/"),
              QStringLiteral("views/")}},
            {QStringLiteral("src/game"),
             {QStringLiteral("dialogs/"), QStringLiteral("widgets/"), QStringLiteral("views/")}},
            {QStringLiteral("src/services"),
             {QStringLiteral("app/"), QStringLiteral("dialogs/"), QStringLiteral("widgets/"),
              QStringLiteral("views/")}},
        };
    }

    /// 指定ディレクトリ直下の .h ファイル名セットを収集する（フラットinclude検出用）
    static QSet<QString> collectHeaderNames(const QString &rootDir, const QString &forbiddenDir)
    {
        QSet<QString> names;
        // forbiddenDir は "dialogs/" のような形式。src/ 配下にマップする
        const QString dirPath = rootDir + QStringLiteral("/src/") + forbiddenDir;
        const QDir dir(dirPath);
        if (!dir.exists())
            return names;
        const QStringList headers = dir.entryList({QStringLiteral("*.h")}, QDir::Files);
        for (const auto &h : std::as_const(headers))
            names.insert(h);
        return names;
    }

    static QStringList findViolations(const QString &rootDir, const LayerRule &rule)
    {
        QStringList violations;
        const QString scanDir = rootDir + QStringLiteral("/") + rule.sourceDir;

        // #include "xxx.h" 形式のみ対象（<xxx> はスキップ）
        const QRegularExpression includePattern(
            QStringLiteral("^\\s*#\\s*include\\s+\"([^\"]+)\""));
        // コメント行を除外
        const QRegularExpression commentPattern(QStringLiteral("^\\s*//"));

        // フラットinclude検出用: 各禁止ディレクトリのヘッダファイル名セットを収集
        QHash<QString, QSet<QString>> forbiddenFileNames;
        for (const auto &forbidden : std::as_const(rule.forbiddenDirs))
            forbiddenFileNames[forbidden] = collectHeaderNames(rootDir, forbidden);

        QDirIterator it(scanDir, {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
                        QDirIterator::Subdirectories);

        while (it.hasNext()) {
            it.next();
            const QString absPath = it.filePath();
            const QString relPath = QDir(rootDir).relativeFilePath(absPath);

            QFile file(absPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            int lineNum = 0;

            while (!in.atEnd()) {
                const QString line = in.readLine();
                ++lineNum;

                // コメント行はスキップ
                if (commentPattern.match(line).hasMatch())
                    continue;

                const auto match = includePattern.match(line);
                if (!match.hasMatch())
                    continue;

                const QString includePath = match.captured(1);
                // フラットinclude（パス区切りなし）の場合はファイル名そのものを使う
                const QString includeBasename = QFileInfo(includePath).fileName();

                for (const auto &forbidden : std::as_const(rule.forbiddenDirs)) {
                    // パスベースの検出: "../dialogs/foo.h" のような形式
                    if (includePath.contains(forbidden)) {
                        violations.append(
                            QStringLiteral("  %1:%2 includes \"%3\" (forbidden: %4)")
                                .arg(relPath)
                                .arg(lineNum)
                                .arg(includePath)
                                .arg(forbidden));
                        continue;
                    }
                    // ファイル名ベースの検出: "foo.h" が禁止ディレクトリに存在する場合
                    if (forbiddenFileNames.value(forbidden).contains(includeBasename)) {
                        violations.append(
                            QStringLiteral("  %1:%2 includes \"%3\" (forbidden flat include from: %4)")
                                .arg(relPath)
                                .arg(lineNum)
                                .arg(includePath)
                                .arg(forbidden));
                    }
                }
            }
        }

        return violations;
    }

private slots:
    void verifyLayerDependencies()
    {
        const QString rootDir = QStringLiteral(SOURCE_DIR);
        const QList<LayerRule> rules = layerRules();

        QStringList allViolations;

        for (const auto &rule : std::as_const(rules)) {
            const QStringList violations = findViolations(rootDir, rule);
            if (!violations.isEmpty()) {
                allViolations.append(
                    QStringLiteral("[%1] -> forbidden includes:").arg(rule.sourceDir));
                allViolations.append(violations);
            }
        }

        if (!allViolations.isEmpty()) {
            QString msg = QStringLiteral("Layer dependency violations found:\n");
            for (const auto &v : std::as_const(allViolations))
                msg += v + QStringLiteral("\n");
            QFAIL(qPrintable(msg));
        }
    }
};

QTEST_MAIN(TestLayerDependencies)
#include "tst_layer_dependencies.moc"
