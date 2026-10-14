#ifndef DIALOGUTILS_H
#define DIALOGUTILS_H

#include <QFont>
#include <QList>
#include <QMessageBox>
#include <QSize>
#include <QWidget>
#include <functional>

#include "setting.h"

namespace DialogUtils {

/// 保存済みサイズを復元する。savedSize が無効または小さすぎる場合は何もしない。
void restoreDialogSize(QWidget* dialog, const QSize& savedSize);

/// 設定に保存したサイズを復元する（ダイアログの大きさは設定の宣言の表に1行で書く）
inline void restoreDialogSize(QWidget* dialog, const Setting<QSize>& setting)
{
    restoreDialogSize(dialog, setting());
}

/// 現在のウィジェットサイズを保存する。
void saveDialogSize(const QWidget* dialog, const std::function<void(const QSize&)>& setter);

/// 現在のウィジェットサイズを設定に保存する
inline void saveDialogSize(const QWidget* dialog, const Setting<QSize>& setting)
{
    saveDialogSize(dialog, [&setting](const QSize& size) { setting.set(size); });
}

/// ウィジェットと全子ウィジェットにフォントを適用する。
/// KDE Breeze テーマでは setFont() が子ウィジェットに伝播しないため、
/// 明示的に全子ウィジェットにフォントを設定する。ダイアログが持つメニューにも適用する。
/// 盤面など自前で文字の大きさを決めるウィジェットは excluded に渡すと、その部分木ごと対象外にする。
void applyFontToAllChildren(QWidget* widget, const QFont& font, const QList<QWidget*>& excluded = {});

/// ダイアログの文字サイズを一括で適用する（A-/A+ の共通処理）。
/// 余白・ボタンの標準化、全子ウィジェットへのフォント設定、A-/A+ ボタンの有効状態の更新を行い、
/// 適用したフォントを返す。ダイアログ固有の調整（太字の見出しなど）は戻り値のフォントから作る。
QFont applyFontSize(QWidget* dialog, int size, int minimum = 8, int maximum = 24,
                    const QList<QWidget*>& excluded = {});

/// 文字サイズボタンの寸法・説明・上下限を統一する（子の別ウィンドウは対象外）。
void updateFontButtons(QWidget* widget, int size, int minimum = 8, int maximum = 24);

/// 余白、ボタンの最小高さ、主操作・補助操作の表現を統一する。
void standardizeDialog(QWidget* dialog);

/// 折り返しラベルの必要な高さを確保し、内容の重なりを防ぐ。
void fitWrappedLabels(QWidget* dialog);

/// 確認を、実行する操作名のボタンと「キャンセル」で尋ねる。操作を選んだときだけ true を返す。
/// KDE（KF6）では Yes/No の訳がなく、日本語の画面でも英語のまま表示されるため使わない。
/// 操作のボタンは QMessageBox::Yes、既定のボタンと Esc はキャンセル。
bool confirmAction(QWidget* parent, const QString& title, const QString& text,
                   const QString& actionText, QMessageBox::Icon icon = QMessageBox::Question);

} // namespace DialogUtils

#endif // DIALOGUTILS_H
