#ifndef OPAQUEBACKGROUND_H
#define OPAQUEBACKGROUND_H

/// @file opaquebackground.h
/// @brief ドックの中身などを、親と同じ背景色で塗る不透明なウィジェットにする

#include <QBrush>
#include <optional>

class QAbstractScrollArea;
class QWidget;

/// 背景を塗らない（不透明でない）ウィジェットを描き直すと、Qt はその下にある
/// 祖先まで描き直す。ドックの中の表を更新するたびにドックのタイトル（Fusion の
/// アイコンの読み込みを含む）やメインウィンドウまで描き直されるのを防ぐため、
/// 中身に親と同じ色で背景を塗らせ、不透明として扱わせる。
namespace OpaqueBackground {

/// widget に自分の背景色（パレットの背景の役割の色）で塗らせる。
/// メインウィンドウかダイアログに収まっていて、親とそのウィンドウの背景と同じ色のときだけ塗るので、
/// 見た目は変わらない（ドックを切り離している間や、違う色のときは何もしない）。
/// スタイルシートの適用・パレットの変更・ドックの切り離しと戻しのたびに判断し直す。
void keep(QWidget* widget);

/// area のスクロールバーの入れ物（QAbstractScrollArea が作る、スクロールバーの親）を brush で塗って
/// 不透明にする（nullopt なら塗るのをやめる）。スクロールバーも入れ物も不透明でないので、そのままでは
/// スクロールバーを描き直すたびに、スクロール領域と、その下にある親まで同じ範囲を描き直す。
/// 入れ物のパレットは変えない（変えると、中のスクロールバーの縁の色まで変わる。Fusion はウィンドウの
/// 背景の役割の色から縁の色を作る）。brush は、入れ物の下に見えている色にする。
void fillScrollBarContainers(QAbstractScrollArea* area, const std::optional<QBrush>& brush);

/// area のスクロールバーの入れ物を、表示領域の背景色（スクロールバーの下に見えている色）で塗らせ続ける。
/// 整数倍率で、背景が不透明な色のときだけ塗る（小数倍率では入れ物の端が物理画素の途中に来て、塗ると
/// 隣の枠線の画素が変わる）。スタイルの適用・パレットの変更・表示・倍率の変更のたびに判断し直す。
/// スタイルシートで背景を付けていないスクロール領域（分岐ツリーの QGraphicsView）に使う。
void keepScrollBars(QAbstractScrollArea* area);

} // namespace OpaqueBackground

#endif // OPAQUEBACKGROUND_H
