#ifndef CLICOMMANDSPECS_H
#define CLICOMMANDSPECS_H

/// @file clicommandspecs.h
/// @brief shogiboardq-cli のコマンドごとの引数の表と、コマンドラインから JSON への変換

#include "paramspec.h"

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <optional>

/**
 * @brief CLI の全コマンドの引数の表
 *
 * オプション名は引数名の "_" を "-" にしたもの（max_positions → --max-positions）。
 * オプションの登録・使い方の表示・値の変換と検査（範囲・既定値）をこの表から行う。
 * `describe` は表を JSON Schema として出す（MCP サーバーのスキーマとの一致は mcp/tests で検査する）。
 */
namespace CliCommandSpecs {

const QList<OperationSpec>& all();
/// 名前のコマンド（無ければ nullptr）
const OperationSpec* find(const QString& name);

/// 引数名に対応するオプション名（"--" を除く）
QString optionName(const QString& paramName);

/**
 * @brief コマンドの引数（コマンド名を除く）を表で読み、検査して既定値を補った JSON にする
 *
 * 真偽値はフラグ（指定すれば true）、文字列の配列は空白区切りの1つの値。失敗したら nullopt と error。
 */
std::optional<QJsonObject> parse(const OperationSpec& command, const QStringList& args, QString* error);

/// 使い方（全コマンドのオプションの一覧）
QString usage();

} // namespace CliCommandSpecs

#endif // CLICOMMANDSPECS_H
