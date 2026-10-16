#ifndef BOARDINPUTROUTE_H
#define BOARDINPUTROUTE_H

/// @file boardinputroute.h
/// @brief 盤で指した手の行き先（動作状態ごとに、受け付けるか・どう処理するかを決める）

#include <QPoint>

/**
 * @brief 盤の操作（BoardInteractionController）が、つまむ・指すたびに問い合わせる行き先
 *
 * 主画面では動作状態（局面編集・変化の記録・対局・通信対局）ごとに行き先が決まり、
 * 詰将棋を解くダイアログでは解答の受け付けが行き先になる。
 * 盤の操作はクリックを移動元・移動先の候補にすることとハイライトだけを受け持ち、
 * 手番の判定・成りの選択・着手は行き先が行う。
 */
class BoardInputRoute
{
public:
    virtual ~BoardInputRoute() = default;

    /// 盤の駒をつまめるか（人間が指せる手番か。局面編集中は常に true）
    virtual bool acceptsInput() const = 0;
    /// 局面編集中か（両方の駒・駒台・駒箱を動かせる。合法な移動先は出さず、右クリックで成り・不成を替える）
    virtual bool isEditing() const = 0;
    /// つまんだ駒の行き先が決まった。結果は BoardInteractionController::onMoveApplied で返す
    virtual void submit(const QPoint& from, const QPoint& to) = 0;
};

#endif // BOARDINPUTROUTE_H
