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

/// 大きさの設定を戻し、閉じた（Hide の）ときに保存する。文字の大きさを持たない窓のため。
/// A-/A+ のあるダイアログは DialogFontScale の options.size に渡す
void rememberDialogSize(QDialog* dialog, const Setting<QSize>& setting);

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

/// 余白、ボタンの最小高さ、主操作・補助操作の表現を統一する。
void standardizeDialog(QWidget* dialog);

/// 折り返しラベルの必要な高さを確保し、内容の重なりを防ぐ。
void fitWrappedLabels(QWidget* dialog);

/// 確認を、実行する操作名のボタンと「キャンセル」で尋ねる。操作を選んだときだけ true を返す。
/// KDE（KF6）では Yes/No の訳がなく、日本語の画面でも英語のまま表示されるため使わない。
/// 操作のボタンは QMessageBox::Yes、既定のボタンと Esc はキャンセル。
bool confirmAction(QWidget* parent, const QString& title, const QString& text,
                   const QString& actionText, QMessageBox::Icon icon = QMessageBox::Question);

/// confirmAction と同じ問いの箱を、待たずに出すために作る（閉じたら消える）。出すのは呼ぶ側（show）で、
/// 答えは finished で受ける（操作を選べば QMessageBox::Yes、キャンセル・閉じるは QMessageBox::Cancel）
QMessageBox* createConfirmBox(QWidget* parent, const QString& title, const QString& text,
                              const QString& actionText, QMessageBox::Icon icon = QMessageBox::Question);

/// 知らせのメッセージボックス（dialogs・widgets 用。ui・app は NotificationPresenter を使う）。
/// QMessageBox の静的関数は文を自動判定し、外から来た文字列が HTML として読まれるので、文は PlainText で出す
void warning(QWidget* parent, const QString& title, const QString& text);
void critical(QWidget* parent, const QString& title, const QString& text);
void information(QWidget* parent, const QString& title, const QString& text);

/// 上の知らせと同じ箱を、待たずに出すために作る（PlainText・モードレス・閉じたら消える）。出すのは呼ぶ側（show）。
/// 外から届く知らせ（通信プロセスの通知など）の処理の中で出すときに使い、入れ子のイベントループに入らない
QMessageBox* createNoticeBox(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text);

} // namespace DialogUtils

#endif // DIALOGUTILS_H
