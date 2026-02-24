/// @file main.cpp
/// @brief アプリケーションエントリーポイントの実装

#include "mainwindow.h"
#include "logcategories.h"
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

// ログメッセージハンドラ（デバッグビルド専用）
//
// ビルドごとの動作:
//   デバッグビルド:
//     - debug.log ファイルを作成し、全レベルのメッセージを出力する
//     - ソース位置情報（ファイル名、行番号、関数名）も記録される
//   リリースビルド:
//     - qDebug/qCDebug はコンパイル時に除去される（QT_NO_DEBUG_OUTPUT）
//     - qCInfo/qCWarning/qCCritical は空のハンドラで破棄される
//     - debug.log ファイルは作成されない
//     - stderr への出力もない
static QFile *logFile = nullptr;

[[maybe_unused]] static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
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

    // カテゴリ（QLoggingCategory使用時のみ出力）
    QString category;
    if (context.category && qstrcmp(context.category, "default") != 0) {
        category = QString(" [%1]").arg(context.category);
    }

    // ソース位置（デバッグビルドでのみ利用可能）
    QString location;
    if (context.file) {
        const char *file = context.file;
        if (const char *slash = strrchr(file, '/'))
            file = slash + 1;
        else if (const char *bslash = strrchr(file, '\\'))
            file = bslash + 1;
        location = QString(" %1:%2").arg(file).arg(context.line);
        if (context.function)
            location += QString(" (%1)").arg(context.function);
    }

    out << timestamp << " [" << level << "]" << category << location << " " << msg << "\n";
    out.flush();
}

int main(int argc, char *argv[])
{
    // ログハンドラの設定（上記コメント参照）
#ifdef QT_DEBUG
    logFile = new QFile("debug.log");
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
    }
#else
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {});
#endif

    QApplication a(argc, argv);
    a.setApplicationName("ShogiBoardQ");

    // アプリケーションアイコンを設定
    a.setWindowIcon(QIcon(":/icons/shogiboardq.png"));

    // 言語設定を読み込み、適切な翻訳ファイルをロード
    QTranslator translator;
    QString langSetting = SettingsService::language();

    qCInfo(lcApp) << "Language setting:" << langSetting;
    qCInfo(lcApp) << "Application dir:" << QCoreApplication::applicationDirPath();

    bool loaded = false;
    if (langSetting == "ja_JP") {
        // 日本語を明示的に指定
        loaded = translator.load(":/i18n/ShogiBoardQ_ja_JP");
        qCInfo(lcApp) << "Trying :/i18n/ShogiBoardQ_ja_JP ->" << (loaded ? "SUCCESS" : "FAILED");
        if (!loaded) {
            // ビルドディレクトリから読み込みを試行
            loaded = translator.load(QCoreApplication::applicationDirPath() + "/ShogiBoardQ_ja_JP.qm");
            qCInfo(lcApp) << "Trying applicationDir/ShogiBoardQ_ja_JP.qm ->" << (loaded ? "SUCCESS" : "FAILED");
        }
        if (loaded) {
            a.installTranslator(&translator);
            qCInfo(lcApp) << "Translator installed for ja_JP";
        }
    } else if (langSetting == "en") {
        // 英語翻訳ファイルをロード
        loaded = translator.load(":/i18n/ShogiBoardQ_en");
        qCInfo(lcApp) << "Trying :/i18n/ShogiBoardQ_en ->" << (loaded ? "SUCCESS" : "FAILED");
        if (!loaded) {
            // ビルドディレクトリから読み込みを試行
            loaded = translator.load(QCoreApplication::applicationDirPath() + "/ShogiBoardQ_en.qm");
            qCInfo(lcApp) << "Trying applicationDir/ShogiBoardQ_en.qm ->" << (loaded ? "SUCCESS" : "FAILED");
        }
        if (loaded) {
            a.installTranslator(&translator);
            qCInfo(lcApp) << "Translator installed for en";
        } else {
            qCWarning(lcApp) << "English translation file not found!";
        }
    } else {
        // "system" またはその他: システムロケールに従う（既存の動作）
        qCInfo(lcApp) << "Using system locale";
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        qCInfo(lcApp) << "System UI languages:" << uiLanguages;
        for (const QString &locale : uiLanguages) {
            const QString baseName = "ShogiBoardQ_" + QLocale(locale).name();
            loaded = translator.load(":/i18n/" + baseName);
            qCInfo(lcApp) << "Trying :/i18n/" << baseName << "->" << (loaded ? "SUCCESS" : "FAILED");
            if (loaded) {
                a.installTranslator(&translator);
                qCInfo(lcApp) << "Translator installed for" << baseName;
                break;
            }
        }
    }

    // Creatorのような「Fusion」スタイルに統一する。
    a.setStyle(QStyleFactory::create("Fusion"));

    // QDialogButtonBox のデフォルトスタイル（全ダイアログ共通）
    a.setStyleSheet(QStringLiteral(
        "QDialogButtonBox QPushButton {"
        "  background-color: #e0e0e0; border: 1px solid #bdbdbd;"
        "  border-radius: 3px; padding: 4px 12px; min-width: 70px;"
        "}"
        "QDialogButtonBox QPushButton:hover { background-color: #d0d0d0; }"
        "QDialogButtonBox QPushButton:pressed { background-color: #bdbdbd; }"
        "QDialogButtonBox QPushButton:default {"
        "  background-color: #1976d2; color: white; border: 1px solid #1565c0;"
        "}"
        "QDialogButtonBox QPushButton:default:hover { background-color: #1e88e5; }"
        "QDialogButtonBox QPushButton:default:pressed { background-color: #1565c0; }"
    ));

    MainWindow w;
    w.show();

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
