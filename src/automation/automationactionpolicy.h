#ifndef AUTOMATIONACTIONPOLICY_H
#define AUTOMATIONACTIONPOLICY_H

/// @file automationactionpolicy.h
/// @brief 自動化 API から実行してよい QAction の許可リスト

#include <QString>
#include <QStringList>

/**
 * @brief `action.trigger` の許可リスト
 *
 * 終了・上書き保存・言語切替・外部サイトを開く・ドックレイアウト保存など、
 * ユーザーの意図なしに実行すべきでない動作は含めない。
 */
namespace AutomationActionPolicy {

/// 許可された QAction の objectName（mainwindow.ui の定義順）
const QStringList& allowedActions();

bool isAllowed(const QString& objectName);

} // namespace AutomationActionPolicy

#endif // AUTOMATIONACTIONPOLICY_H
