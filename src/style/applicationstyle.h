#ifndef APPLICATIONSTYLE_H
#define APPLICATIONSTYLE_H

/// @file applicationstyle.h
/// @brief アプリ全体で使うスタイル（Fusion）

class QStyle;

namespace ApplicationStyle {

/// アプリ全体で使う Fusion スタイルを作る（呼び出し側が QApplication::setStyle で所有させる）。
///
/// Qt の Fusion スタイルは、ドックやタイトルのボタンのアイコンを頼まれるたびに、内蔵の画像6枚を
/// 読み込んで作る（Qt 6.11 の QFusionStyle::iconFromTheme。使い回す仕組みがない）。ドックの
/// 描き直しや配置の計算のたびに呼ばれて重いので、Fusion を包んで、作ったアイコンを使い回す。
/// これらのアイコンは内蔵の画像だけから作られ、部品や設定によらないので、見た目は変わらない。
///
/// 表や一覧の項目を描くとき、Qt は各部（チェック・絵・文字）の位置を求めるたびに文字全体の配置を
/// 計算する。絵を文字の左右に置く配置では位置は文字の大きさによらないので、位置を決める値ごとに
/// Fusion が求めた位置を覚えて使い回す。
QStyle* create();

} // namespace ApplicationStyle

#endif // APPLICATIONSTYLE_H
