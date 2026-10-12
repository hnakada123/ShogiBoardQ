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

/// QApplication を作った後に呼び、QPixmapCache の上限を駒画像が収まる大きさまで広げる。
///
/// 盤の駒は PiecePainter が駒種ごとに「元の SVG 画像」と「影付きの描画済み画像」の2枚を
/// QPixmapCache に預ける（QApplication の破棄時に Qt がまとめて消すため）。Qt の既定の上限は
/// 10MB で、2倍表示でマスが 110〜150px になると 16〜28駒種で溢れ、駒を描くたびに SVG の解析と
/// 影の描画をやり直していた（全面の描き直しが 11ms → 63〜138ms）。2倍・150px・28駒種で約28MB、
/// 読み筋ウィンドウなど別の大きさの盤の分と Fusion の部品の分を合わせて 64MB あれば足りる。
/// 使った分だけ確保されるので、小さな盤では増えない。利用者やテストが広げた値は縮めない。
void reservePixmapCacheForPieces();

/// reservePixmapCacheForPieces が確保する QPixmapCache の上限（KB）
inline constexpr int kPixmapCacheLimitKb = 64 * 1024;

} // namespace ApplicationRendering

#endif // APPLICATIONRENDERING_H
