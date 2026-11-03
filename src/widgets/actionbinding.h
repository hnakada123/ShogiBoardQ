#ifndef ACTIONBINDING_H
#define ACTIONBINDING_H

/// @file actionbinding.h
/// @brief パネルのチェックと、正本の checkable な QAction の結び付け

class QAbstractButton;
class QAction;

/**
 * @brief パネルのチェックを、呼び出し側が持つ切り替えの QAction に結ぶ
 *
 * 切り替えの正本は QAction（盤の矢印は ui の BoardArrowActions、定跡の表示と盤の手の登録は
 * JosekiWindowController が作る）。チェックは QAction の今の値から始め、どちらを変えても
 * もう片方が合う。思考ドック・検討タブ・定跡ウィンドウのチェックが使う。
 */
namespace ActionBinding {

/// button（チェックできるボタン）を action（必須。checkable）に両方向に結ぶ
void bindCheckable(QAbstractButton* button, QAction* action);

} // namespace ActionBinding

#endif // ACTIONBINDING_H
