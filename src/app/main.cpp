/// @file main.cpp
/// @brief アプリケーションエントリーポイントの実装

#include "mainwindow.h"
#include "appsettings.h"
#include "applicationfonts.h"
#include "applicationlogging.h"
#include "applicationtranslations.h"
#include "settingsresetcontroller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>
#include <QIcon>

static QIcon applicationIcon()
{
#ifdef Q_OS_WIN
    // 実行ファイルと同じ複数サイズのICOを使い、高DPI用に512pxも用意する。
    QIcon icon(":/icons/shogiboardq.ico");
    icon.addFile(":/icons/windows/shogiboardq.png", QSize(512, 512));
    return icon;
#elif defined(Q_OS_LINUX)
    return QIcon(":/icons/linux/shogiboardq.png");
#else
    return QIcon(":/icons/shogiboardq.png");
#endif
}

// 翻訳の初期化後、ウィジェットの生成前に共通の外観を設定する。
static void configureApplicationAppearance(QApplication& app, const QString& language)
{
    // Creatorのような「Fusion」スタイルに統一する。
    app.setStyle(QStyleFactory::create("Fusion"));
    ApplicationFonts::initialize(AppSettings::uiFontFamily(), language);

    // QDialogButtonBox のデフォルトスタイル（全ダイアログ共通）
    app.setStyleSheet(QStringLiteral(
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
}

int main(int argc, char *argv[])
{
    // QApplication の破棄時にもログを出せるよう先に生成する。
    const ApplicationLogging logging;

    QApplication a(argc, argv);
    a.setApplicationName("ShogiBoardQ");
    a.setApplicationVersion(QStringLiteral(APP_VERSION));

    // コマンドライン引数。自動化 API（MCP サーバーやテストからの操作用）は既定で無効
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("ShogiBoardQ"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption automationOption(
        QStringLiteral("automation"),
        QStringLiteral("Enable the local automation API (JSON-RPC over a user-only local socket)."));
    const QCommandLineOption automationSocketOption(
        QStringLiteral("automation-socket"),
        QStringLiteral("Socket path (or pipe name on Windows) for the automation API."),
        QStringLiteral("path"));
    parser.addOption(automationOption);
    parser.addOption(automationSocketOption);
    parser.process(a);
#ifdef Q_OS_LINUX
    // Waylandでもデスクトップエントリのアイコンと関連付ける。
    a.setDesktopFileName(QStringLiteral("shogiboardq"));
#endif

    // アプリケーションアイコンを設定
    a.setWindowIcon(applicationIcon());

    // 終了時の設定初期化メッセージも翻訳できるよう、このスコープで保持する。
    const ApplicationTranslations translations;

    configureApplicationAppearance(a, translations.language());

    int result;
    {
        MainWindow w;
        w.show();

        if (parser.isSet(automationOption)) {
            w.startAutomationServer(parser.value(automationSocketOption));
        }

        result = a.exec();
    }

    // デストラクタによる設定の再保存も終わってから初期化する。
    result = SettingsResetController::finalizeExit(result);

    return result;
}
