/// @file main.cpp
/// @brief shogiboardq-cli のエントリーポイント
///
/// GUI を起動せずに棋譜変換・SFEN 検証・エンジン解析・詰将棋生成などを実行する。
/// MCP サーバー（mcp/shogiboardq_mcp）から呼ばれることを想定し、標準出力は JSON 専用とする。

#include "clicommands.h"
#include "clicommandspecs.h"
#include "clioutput.h"
#include "engineprofilestore.h"

#include <QApplication>
#include <QStringList>

#include <optional>

namespace {
/// 出力の種類（失敗も同じ種類で書く）
enum class Output {
    Json,     ///< 結果を JSON 1つで書く
    Events,   ///< 進み具合と結果を JSON Lines のイベントで書く（長い処理。--stdin-control で止められる）
};

/// コマンド1つ（名前・実装・出力の種類）。引数の表は同じ名前の CliCommandSpecs
struct Command {
    const char* name;
    int (*run)(const QJsonObject& params);
    Output output;
};

const Command kCommands[] = {
    {"version", &CliCommands::version, Output::Json},
    {"describe", &CliCommands::describe, Output::Json},
    {"environment", &CliCommands::environment, Output::Json},
    {"list-engines", &CliCommands::listEngines, Output::Json},
    {"validate-sfen", &CliCommands::validateSfen, Output::Json},
    {"convert-kifu", &CliCommands::convertKifu, Output::Json},
    {"render-board", &CliCommands::renderBoard, Output::Json},
    {"analyze", &CliCommands::analyze, Output::Events},
    {"analyze-kifu", &CliCommands::analyzeKifu, Output::Events},
    {"mate", &CliCommands::mate, Output::Events},
    {"generate-tsume", &CliCommands::generateTsume, Output::Events},
    {"verify-tsume", &CliCommands::verifyTsume, Output::Json},
};
} // namespace

int CliCommands::run(const QStringList& arguments)
{
    if (arguments.size() < 2 || arguments.at(1) == QLatin1String("--help") || arguments.at(1) == QLatin1String("-h")
        || arguments.at(1) == QLatin1String("help")) {
        CliOutput::printUsage();
        return arguments.size() < 2 ? 2 : 0;
    }
    const QString& name = arguments.at(1);
    const Command* command = nullptr;
    for (const Command& candidate : kCommands)
        if (name == QLatin1String(candidate.name)) command = &candidate;
    // 表どうしがずれていても（実装か引数の表の片方だけにある）、知らないコマンドとして扱う
    const OperationSpec* spec = CliCommandSpecs::find(name);
    if (!command || !spec) {
        return CliOutput::fail(ErrorKind::UnknownOperation,
                               QStringLiteral("Unknown command \"%1\". Run without arguments for usage.").arg(name));
    }
    // 引数は表で検査して既定値を補う。使い方の誤りもコマンドの出力の種類で書く
    QString error;
    const std::optional<QJsonObject> params = CliCommandSpecs::parse(*spec, arguments.mid(2), &error);
    if (!params) {
        return command->output == Output::Events ? CliOutput::failEvent(ErrorKind::InvalidArgument, error)
                                                 : CliOutput::fail(ErrorKind::InvalidArgument, error);
    }
    return command->run(*params);
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
    // 以前の版の設定ファイルにあるエンジンごとの設定を、今の保存先へ移す（GUI と同じ）
    EngineProfileStore::migrateLegacyOptionGroups();

    return CliCommands::run(app.arguments());
}
