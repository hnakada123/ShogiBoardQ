#ifndef AUTOMATIONACTIONPOLICY_H
#define AUTOMATIONACTIONPOLICY_H

/// @file automationactionpolicy.h
/// @brief 自動化 API から実行してよい QAction の許可リスト

#include <QString>
#include <QStringList>

/**
 * @brief `action.trigger` の許可リスト
 *
 * 終了・上書き保存・外部サイトを開くなど、ユーザーの意図なしに実行すべきでない動作は含めない。
 * 言語の切り替え（再起動後に反映）とドックレイアウトの保存・リセットは含める。
 */
namespace AutomationActionPolicy {

/// 許可された QAction の objectName（mainwindow.ui の定義順）
const QStringList& allowedActions();

bool isAllowed(const QString& objectName);

} // namespace AutomationActionPolicy

#endif // AUTOMATIONACTIONPOLICY_H
