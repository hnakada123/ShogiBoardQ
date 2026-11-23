/// @file tst_executabledirectory.cpp
/// @brief 実行ファイルの実体があるフォルダ（ExecutableDirectory）のテスト
///
/// Arch Linux のパッケージは /usr/bin/ShogiBoardQ を /usr/lib/shogiboardq/ShogiBoardQ へのリンクにしている。
/// Qt 6.12 の applicationDirPath() はリンクのあるフォルダを返すので、そこから起動すると P2P対局の通信プロセスと
/// 翻訳を見つけられなかった。このテストは自分自身へのリンクを作って起動し、リンクをたどった先を返すかを確かめる。

#include "executabledirectory.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>
#include <cstring>

namespace {

/// 子プロセスとして起動されたとき、ExecutableDirectory::path() を標準出力に書く引数
constexpr const char* kPrintArgument = "--print-executable-directory";

QString realDirectory()
{
    return QFileInfo(QCoreApplication::applicationFilePath()).canonicalPath();
}

} // namespace

class TestExecutableDirectory : public QObject
{
    Q_OBJECT

private slots:
    void directLaunchReturnsOwnDirectory();
    void symlinkLaunchReturnsRealDirectory_data();
    void symlinkLaunchReturnsRealDirectory();
};

// ----------------------------------------------------------------------------
// 実体から起動したときは、実行ファイルのフォルダを返すか
// ----------------------------------------------------------------------------

void TestExecutableDirectory::directLaunchReturnsOwnDirectory()
{
    QCOMPARE(ExecutableDirectory::path(), realDirectory());
}

// ----------------------------------------------------------------------------
// 別のフォルダのリンクから起動したとき（絶対パス・PATH から探した名前）も、リンクをたどった先を返すか
// ----------------------------------------------------------------------------

void TestExecutableDirectory::symlinkLaunchReturnsRealDirectory_data()
{
    QTest::addColumn<bool>("throughPath");
    QTest::newRow("absolute path of the link") << false;
    QTest::newRow("name found in PATH") << true;
}

void TestExecutableDirectory::symlinkLaunchReturnsRealDirectory()
{
#ifdef Q_OS_WIN
    QSKIP("Windows の QFile::link はショートカット（.lnk）を作り、シンボリックリンクにはならない");
#else
    QFETCH(bool, throughPath);

    QTemporaryDir linkDirectory;
    QVERIFY(linkDirectory.isValid());
    const QString linkName = QStringLiteral("tst_executabledirectory_link");
    const QString linkPath = QDir(linkDirectory.path()).filePath(linkName);
    QVERIFY(QFile::link(QCoreApplication::applicationFilePath(), linkPath));
    QVERIFY(QDir(linkDirectory.path()).canonicalPath() != realDirectory());

    // アプリケーションメニューの起動と同じく、名前（PATH から探す）か絶対パスを argv[0] にして起動する
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"),
                       linkDirectory.path() + QLatin1Char(':') + environment.value(QStringLiteral("PATH")));
    QProcess process;
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral("/bin/sh"),
                  {QStringLiteral("-c"), QStringLiteral("exec \"$0\" \"$1\""), throughPath ? linkName : linkPath,
                   QString::fromLatin1(kPrintArgument)});
    QVERIFY(process.waitForFinished(30000));
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(QString::fromUtf8(process.readAllStandardOutput()).trimmed(), realDirectory());
#endif
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], kPrintArgument) == 0) {
        QCoreApplication app(argc, argv);
        std::fputs(ExecutableDirectory::path().toLocal8Bit().constData(), stdout);
        return 0;
    }
    QCoreApplication app(argc, argv);
    TestExecutableDirectory test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_executabledirectory.moc"
