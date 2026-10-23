#ifndef TEST_REPLAY_H
#define TEST_REPLAY_H

/// @file test_replay.h
/// @brief テスト用: 開始局面と USI の指し手から局面列・指し手列を作る（棋譜の読み込みと同じく Position で指す）

#include <QList>
#include <QStringList>
#include <optional>

#include "recordline.h"
#include "shogimove.h"
#include "shogiposition.h"
#include "usimove.h"

namespace TestReplay {

/// 開始局面から手を順に指す（持ち駒が合わない駒打ちも指す）。指せない手があれば nullopt
inline std::optional<QList<std::pair<ShogiMove, QString>>> play(const QString& initialSfen, const QStringList& usiMoves)
{
    std::optional<Position> position = Position::fromSfen(initialSfen);
    if (!position) return std::nullopt;
    QList<std::pair<ShogiMove, QString>> steps;
    for (const QString& text : usiMoves) {
        const auto move = UsiMove::parse(text);
        if (!move) return std::nullopt;
        const ShogiMove described = position->describe(*move);
        if (described.movingPiece == Piece::None || !position->apply(*move, Position::DropRule::Lenient))
            return std::nullopt;
        steps.append({described, position->toSfen()});
    }
    return steps;
}

/// 開始局面と各手の後の局面（hasTerminal のとき終局の行のために最後を複製する）。指せなければ空
inline QStringList buildSfenRecord(const QString& initialSfen, const QStringList& usiMoves, bool hasTerminal)
{
    const auto steps = play(initialSfen, usiMoves);
    if (!steps) return {};
    QStringList sfens{Position::fromSfen(initialSfen)->toSfen()};
    for (const auto& step : *steps) sfens.append(step.second);
    if (hasTerminal) sfens.append(sfens.last());
    return sfens;
}

/// 各手（動かした駒・取った駒つき）。指せない手があれば空
inline QList<ShogiMove> buildGameMoves(const QString& initialSfen, const QStringList& usiMoves)
{
    const auto steps = play(initialSfen, usiMoves);
    if (!steps) return {};
    QList<ShogiMove> moves;
    for (const auto& step : *steps) moves.append(step.first);
    return moves;
}

/// 局面列（[0] が開始局面、[i] が i 手目の後）と指し手列から手順を作る。
/// 指し手が局面より多ければ、局面のある手までにする
inline RecordLine recordLine(const QStringList& sfens, const QList<ShogiMove>& moves)
{
    RecordLine line;
    line.startSfen = sfens.value(0);
    const qsizetype count = qMin(moves.size(), sfens.size() - 1);
    for (qsizetype i = 0; i < count; ++i)
        line.entries << RecordEntry::played(moves.at(i), {}, sfens.at(i + 1));
    line.currentPly = line.moveCount();
    return line;
}

} // namespace TestReplay

#endif // TEST_REPLAY_H
