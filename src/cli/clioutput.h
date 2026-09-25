#ifndef CLIOUTPUT_H
#define CLIOUTPUT_H

/// @file clioutput.h
/// @brief shogiboardq-cli の JSON 出力ヘルパ

#include <QJsonObject>
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

/// `{"ok":false,"error":{"code","message"}}` を書き、終了コード 1 を返す
int fail(const QString& code, const QString& message);

/// `{"event":"error","code","message"}` を書き、終了コード 1 を返す
int failEvent(const QString& code, const QString& message);

/// 使い方をテキストで標準出力に書く
void printUsage();

} // namespace CliOutput

#endif // CLIOUTPUT_H
