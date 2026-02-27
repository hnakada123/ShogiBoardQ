#pragma once

#include <QSize>
#include <QWidget>
#include <functional>

namespace DialogUtils {

/// 保存済みサイズを復元する。savedSize が無効または小さすぎる場合は何もしない。
void restoreDialogSize(QWidget* dialog, const QSize& savedSize);

/// 現在のウィジェットサイズを保存する。
void saveDialogSize(const QWidget* dialog, const std::function<void(const QSize&)>& setter);

} // namespace DialogUtils
