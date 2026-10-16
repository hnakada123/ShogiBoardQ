#ifndef CLIARGS_H
#define CLIARGS_H

/// @file cliargs.h
/// @brief shogiboardq-cli のコマンド共通の引数解釈（局面・エンジン・数値）

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

#include "enginelistsettings.h"
#include "usiposition.h"

/// 引数の sfen・moves から組み立てた局面
struct CliPosition {
    QString startSfen;        ///< 正規化した開始局面（startpos は平手 SFEN に展開）
    QStringList moves;        ///< USI 手順
    UsiPosition position;     ///< エンジンに探索させる局面
    QString finalSfen;        ///< 手順適用後の SFEN
};

namespace CliArgs {

/**
 * @brief 表（CliCommandSpecs）のコマンド command の引数を読み、検査して既定値を補う
 *
 * 失敗したら invalid_argument を書き（events なら JSON Lines のイベントで）、*exitCode に終了コードを入れる。
 */
std::optional<QJsonObject> parse(const QString& command, const QStringList& args, bool events, int* exitCode);

/// 引数の "sfen"・"moves"（CliCommandSpecs::parse で読んだ値）から局面を解決する。失敗時は nullopt と error
std::optional<CliPosition> resolvePosition(const QJsonObject& params, QString* error);
/// 引数の "engine" の登録エンジンを解決する。失敗時は nullopt と error
std::optional<EngineListSettings::EngineEntry> resolveEngine(const QJsonObject& params, QString* error);

/// 出力ファイルの事前検査（絶対パス、既存ファイルは --overwrite 必須、親ディレクトリの存在）
bool checkOutputPath(const QString& path, bool overwrite, QString* error);

} // namespace CliArgs

#endif // CLIARGS_H
