#ifndef AUTOMATIONWIDGETADAPTERS_H
#define AUTOMATIONWIDGETADAPTERS_H

/// @file automationwidgetadapters.h
/// @brief 自動化 API のウィジェットの読み書き（型ごとのアダプタの表）

#include <QJsonObject>
#include <functional>

class QWidget;

/**
 * @brief ウィジェットの型ごとの読み（widget.text）と書き（widget.setValue）を1つの表で持つ
 *
 * 表の行は型・読み・書きの組で、上の行から順に当てはめる（派生クラスの行を基底クラスより先に置く）。
 * どの型が読めて書けるかは表を見れば分かる。型を足すときは表に1行足す。
 */
namespace AutomationWidgetAdapters {

/// 読める型なら obj に中身（text・value・items など）を書いて true
bool describe(QWidget* widget, int maxRows, QJsonObject& obj);

/**
 * @brief widget に params の value を書く操作を作る
 *
 * 型・値・範囲は今確かめ、合わなければ AutomationError を投げる。返した操作は応答の後に実行し、
 * そのときに書けなくなっていれば（読み取り専用になった・項目が隠れたなど）AutomationError を投げる。
 */
std::function<void()> writer(QWidget* widget, const QJsonObject& params);

} // namespace AutomationWidgetAdapters

#endif // AUTOMATIONWIDGETADAPTERS_H
