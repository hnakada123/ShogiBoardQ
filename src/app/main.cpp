#include "mainwindow.h"
#include "settingsservice.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QToolTip>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QCommandLineParser>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

// デバッグメッセージをファイルに出力するハンドラ
static QFile *logFile = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context)
    if (!logFile) return;

    QTextStream out(logFile);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG"; break;
    case QtInfoMsg:     level = "INFO"; break;
    case QtWarningMsg:  level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg:    level = "FATAL"; break;
    }
    out << timestamp << " [" << level << "] " << msg << "\n";
    out.flush();
}

int main(int argc, char *argv[])
{
    // ログファイルを設定（実行ファイルと同じディレクトリに出力）
    // オリジナルの作業ディレクトリを保存（SettingsServiceがCWDを変更する前に）
    const QString originalCwd = QDir::currentPath();

    // 注: applicationDirPathはQApplicationインスタンス作成後でないと使えないため固定パス
    logFile = new QFile("debug.log");
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
    }

    // 高DPI対応（Qt 6ではデフォルトで有効だが明示的に設定）
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication a(argc, argv);

    // アプリケーションアイコンを設定
    a.setWindowIcon(QIcon(":/icons/shogiboardq.png"));

    // 言語設定を読み込み、適切な翻訳ファイルをロード
    QTranslator translator;
    QString langSetting = SettingsService::language();

    qDebug() << "[i18n] Language setting:" << langSetting;
    qDebug() << "[i18n] Application dir:" << QCoreApplication::applicationDirPath();

    bool loaded = false;
    if (langSetting == "ja_JP") {
        // 日本語を明示的に指定
        loaded = translator.load(":/i18n/ShogiBoardQ_ja_JP");
        qDebug() << "[i18n] Trying :/i18n/ShogiBoardQ_ja_JP ->" << (loaded ? "SUCCESS" : "FAILED");
        if (!loaded) {
            // ビルドディレクトリから読み込みを試行
            loaded = translator.load(QCoreApplication::applicationDirPath() + "/ShogiBoardQ_ja_JP.qm");
            qDebug() << "[i18n] Trying applicationDir/ShogiBoardQ_ja_JP.qm ->" << (loaded ? "SUCCESS" : "FAILED");
        }
        if (loaded) {
            a.installTranslator(&translator);
            qDebug() << "[i18n] Translator installed for ja_JP";
        }
    } else if (langSetting == "en") {
        // 英語翻訳ファイルをロード
        loaded = translator.load(":/i18n/ShogiBoardQ_en");
        qDebug() << "[i18n] Trying :/i18n/ShogiBoardQ_en ->" << (loaded ? "SUCCESS" : "FAILED");
        if (!loaded) {
            // ビルドディレクトリから読み込みを試行
            loaded = translator.load(QCoreApplication::applicationDirPath() + "/ShogiBoardQ_en.qm");
            qDebug() << "[i18n] Trying applicationDir/ShogiBoardQ_en.qm ->" << (loaded ? "SUCCESS" : "FAILED");
        }
        if (loaded) {
            a.installTranslator(&translator);
            qDebug() << "[i18n] Translator installed for en";
        } else {
            qDebug() << "[i18n] WARNING: English translation file not found!";
        }
    } else {
        // "system" またはその他: システムロケールに従う（既存の動作）
        qDebug() << "[i18n] Using system locale";
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        qDebug() << "[i18n] System UI languages:" << uiLanguages;
        for (const QString &locale : uiLanguages) {
            const QString baseName = "ShogiBoardQ_" + QLocale(locale).name();
            loaded = translator.load(":/i18n/" + baseName);
            qDebug() << "[i18n] Trying :/i18n/" << baseName << "->" << (loaded ? "SUCCESS" : "FAILED");
            if (loaded) {
                a.installTranslator(&translator);
                qDebug() << "[i18n] Translator installed for" << baseName;
                break;
            }
        }
    }

    // Creatorのような「Fusion」スタイルに統一する。
    a.setStyle(QStyleFactory::create("Fusion"));

    // コマンドライン引数の解析
    QCommandLineParser parser;
    parser.setApplicationDescription("ShogiBoardQ - Japanese Chess Board Application");
    parser.addHelpOption();
    parser.addVersionOption();

    // --load-kif <file> : 起動時にKIFファイルを読み込む
    QCommandLineOption loadKifOption(
        QStringList() << "load-kif",
        "Load a KIF/KI2/CSA file on startup.",
        "file");
    parser.addOption(loadKifOption);

    // --test-mode : テストモード（詳細なログ出力）
    QCommandLineOption testModeOption(
        QStringList() << "test-mode",
        "Enable test mode with verbose state logging.");
    parser.addOption(testModeOption);

    // --dump-state-after <ms> : 指定ミリ秒後に状態をダンプして終了
    QCommandLineOption dumpStateOption(
        QStringList() << "dump-state-after",
        "Dump UI state after <ms> milliseconds and exit.",
        "ms");
    parser.addOption(dumpStateOption);

    // --click-branch <index> : 指定インデックスの分岐候補をクリック
    QCommandLineOption clickBranchOption(
        QStringList() << "click-branch",
        "Click branch candidate at <index> (0-based).",
        "index");
    parser.addOption(clickBranchOption);

    // --navigate-to <ply> : 指定手数まで移動
    QCommandLineOption navigateToOption(
        QStringList() << "navigate-to",
        "Navigate to specified ply (0-based).",
        "ply");
    parser.addOption(navigateToOption);

    // --click-next <count> : 1手進むボタンを指定回数クリック
    QCommandLineOption clickNextOption(
        QStringList() << "click-next",
        "Click next button <count> times.",
        "count");
    parser.addOption(clickNextOption);

    // --click-prev <count> : 1手戻るボタンを指定回数クリック
    QCommandLineOption clickPrevOption(
        QStringList() << "click-prev",
        "Click prev button <count> times.",
        "count");
    parser.addOption(clickPrevOption);

    // --click-branch2 <index> : 2回目の分岐候補クリック（next/prev後に実行）
    QCommandLineOption clickBranch2Option(
        QStringList() << "click-branch2",
        "Click branch candidate at <index> after next/prev buttons.",
        "index");
    parser.addOption(clickBranch2Option);

    // --click-next2 <count> : click-branch2後の1手進む
    QCommandLineOption clickNext2Option(
        QStringList() << "click-next2",
        "Click next button <count> times after click-branch2.",
        "count");
    parser.addOption(clickNext2Option);

    // --click-prev2 <count> : click-branch2後の1手戻る
    QCommandLineOption clickPrev2Option(
        QStringList() << "click-prev2",
        "Click prev button <count> times after click-branch2.",
        "count");
    parser.addOption(clickPrev2Option);

    // --click-kifu-row <row> : 棋譜欄の行を直接クリック
    QCommandLineOption clickKifuRowOption(
        QStringList() << "click-kifu-row",
        "Click kifu list row directly.",
        "row");
    parser.addOption(clickKifuRowOption);

    // --click-tree-node <row,ply> : 分岐ツリーのノードを直接クリック
    QCommandLineOption clickTreeNodeOption(
        QStringList() << "click-tree-node",
        "Click branch tree node directly at <row,ply>.",
        "row,ply");
    parser.addOption(clickTreeNodeOption);

    // --click-tree-node2 <row,ply> : 2回目の分岐ツリーノードクリック
    QCommandLineOption clickTreeNode2Option(
        QStringList() << "click-tree-node2",
        "Click branch tree node directly at <row,ply> (second click).",
        "row,ply");
    parser.addOption(clickTreeNode2Option);

    // --click-next3 <count> : click-prev2後の1手進む
    QCommandLineOption clickNext3Option(
        QStringList() << "click-next3",
        "Click next button <count> times after click-prev2.",
        "count");
    parser.addOption(clickNext3Option);

    // --click-next-after-kifu <count> : click-kifu-row後の1手進む
    QCommandLineOption clickNextAfterKifuOption(
        QStringList() << "click-next-after-kifu",
        "Click next button <count> times after click-kifu-row.",
        "count");
    parser.addOption(clickNextAfterKifuOption);

    // --click-next-after-prev <count> : click-prev後の1手進む
    QCommandLineOption clickNextAfterPrevOption(
        QStringList() << "click-next-after-prev",
        "Click next button <count> times after click-prev.",
        "count");
    parser.addOption(clickNextAfterPrevOption);

    // --click-first : 先頭へボタンをクリック
    QCommandLineOption clickFirstOption(
        QStringList() << "click-first",
        "Click first button (go to start position).");
    parser.addOption(clickFirstOption);

    // --click-last : 末尾へボタンをクリック
    QCommandLineOption clickLastOption(
        QStringList() << "click-last",
        "Click last button (go to end position).");
    parser.addOption(clickLastOption);

    parser.process(a);

    MainWindow w;

    // テストモードの設定
    const bool testMode = parser.isSet(testModeOption);
    if (testMode) {
        w.setTestMode(true);
        qDebug() << "[TEST] Test mode enabled";
    }

    w.show();

    // KIFファイルの自動読み込み
    if (parser.isSet(loadKifOption)) {
        const QString kifFile = parser.value(loadKifOption);
        // 相対パスを絶対パスに変換（オリジナルのCWDを使用）
        QString absoluteKifFile;
        if (QFileInfo(kifFile).isAbsolute()) {
            absoluteKifFile = kifFile;
        } else {
            absoluteKifFile = QDir(originalCwd).absoluteFilePath(kifFile);
        }
        qDebug() << "[TEST] Auto-loading KIF file:" << absoluteKifFile;
        // ウィンドウ表示後に読み込みを遅延実行
        QTimer::singleShot(500, &w, [&w, absoluteKifFile]() {
            w.loadKifuFile(absoluteKifFile);
        });
    }

    // 手数へのナビゲーション
    if (parser.isSet(navigateToOption)) {
        const int ply = parser.value(navigateToOption).toInt();
        qDebug() << "[TEST] Auto-navigating to ply:" << ply;
        // KIF読み込み後に実行（1秒後）
        QTimer::singleShot(1000, &w, [&w, ply]() {
            w.navigateToPly(ply);
        });
    }

    // 分岐候補クリックの自動実行
    // click-tree-nodeまたはclick-kifu-rowが設定されている場合は、それらの後に実行
    // ★ 手動操作をシミュレートするため、他の操作後に十分な遅延を設ける
    int branchClickTime;
    if (parser.isSet(clickKifuRowOption)) {
        // kifu-rowクリック後に分岐候補クリック
        branchClickTime = 1500 + 500;  // kifu-row (1500ms) + 500ms
    } else if (parser.isSet(clickTreeNodeOption)) {
        branchClickTime = 3500;
    } else {
        branchClickTime = 1500;
    }
    if (parser.isSet(clickBranchOption)) {
        const int branchIndex = parser.value(clickBranchOption).toInt();
        qDebug() << "[TEST] Auto-clicking branch index:" << branchIndex << "at" << branchClickTime << "ms";
        QTimer::singleShot(branchClickTime, &w, [&w, branchIndex]() {
            w.clickBranchCandidate(branchIndex);
        });
    }

    // 1手進むボタンの自動クリック
    const int nextCount = parser.isSet(clickNextOption) ? parser.value(clickNextOption).toInt() : 0;
    const int nextStartTime = branchClickTime + 500; // 分岐クリック後に実行
    if (nextCount > 0) {
        qDebug() << "[TEST] Auto-clicking next button" << nextCount << "times starting at" << nextStartTime << "ms";
        for (int i = 0; i < nextCount; ++i) {
            QTimer::singleShot(nextStartTime + i * 100, &w, [&w]() {
                w.clickNextButton();
            });
        }
    }

    // 1手戻るボタンの自動クリック（nextボタンまたはlastボタンの後に実行）
    const int prevCount = parser.isSet(clickPrevOption) ? parser.value(clickPrevOption).toInt() : 0;
    // lastオプションが設定されていて、他の操作がない場合はlast後に実行
    // ※ lastClickTimeはまだ計算されていないので、ここで計算
    int prevStartTime;
    if (parser.isSet(clickLastOption) && nextCount == 0 && !parser.isSet(clickBranchOption)) {
        // --click-lastの後に実行
        prevStartTime = 1500 + 500;  // last (1500ms) + 500ms
    } else {
        prevStartTime = nextStartTime + nextCount * 100 + 500;
    }
    if (prevCount > 0) {
        qDebug() << "[TEST] Auto-clicking prev button" << prevCount << "times starting at" << prevStartTime << "ms";
        for (int i = 0; i < prevCount; ++i) {
            QTimer::singleShot(prevStartTime + i * 100, &w, [&w]() {
                w.clickPrevButton();
            });
        }
    }

    // click-prev後の1手進む
    const int nextAfterPrevCount = parser.isSet(clickNextAfterPrevOption) ? parser.value(clickNextAfterPrevOption).toInt() : 0;
    const int nextAfterPrevStartTime = prevStartTime + prevCount * 100 + 500;
    if (nextAfterPrevCount > 0) {
        qDebug() << "[TEST] Auto-clicking next-after-prev button" << nextAfterPrevCount << "times starting at" << nextAfterPrevStartTime << "ms";
        for (int i = 0; i < nextAfterPrevCount; ++i) {
            QTimer::singleShot(nextAfterPrevStartTime + i * 100, &w, [&w]() {
                w.clickNextButton();
            });
        }
    }

    // 2回目の分岐候補クリック（next/prev後に実行）
    const int branch2StartTime = nextAfterPrevStartTime + nextAfterPrevCount * 100 + 500;
    if (parser.isSet(clickBranch2Option)) {
        const int branch2Index = parser.value(clickBranch2Option).toInt();
        qDebug() << "[TEST] Auto-clicking branch2 index:" << branch2Index;
        QTimer::singleShot(branch2StartTime, &w, [&w, branch2Index]() {
            w.clickBranchCandidate(branch2Index);
        });
    }

    // click-branch2後の1手進む
    const int next2Count = parser.isSet(clickNext2Option) ? parser.value(clickNext2Option).toInt() : 0;
    if (next2Count > 0) {
        qDebug() << "[TEST] Auto-clicking next2 button" << next2Count << "times";
        const int next2StartTime = branch2StartTime + 500;
        for (int i = 0; i < next2Count; ++i) {
            QTimer::singleShot(next2StartTime + i * 100, &w, [&w]() {
                w.clickNextButton();
            });
        }
    }

    // click-branch2後の1手戻る
    const int prev2Count = parser.isSet(clickPrev2Option) ? parser.value(clickPrev2Option).toInt() : 0;
    if (prev2Count > 0) {
        qDebug() << "[TEST] Auto-clicking prev2 button" << prev2Count << "times";
        const int prev2StartTime = branch2StartTime + 500 + next2Count * 100 + 500;
        for (int i = 0; i < prev2Count; ++i) {
            QTimer::singleShot(prev2StartTime + i * 100, &w, [&w]() {
                w.clickPrevButton();
            });
        }
    }

    // click-prev2後の1手進む
    const int next3Count = parser.isSet(clickNext3Option) ? parser.value(clickNext3Option).toInt() : 0;
    if (next3Count > 0) {
        qDebug() << "[TEST] Auto-clicking next3 button" << next3Count << "times";
        const int next3StartTime = branch2StartTime + 500 + next2Count * 100 + 500 + prev2Count * 100 + 500;
        for (int i = 0; i < next3Count; ++i) {
            QTimer::singleShot(next3StartTime + i * 100, &w, [&w]() {
                w.clickNextButton();
            });
        }
    }

    // 分岐ツリーのノードを直接クリック
    // KIF読み込み後に実行
    int treeNodeClickTime = 1500;
    if (parser.isSet(clickTreeNodeOption)) {
        const QString value = parser.value(clickTreeNodeOption);
        const QStringList parts = value.split(QLatin1Char(','));
        if (parts.size() == 2) {
            const int treeRow = parts.at(0).toInt();
            const int treePly = parts.at(1).toInt();
            qDebug() << "[TEST] Auto-clicking tree node: row=" << treeRow << "ply=" << treePly;
            treeNodeClickTime = 1500;
            QTimer::singleShot(treeNodeClickTime, &w, [&w, treeRow, treePly]() {
                w.clickBranchTreeNode(treeRow, treePly);
            });
        }
    }

    // 2回目の分岐ツリーノードクリック
    // tree-node後に実行
    int treeNode2ClickTime = treeNodeClickTime + 1500;
    if (parser.isSet(clickTreeNode2Option)) {
        const QString value = parser.value(clickTreeNode2Option);
        const QStringList parts = value.split(QLatin1Char(','));
        if (parts.size() == 2) {
            const int treeRow = parts.at(0).toInt();
            const int treePly = parts.at(1).toInt();
            qDebug() << "[TEST] Auto-clicking tree node2: row=" << treeRow << "ply=" << treePly;
            QTimer::singleShot(treeNode2ClickTime, &w, [&w, treeRow, treePly]() {
                w.clickBranchTreeNode(treeRow, treePly);
            });
        }
    }

    // 棋譜欄の行を直接クリック
    // 他の操作（tree-node等）がある場合はその後に実行、なければKIF読み込み直後に実行
    // 注: click-branchはkifu-row後に実行されるので、hasOtherOpsから除外
    if (parser.isSet(clickKifuRowOption)) {
        const int kifuRow = parser.value(clickKifuRowOption).toInt();
        qDebug() << "[TEST] Auto-clicking kifu row:" << kifuRow;

        int kifuRowStartTime;
        const bool hasOtherOps = nextCount > 0 || prevCount > 0 ||
                                  parser.isSet(clickBranch2Option) || next2Count > 0 ||
                                  prev2Count > 0 || next3Count > 0 || parser.isSet(clickTreeNodeOption);
        if (hasOtherOps) {
            // 他の操作がある場合は全操作完了後
            kifuRowStartTime = branch2StartTime + 500 + next2Count * 100 + 500 + prev2Count * 100 + 500 + next3Count * 100 + 500;
        } else {
            // 他の操作がなければKIF読み込み後すぐ
            kifuRowStartTime = 1500;
        }

        QTimer::singleShot(kifuRowStartTime, &w, [&w, kifuRow]() {
            w.clickKifuRow(kifuRow);
        });

        // click-kifu-row後のnextボタンクリック
        const int nextAfterKifuCount = parser.isSet(clickNextAfterKifuOption)
                                           ? parser.value(clickNextAfterKifuOption).toInt()
                                           : 0;
        if (nextAfterKifuCount > 0) {
            const int nextAfterKifuStartTime = kifuRowStartTime + 500;
            qDebug() << "[TEST] Auto-clicking next after kifu" << nextAfterKifuCount << "times at" << nextAfterKifuStartTime << "ms";
            for (int i = 0; i < nextAfterKifuCount; ++i) {
                QTimer::singleShot(nextAfterKifuStartTime + i * 100, &w, [&w]() {
                    w.clickNextButton();
                });
            }
        }
    }

    // 先頭へボタンの自動クリック
    // 他の操作がある場合は全操作完了後、なければKIF読み込み直後に実行
    // 全操作完了後の時間を計算
    int allOpsCompletedTime = branch2StartTime + 500 + next2Count * 100 + 500 + prev2Count * 100 + 500 + next3Count * 100 + 500;
    if (parser.isSet(clickKifuRowOption)) {
        const int nextAfterKifuCount = parser.isSet(clickNextAfterKifuOption)
                                           ? parser.value(clickNextAfterKifuOption).toInt()
                                           : 0;
        allOpsCompletedTime += 500 + nextAfterKifuCount * 100;
    }

    int firstClickTime = 0;
    if (parser.isSet(clickFirstOption)) {
        const bool hasOtherOps = parser.isSet(clickBranchOption) || nextCount > 0 || prevCount > 0 ||
                                  parser.isSet(clickBranch2Option) || next2Count > 0 ||
                                  prev2Count > 0 || next3Count > 0 || parser.isSet(clickTreeNodeOption) ||
                                  parser.isSet(clickKifuRowOption);
        if (hasOtherOps) {
            firstClickTime = allOpsCompletedTime + 500;
        } else {
            firstClickTime = 1500;
        }
        qDebug() << "[TEST] Auto-clicking first button at" << firstClickTime << "ms";
        QTimer::singleShot(firstClickTime, &w, [&w]() {
            w.clickFirstButton();
        });
    }

    // 末尾へボタンの自動クリック
    // firstボタンの後に実行（設定されている場合）
    if (parser.isSet(clickLastOption)) {
        int lastClickTime;
        if (parser.isSet(clickFirstOption)) {
            lastClickTime = firstClickTime + 500;
        } else {
            const bool hasOtherOps = parser.isSet(clickBranchOption) || nextCount > 0 || prevCount > 0 ||
                                      parser.isSet(clickBranch2Option) || next2Count > 0 ||
                                      prev2Count > 0 || next3Count > 0 || parser.isSet(clickTreeNodeOption) ||
                                      parser.isSet(clickKifuRowOption);
            if (hasOtherOps) {
                lastClickTime = allOpsCompletedTime + 500;
            } else {
                lastClickTime = 1500;
            }
        }
        qDebug() << "[TEST] Auto-clicking last button at" << lastClickTime << "ms";
        QTimer::singleShot(lastClickTime, &w, [&w]() {
            w.clickLastButton();
        });
    }

    // 状態ダンプ＆終了
    if (parser.isSet(dumpStateOption)) {
        const int delayMs = parser.value(dumpStateOption).toInt();
        qDebug() << "[TEST] Will dump state and exit after" << delayMs << "ms";
        QTimer::singleShot(delayMs, &w, [&w]() {
            w.dumpTestState();
            QApplication::quit();
        });
    }

    int result = a.exec();

    // ログファイルのクリーンアップ
    if (logFile) {
        qInstallMessageHandler(nullptr);  // デフォルトハンドラに戻す
        logFile->close();
        delete logFile;
        logFile = nullptr;
    }

    return result;
}
