#ifndef TSUMEPOSITIONEDITS_H
#define TSUMEPOSITIONEDITS_H

/// @file tsumepositionedits.h
/// @brief 詰将棋の候補局面の盤面編集と余詰リペア（純粋局面演算）

#include "tsumeedittypes.h"

#include <QList>
#include <QString>

#include <random>

/// いまある攻方手番の局面を少し変えて、別の候補局面を作る操作。
/// 手を戻して作る操作（逆算）は TsumeRetrogradeOps。
namespace TsumePositionEdits {

using Limits = TsumeEditBoard::Limits;
using Candidate = TsumeEditBoard::Candidate;

/// 攻方手番の局面に対して駒の移動・追加・削除・駒種変更・成不成・持駒調整を行う。
QList<Candidate> generateMutations(const QString& sfen, const Limits& limits,
                                   int maxCandidates, std::mt19937_64& rng);

/// 余詰の攻方初手 cookMove を阻止し、主手順 mainMove を活かす標的型リペア候補を生成する。
QList<Candidate> generateCookRepairs(const QString& sfen, const QString& mainMove,
                                     const QString& cookMove, const Limits& limits,
                                     int maxCandidates, std::mt19937_64& rng);

} // namespace TsumePositionEdits

#endif // TSUMEPOSITIONEDITS_H
