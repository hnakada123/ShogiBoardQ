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

/// ドックのウィンドウ（定跡ウィンドウなど）の文字サイズを一括で適用する。
/// 余白・ボタンの標準化と全子ウィジェットへのフォント設定を行い、適用したフォントを返す。
/// ダイアログは DialogFontScale（元の大きさからの差で拡大する）を使う。A-/A+ のボタンは FontSizeControl が受け持つ
QFont applyFontSize(QWidget* widget, int size, const QList<QWidget*>& excluded = {});

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
