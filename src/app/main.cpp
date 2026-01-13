#include "mainwindow.h"

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
    QString logPath = QCoreApplication::applicationDirPath() + "/debug.log";
    // applicationDirPathはQApplicationインスタンス作成後でないと使えないので、一時的に固定パス
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
    a.setWindowIcon(QIcon(":/shogiboardq.png"));

    QTranslator translator;

    const QStringList uiLanguages = QLocale::system().uiLanguages();

    for (const QString &locale : uiLanguages) {
        const QString baseName = "ShogiBoardQ_" + QLocale(locale).name();

        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Creatorのような「Fusion」スタイルに統一する。
    a.setStyle(QStyleFactory::create("Fusion"));

    MainWindow w;

    w.show();

    return a.exec();
}
