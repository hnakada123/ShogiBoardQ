#ifndef OPAQUEBACKGROUND_H
#define OPAQUEBACKGROUND_H

/// @file opaquebackground.h
/// @brief ドックの中身などを、親と同じ背景色で塗る不透明なウィジェットにする

class QWidget;

/// 背景を塗らない（不透明でない）ウィジェットを描き直すと、Qt はその下にある
/// 祖先まで描き直す。ドックの中の表を更新するたびにドックのタイトル（Fusion の
/// アイコンの読み込みを含む）やメインウィンドウまで描き直されるのを防ぐため、
/// 中身に親と同じ色で背景を塗らせ、不透明として扱わせる。
namespace OpaqueBackground {

/// widget に自分の背景色（パレットの背景の役割の色）で塗らせる。
/// メインウィンドウに収まっていて、親とメインウィンドウの背景と同じ色のときだけ塗るので、
/// 見た目は変わらない（ドックを切り離している間や、違う色のときは何もしない）。
/// スタイルシートの適用・パレットの変更・ドックの切り離しと戻しのたびに判断し直す。
void keep(QWidget* widget);

} // namespace OpaqueBackground

#endif // OPAQUEBACKGROUND_H
