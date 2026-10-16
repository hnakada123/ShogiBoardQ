#ifndef AUTOMATIONMETHODSPECS_H
#define AUTOMATIONMETHODSPECS_H

/// @file automationmethodspecs.h
/// @brief 自動化 API のメソッドごとの引数の表（GUI 非依存）

#include "paramspec.h"

#include <QList>

/**
 * @brief 自動化 API の全メソッドの引数の表
 *
 * AutomationDispatcher はメソッドを呼ぶ前にこの表で引数を検査し（知らない引数は拒否する）、
 * 既定値を補う。`shogiboardq-cli describe` が JSON Schema として出し、MCP サーバーのスキーマとの
 * 一致は mcp/tests で検査する。値の範囲が開いている画面の状態で決まるもの（表の行など）だけは
 * 各メソッドで検査する。
 */
namespace AutomationMethodSpecs {

const QList<OperationSpec>& all();
/// 名前のメソッド（無ければ nullptr）
const OperationSpec* find(const QString& name);

} // namespace AutomationMethodSpecs

#endif // AUTOMATIONMETHODSPECS_H
