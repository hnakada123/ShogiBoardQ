#ifndef AUTOMATIONWIDGETS_H
#define AUTOMATIONWIDGETS_H

/// @file automationwidgets.h
/// @brief 自動化 API のウィンドウ列挙・ウィジェット内容取得ヘルパ

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

class QWidget;

namespace AutomationWidgets {

/// 表示中のトップレベルウィンドウ（メインウィンドウ・ダイアログ・メッセージボックス）
QList<QWidget*> visibleWindows();

/// objectName の完全一致、次にウィンドウタイトルの部分一致で探す。無ければ nullptr
QWidget* findWindow(const QString& nameOrTitle);

/// ウィンドウの概要（object_name, class, title, visible, modal, active, width, height）
QJsonObject describeWindow(const QWidget* window, bool isMain);

/// root 配下（objectName 指定時はその 1 つ）の読み取れるウィジェットを列挙する
QJsonArray describeWidgets(QWidget* root, const QString& objectName, int maxRows, int maxWidgets = 300);

} // namespace AutomationWidgets

#endif // AUTOMATIONWIDGETS_H
