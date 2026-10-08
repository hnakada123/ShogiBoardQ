/// @file tst_app_paths.cpp
/// @brief 設定・データ・キャッシュの置き場所を環境変数で切り替えられることのテスト

#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "apppaths.h"
#include "settingscommon.h"

class TestAppPaths : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("ShogiBoardQ"));
        QVERIFY(m_root.isValid());
    }

    // 指定がなければ Qt の標準の場所
    void defaultsToStandardLocations()
    {
        qunsetenv("SHOGIBOARDQ_CONFIG_HOME");
        qunsetenv("SHOGIBOARDQ_DATA_HOME");
        qunsetenv("SHOGIBOARDQ_CACHE_HOME");
        QCOMPARE(AppPaths::configDirectory(), QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        QCOMPARE(AppPaths::dataDirectory(), QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        QCOMPARE(AppPaths::cacheDirectory(), QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    }

    // 指定するとその下のアプリ名のフォルダ（どの OS でも。macOS の Qt は XDG_* を見ない）
    void environmentOverridesEveryPlatform()
    {
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_root.filePath(QStringLiteral("config")).toUtf8());
        qputenv("SHOGIBOARDQ_DATA_HOME", m_root.filePath(QStringLiteral("data")).toUtf8());
        qputenv("SHOGIBOARDQ_CACHE_HOME", m_root.filePath(QStringLiteral("cache")).toUtf8());
        QCOMPARE(AppPaths::configDirectory(), m_root.filePath(QStringLiteral("config/ShogiBoardQ")));
        QCOMPARE(AppPaths::dataDirectory(), m_root.filePath(QStringLiteral("data/ShogiBoardQ")));
        QCOMPARE(AppPaths::cacheDirectory(), m_root.filePath(QStringLiteral("cache/ShogiBoardQ")));

        // 設定ファイル（最初の呼び出しで場所が決まる）もそこに作られる
        QCOMPARE(SettingsCommon::settingsFilePath(), m_root.filePath(QStringLiteral("config/ShogiBoardQ/ShogiBoardQ.ini")));
        QSettings& settings = SettingsCommon::openSettings();
        settings.setValue(QStringLiteral("Test/value"), 1);
        settings.sync();
        QVERIFY(QFile::exists(m_root.filePath(QStringLiteral("config/ShogiBoardQ/ShogiBoardQ.ini"))));
    }
};

QTEST_GUILESS_MAIN(TestAppPaths)
#include "tst_app_paths.moc"
