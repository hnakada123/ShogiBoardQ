/// @file main.cpp
/// @brief アプリケーションエントリーポイントの実装
/// @todo remove コメントスタイルガイド適用済み

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
