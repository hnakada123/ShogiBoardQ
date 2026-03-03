/// @file tst_translation_files.cpp
/// @brief 翻訳ファイル品質テスト

#include <QtTest>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>
#include <QSet>

class TestTranslationFiles : public QObject
{
    Q_OBJECT

private:
    QString translationsDir() const
    {
        return QStringLiteral(SOURCE_DIR "/resources/translations");
    }

    QString jaPath() const
    {
        return translationsDir() + QStringLiteral("/ShogiBoardQ_ja_JP.ts");
    }

    QString enPath() const
    {
        return translationsDir() + QStringLiteral("/ShogiBoardQ_en.ts");
    }

    QString baselinePath() const
    {
        return translationsDir() + QStringLiteral("/baseline-unfinished.txt");
    }

    int countUnfinished(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;
        int count = 0;
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()
                && xml.name() == QLatin1String("translation")) {
                if (xml.attributes().value(QLatin1String("type"))
                    == QLatin1String("unfinished"))
                    ++count;
            }
        }
        return count;
    }

    int countTranslationEntries(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;
        int count = 0;
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()
                && xml.name() == QLatin1String("translation"))
                ++count;
        }
        return count;
    }

    QSet<QString> collectContextNames(const QString& filePath)
    {
        QFile file(filePath);
        QSet<QString> names;
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return names;
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()
                && xml.name() == QLatin1String("name")) {
                names.insert(xml.readElementText());
            }
        }
        return names;
    }

private slots:
    // --- 翻訳ソースファイル存在テスト ---

    void tsFile_japanese_exists()
    {
        QVERIFY2(QFile::exists(jaPath()),
                 "ShogiBoardQ_ja_JP.ts not found");
    }

    void tsFile_english_exists()
    {
        QVERIFY2(QFile::exists(enPath()),
                 "ShogiBoardQ_en.ts not found");
    }

    // --- 翻訳ソースファイル品質テスト ---

    void tsFile_japanese_wellFormedXml()
    {
        QFile file(jaPath());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QXmlStreamReader xml(&file);
        while (!xml.atEnd())
            xml.readNext();
        QVERIFY2(!xml.hasError(),
                 qPrintable(QStringLiteral("XML parse error: %1")
                                .arg(xml.errorString())));
    }

    void tsFile_english_wellFormedXml()
    {
        QFile file(enPath());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QXmlStreamReader xml(&file);
        while (!xml.atEnd())
            xml.readNext();
        QVERIFY2(!xml.hasError(),
                 qPrintable(QStringLiteral("XML parse error: %1")
                                .arg(xml.errorString())));
    }

    void tsFile_japanese_hasTranslations()
    {
        int count = countTranslationEntries(jaPath());
        QVERIFY2(count > 0,
                 qPrintable(QStringLiteral("No translation entries found (count=%1)")
                                .arg(count)));
    }

    void tsFile_english_hasTranslations()
    {
        int count = countTranslationEntries(enPath());
        QVERIFY2(count > 0,
                 qPrintable(QStringLiteral("No translation entries found (count=%1)")
                                .arg(count)));
    }

    // --- 未翻訳ベースラインテスト ---

    void baseline_exists()
    {
        QVERIFY2(QFile::exists(baselinePath()),
                 "baseline-unfinished.txt not found");
    }

    void baseline_unfinishedCount_matchesOrBetter()
    {
        QFile baseline(baselinePath());
        QVERIFY(baseline.open(QIODevice::ReadOnly | QIODevice::Text));

        while (!baseline.atEnd()) {
            const QString line =
                QString::fromUtf8(baseline.readLine()).trimmed();
            if (line.isEmpty())
                continue;

            const auto sep = line.lastIndexOf(QLatin1Char(':'));
            QVERIFY2(sep > 0,
                     qPrintable(
                         QStringLiteral("Bad baseline format: %1").arg(line)));

            const QString fileName = line.left(sep);
            bool ok = false;
            const int baselineCount = line.mid(sep + 1).toInt(&ok);
            QVERIFY2(ok,
                     qPrintable(
                         QStringLiteral("Bad count in baseline: %1").arg(line)));

            const QString filePath =
                translationsDir() + QLatin1Char('/') + fileName;
            const int actual = countUnfinished(filePath);
            QVERIFY2(actual >= 0,
                     qPrintable(
                         QStringLiteral("Cannot read: %1").arg(filePath)));

            QVERIFY2(actual <= baselineCount,
                     qPrintable(QStringLiteral("%1: unfinished %2 > baseline %3")
                                    .arg(fileName)
                                    .arg(actual)
                                    .arg(baselineCount)));
        }
    }

    // --- 翻訳整合性テスト ---

    void tsFile_english_noEmptyFinishedTranslation()
    {
        // 英語ファイルで検証（日本語は原文言語のため空翻訳は正常）
        QFile file(enPath());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

        QXmlStreamReader xml(&file);
        int emptyCount = 0;
        QString lastSource;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == QLatin1String("source")) {
                    lastSource = xml.readElementText();
                } else if (xml.name() == QLatin1String("translation")) {
                    const bool unfinished =
                        xml.attributes().value(QLatin1String("type"))
                        == QLatin1String("unfinished");
                    const QString text = xml.readElementText();
                    if (!unfinished && text.isEmpty()) {
                        ++emptyCount;
                        qWarning() << "Empty finished translation for source:"
                                   << lastSource;
                    }
                }
            }
        }

        QVERIFY2(emptyCount == 0,
                 qPrintable(QStringLiteral("%1 finished translation(s) are empty")
                                .arg(emptyCount)));
    }

    void tsFile_contexts_jaSubsetOfEn()
    {
        const QSet<QString> jaContexts = collectContextNames(jaPath());
        const QSet<QString> enContexts = collectContextNames(enPath());

        QVERIFY2(!jaContexts.isEmpty(), "No contexts found in ja_JP.ts");
        QVERIFY2(!enContexts.isEmpty(), "No contexts found in en.ts");

        // 日本語のコンテキストは全て英語にも存在すること
        const QSet<QString> jaOnly = jaContexts - enContexts;

        if (!jaOnly.isEmpty())
            qWarning() << "Contexts only in ja_JP:" << jaOnly;

        QVERIFY2(jaOnly.isEmpty(),
                 qPrintable(QStringLiteral("%1 context(s) in ja_JP missing from en")
                                .arg(jaOnly.size())));
    }
};

QTEST_GUILESS_MAIN(TestTranslationFiles)
#include "tst_translation_files.moc"
