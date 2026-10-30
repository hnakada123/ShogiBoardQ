#ifndef SFENREPLAY_H
#define SFENREPLAY_H

/// @file sfenreplay.h
/// @brief 局面に USI の1手を指す（棋譜の読み込みと同じく、合法手かは確かめない）

#include <QList>
#include <QString>
#include <QStringList>

#include "shogiposition.h"
#include "shogimove.h"

/**
 * @brief 開始局面から USI の手を順に指す（合法手かは確かめない）
 *
 * 局面は値（Position）で持つ。棋譜の読み込みに使うので、持ち駒の数が合わない駒打ちも指す（Position::DropRule::Lenient）。
 */
namespace SfenReplay {

/// 局面から USI の1手を手番側の手として指した後の局面。移動元が空・USI として読めない手は指せず空。
/// 打つ駒が駒台に無くても、駒のあるマスでも打つ（棋譜の読み込みでは、持ち駒や盤が合わなくても続ける）
QString positionAfter(const QString& sfen, const QString& usiMove);

} // namespace SfenReplay

#endif // SFENREPLAY_H
