#ifndef TEST_REPLAY_H
#define TEST_REPLAY_H

/// @file test_replay.h
/// @brief テスト用: 開始局面と USI の指し手から局面列・指し手列を作る（SfenReplay::replay の結果を並べるだけ）

#include <QList>
#include <QStringList>

#include "sfenreplay.h"

namespace TestReplay {

/// 開始局面と各手の後の局面（hasTerminal のとき終局の行のために最後を複製する）。指せなければ空
inline QStringList buildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal)
{
    const SfenReplay::Result result = SfenReplay::replay(initialSfen, usiMoves);
    if (!result.success()) return {};
    QStringList sfens{result.initialSfen};
    for (const SfenReplay::Step& step : result.moves) sfens.append(step.sfen);
    if (hasTerminal) sfens.append(sfens.last());
    return sfens;
}

/// 各手（動かした駒・取った駒つき）。指せない手があれば空
inline QList<ShogiMove> buildGameMoves(const QString& initialSfen, const QStringList& usiMoves)
{
    const SfenReplay::Result result = SfenReplay::replay(initialSfen, usiMoves);
    if (!result.success()) return {};
    QList<ShogiMove> moves;
    for (const SfenReplay::Step& step : result.moves) moves.append(step.move);
    return moves;
}

} // namespace TestReplay

#endif // TEST_REPLAY_H
