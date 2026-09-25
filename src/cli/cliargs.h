#ifndef CLIARGS_H
#define CLIARGS_H

/// @file cliargs.h
/// @brief shogiboardq-cli のコマンド共通の引数解釈（局面・エンジン・数値）

#include <QCommandLineParser>
#include <QString>
#include <QStringList>
#include <optional>

#include "enginelistsettings.h"

/// `--sfen` / `--moves` から組み立てた局面
struct CliPosition {
    QString startSfen;        ///< 正規化した開始局面（startpos は平手 SFEN に展開）
    QStringList moves;        ///< USI 手順
    QString positionCommand;  ///< エンジンに送る "position ..." 文字列
    QString finalSfen;        ///< 手順適用後の SFEN
};

namespace CliArgs {

/// `--sfen`, `--moves` を追加する
void addPositionOptions(QCommandLineParser& parser);
/// `--engine` を追加する
void addEngineOption(QCommandLineParser& parser);
/// `--stdin-control` を追加する
void addStdinControlOption(QCommandLineParser& parser);

/// 局面を解決する。失敗時は nullopt と error
std::optional<CliPosition> resolvePosition(const QCommandLineParser& parser, QString* error);
/// 登録エンジンを解決する。失敗時は nullopt と error
std::optional<EngineListSettings::EngineEntry> resolveEngine(const QCommandLineParser& parser, QString* error);

/// 整数オプション（範囲外・非数値は error）
std::optional<int> intOption(const QCommandLineParser& parser, const QString& name, int defaultValue,
                             int minimum, int maximum, QString* error);

/// 出力ファイルの事前検査（絶対パス、既存ファイルは --overwrite 必須、親ディレクトリの存在）
bool checkOutputPath(const QString& path, bool overwrite, QString* error);

} // namespace CliArgs

#endif // CLIARGS_H
