#ifndef DIALOGUTILS_H
#define DIALOGUTILS_H

#include <QSize>
#include <QWidget>
#include <functional>

namespace DialogUtils {

/// 保存済みサイズを復元する。savedSize が無効または小さすぎる場合は何もしない。
void restoreDialogSize(QWidget* dialog, const QSize& savedSize);

/// 現在のウィジェットサイズを保存する。
void saveDialogSize(const QWidget* dialog, const std::function<void(const QSize&)>& setter);

/// ウィジェットと全子ウィジェットにフォントを適用する。
/// KDE Breeze テーマでは setFont() が子ウィジェットに伝播しないため、
/// 明示的に全子ウィジェットにフォントを設定する。
void applyFontToAllChildren(QWidget* widget, const QFont& font);

/// 文字サイズボタンの寸法・説明・上下限を統一する（子の別ウィンドウは対象外）。
void updateFontButtons(QWidget* widget, int size, int minimum = 8, int maximum = 24);

/// 余白、ボタンの最小高さ、主操作・補助操作の表現を統一する。
void standardizeDialog(QWidget* dialog);

/// 折り返しラベルの必要な高さを確保し、内容の重なりを防ぐ。
void fitWrappedLabels(QWidget* dialog);

} // namespace DialogUtils

#endif // DIALOGUTILS_H
