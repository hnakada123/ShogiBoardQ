/// @file main.cpp
/// @brief shogiboardq-cli のエントリーポイント
///
/// GUI を起動せずに棋譜変換・SFEN 検証・エンジン解析・詰将棋生成などを実行する。
/// MCP サーバー（mcp/shogiboardq_mcp）から呼ばれることを想定し、標準出力は JSON 専用とする。

#include "clicommands.h"
#include "clioutput.h"

#include <QApplication>
#include <QHash>
#include <QStringList>

#include <functional>

int CliCommands::run(const QStringList& arguments)
{
    using Handler = std::function<int(const QStringList&)>;
    static const QHash<QString, Handler> handlers = {
        {QStringLiteral("version"), &CliCommands::version},
        {QStringLiteral("list-engines"), &CliCommands::listEngines},
        {QStringLiteral("validate-sfen"), &CliCommands::validateSfen},
        {QStringLiteral("convert-kifu"), &CliCommands::convertKifu},
        {QStringLiteral("render-board"), &CliCommands::renderBoard},
        {QStringLiteral("analyze"), &CliCommands::analyze},
        {QStringLiteral("mate"), &CliCommands::mate},
        {QStringLiteral("generate-tsume"), &CliCommands::generateTsume},
        {QStringLiteral("verify-tsume"), &CliCommands::verifyTsume},
    };

    if (arguments.size() < 2 || arguments.at(1) == QLatin1String("--help") || arguments.at(1) == QLatin1String("-h")
        || arguments.at(1) == QLatin1String("help")) {
        CliOutput::printUsage();
        return arguments.size() < 2 ? 2 : 0;
    }
    const QString& command = arguments.at(1);
    const auto it = handlers.constFind(command);
    if (it == handlers.constEnd()) {
        return CliOutput::fail(QStringLiteral("unknown_command"),
                               QStringLiteral("Unknown command \"%1\". Run without arguments for usage.").arg(command));
    }
    return it.value()(arguments.mid(2));
}

int main(int argc, char* argv[])
{
    // 画面が無い環境でも盤面描画（QWidget::grab）ができるように offscreen を既定にする
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    QApplication app(argc, argv);
    // 設定ファイル（登録エンジン・盤面配色・駒画像）の場所を GUI と揃える
    app.setApplicationName(QStringLiteral("ShogiBoardQ"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    return CliCommands::run(app.arguments());
}
