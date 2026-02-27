#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
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

    static QStringList findViolations(const QString &rootDir, const LayerRule &rule)
    {
        QStringList violations;
        const QString scanDir = rootDir + QStringLiteral("/") + rule.sourceDir;

        // #include "xxx.h" 形式のみ対象（<xxx> はスキップ）
        const QRegularExpression includePattern(
            QStringLiteral("^\\s*#\\s*include\\s+\"([^\"]+)\""));
        // コメント行を除外
        const QRegularExpression commentPattern(QStringLiteral("^\\s*//"));

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

                for (const auto &forbidden : std::as_const(rule.forbiddenDirs)) {
                    if (includePath.contains(forbidden)) {
                        violations.append(
                            QStringLiteral("  %1:%2 includes \"%3\" (forbidden: %4)")
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
