#ifndef APPLICATIONRENDERING_H
#define APPLICATIONRENDERING_H

/// @file applicationrendering.h
/// @brief アプリ全体の描画の設定

namespace ApplicationRendering {

/// QApplication を作る前に呼び、Qt の描画を GUI のスレッドだけで行わせる。
///
/// Qt は1回の塗りの区間（行ごとの横の並び）が96以上あると、塗りを別のスレッドに分けて頼み、
/// 終わるまで待つ（QT_THREAD_PARALLEL_FILLS。画像の拡大・縮小や形式の変換も同じスレッドを使う）。
/// 持駒をつまむときのハイライトの枠や、表のスクロールバーのような中くらいの塗りでは、分けた
/// 仕事の受け渡しのほうが重い。Qt は初めてそのスレッドを使うときに、環境変数
/// QT_NO_GUI_THREADPOOL があるかを1回だけ見て、あれば分けずに塗る（Qt 6.12 の
/// QGuiApplicationPrivate::qtGuiThreadPool）。同じ処理を分けずに行うだけなので、描いた画素は
/// 変わらない。利用者が指定した値はそのままにする。
void paintOnGuiThreadOnly();

} // namespace ApplicationRendering

#endif // APPLICATIONRENDERING_H
