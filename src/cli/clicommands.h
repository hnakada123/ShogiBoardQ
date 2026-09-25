#ifndef CLICOMMANDS_H
#define CLICOMMANDS_H

/// @file clicommands.h
/// @brief shogiboardq-cli の各コマンド

#include <QStringList>

/**
 * @brief コマンドの実装。引数はコマンド名を除いた残り
 *
 * 戻り値はプロセスの終了コード（成功 0、失敗 1、使い方の誤り 2）。
 */
namespace CliCommands {

/// 引数全体（プログラム名を含む）からコマンドを選んで実行する
int run(const QStringList& arguments);

int version(const QStringList& args);
int listEngines(const QStringList& args);
int validateSfen(const QStringList& args);
int convertKifu(const QStringList& args);
int renderBoard(const QStringList& args);
int analyze(const QStringList& args);
int mate(const QStringList& args);
int generateTsume(const QStringList& args);
int verifyTsume(const QStringList& args);

} // namespace CliCommands

#endif // CLICOMMANDS_H
