/// @file main.cpp
/// @brief アプリケーションエントリーポイントの実装

#include "mainwindow.h"
#include "logcategories.h"
#include "appsettings.h"
#include "kifupresentation.h"
#include "applicationfonts.h"
#include "applicationlogging.h"
#include "settingsresetcontroller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QLibraryInfo>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QToolTip>
#include <QIcon>
#include <QStandardPaths>

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

    // 言語設定を読み込み、適切な翻訳ファイルをロード
    QTranslator translator;
    const QStringList systemLanguages = QLocale::system().uiLanguages();
    const QString language = KifuPresentation::resolveLanguage(AppSettings::language(),
        systemLanguages.isEmpty() ? QLocale::system().name() : systemLanguages.first());
    QTranslator qtTranslator;
    const QString qtLanguage = language == QStringLiteral("ja_JP") ? QStringLiteral("ja") : language;
    if (qtTranslator.load(QStringLiteral(":/translations/qt/qtbase_") + qtLanguage + QStringLiteral(".qm"))
        || qtTranslator.load(QStringLiteral("qtbase_") + qtLanguage, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        a.installTranslator(&qtTranslator);
    if (translator.load(QCoreApplication::applicationDirPath() + "/ShogiBoardQ_" + language + ".qm"))
        a.installTranslator(&translator);
    else
        qCWarning(lcApp) << "Translation file not found:" << language;
    KifuPresentation::configure(language, AppSettings::moveNotation(), AppSettings::notationOrigin());

    // Creatorのような「Fusion」スタイルに統一する。
    a.setStyle(QStyleFactory::create("Fusion"));
    ApplicationFonts::initialize(AppSettings::uiFontFamily(), language);

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
