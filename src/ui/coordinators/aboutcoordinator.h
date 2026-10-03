#ifndef ABOUTCOORDINATOR_H
#define ABOUTCOORDINATOR_H

/// @file aboutcoordinator.h
/// @brief バージョン情報コーディネータクラスの定義


class QWidget;

namespace AboutCoordinator {

// 「バージョン情報」ダイアログを表示します。
void showVersionDialog(QWidget* parent);

// プロジェクトWebサイトを既定ブラウザで開きます。
void openProjectWebsite();

} // namespace AboutCoordinator

#endif // ABOUTCOORDINATOR_H
