#ifndef CLICOMMANDS_H
#define CLICOMMANDS_H

/// @file clicommands.h
/// @brief shogiboardq-cli の各コマンド

#include <QJsonObject>
#include <QStringList>

/**
 * @brief コマンドの実装。引数は表（CliCommandSpecs）で検査して既定値を補った値
 *
 * 戻り値はプロセスの終了コード（成功 0、失敗 1、使い方の誤り 2）。
 * コマンドの名前・実装・出力の種類（JSON か JSON Lines のイベントか）は run() の表の1行で決める。
 */
namespace CliCommands {

/// 引数全体（プログラム名を含む）からコマンドを選び、引数を表で検査してから実行する
int run(const QStringList& arguments);

int version(const QJsonObject& params);
int describe(const QJsonObject& params);
int environment(const QJsonObject& params);
int listEngines(const QJsonObject& params);
int validateSfen(const QJsonObject& params);
int convertKifu(const QJsonObject& params);
int renderBoard(const QJsonObject& params);
int analyze(const QJsonObject& params);
int analyzeKifu(const QJsonObject& params);
int mate(const QJsonObject& params);
int generateTsume(const QJsonObject& params);
int verifyTsume(const QJsonObject& params);

} // namespace CliCommands

#endif // CLICOMMANDS_H
