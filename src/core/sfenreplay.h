#ifndef SFENREPLAY_H
#define SFENREPLAY_H

/// @file sfenreplay.h
/// @brief 開始局面から USI の手を順に指して局面列を作る

#include <QList>
#include <QString>
#include <QStringList>

#include "shogimove.h"

class ShogiBoard;

/**
 * @brief 開始局面から USI の手を順に指す（合法手かは確かめない）
 *
 * 局面は盤（ShogiBoard）で持つ。棋譜の読み込みに使うので、持ち駒の数が合わない駒打ちも指す。
 */
namespace SfenReplay {

struct Step {
    ShogiMove move;      ///< 指した手（動かした駒・取った駒つき）
    QString beforeSfen;  ///< 指す前の局面
    QString sfen;        ///< 指した後の局面
};

struct Result {
    QList<Step> moves;
    QString initialSfen;
    int failedPly = -1;  ///< -1 は成功、0 は開始局面、1以上は指せなかった手数
    QString error;
    bool success() const { return failedPly < 0; }
};

/// USI の1手を手番側の手として指す。移動元が空・USI として読めない手は false。
/// 打つ駒が駒台に無くても、駒のあるマスでも打つ（棋譜の読み込みでは、持ち駒や盤が合わなくても続ける）
bool applyLenient(ShogiBoard& board, const QString& usiMove);

/// 局面から USI の1手を指した後の局面。指せなければ空
QString positionAfter(const QString& sfen, const QString& usiMove);

/// 開始局面から手を順に指し、各手と前後の局面をまとめて返す
Result replay(const QString& initialSfen, const QStringList& usiMoves);

/// USI の position コマンド（"position startpos moves ..." / "position sfen ... moves ..."）の局面。
/// 読めなければ空
QString sfenFromPositionCommand(const QString& positionCommand);

} // namespace SfenReplay

#endif // SFENREPLAY_H
