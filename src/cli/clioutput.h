#ifndef CLIOUTPUT_H
#define CLIOUTPUT_H

/// @file clioutput.h
/// @brief shogiboardq-cli の JSON 出力ヘルパ

#include <QJsonObject>
#include "errorkinds.h"
#include <QString>

/**
 * @brief 標準出力へ JSON（1 文書）または JSON Lines（イベント）を書く
 *
 * 標準出力は機械可読な JSON 専用とし、ログは標準エラーに出す。
 */
namespace CliOutput {


/// 1 行の JSON を書いてフラッシュする
void printJson(const QJsonObject& object);

/// `{"event": name, ...fields}` を 1 行書く
void printEvent(const QString& name, QJsonObject fields = QJsonObject());

/// `{"ok":false,"error":{"code","message"}}` を書き、終了コード 1 を返す（code は分類の語）
int fail(ErrorKind kind, const QString& message);
/// サービスが返した分類つきの失敗を書く
int fail(const Failure& failure);

/// `{"event":"error","code","message"}` を書き、終了コード 1 を返す（code は分類の語）
int failEvent(ErrorKind kind, const QString& message);

/// 使い方（CliCommandSpecs の表から作る）をテキストで標準出力に書く
void printUsage();

} // namespace CliOutput

#endif // CLIOUTPUT_H
