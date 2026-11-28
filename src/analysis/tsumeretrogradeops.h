#ifndef TSUMERETROGRADEOPS_H
#define TSUMERETROGRADEOPS_H

/// @file tsumeretrogradeops.h
/// @brief 詰み局面からの逆算による候補局面の生成（純粋局面演算）

#include "tsumeedittypes.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <random>

/// 詰み局面・詰む局面から手を戻して、詰将棋の候補局面を作る操作。盤の値と規則は TsumeEditBoard、
/// いまある局面を変える操作（盤面編集・余詰リペア）は TsumePositionEdits、手順の評価は TsumeLineMetrics。
namespace TsumeRetrogradeOps {

/// 作る局面の駒数と盤面構成の上限
using Limits = TsumeEditBoard::Limits;

/// 逆算で生成した候補局面
using Candidate = TsumeEditBoard::Candidate;

/// 目標手数超過PVから切り出した目標手数の部分局面
struct SuffixSlice {
    QString sfen;
    QStringList pv;
};

/// 攻方持駒ゼロ・後手玉詰み上がり（玉方手番・王手あり・合法手0）の初期局面を生成する。
/// 盤端・隅・中段寄りの多様な収束形を作り、1手以上逆算可能なものだけを返す。
QStringList generateMateEndings(const Limits& limits, int count, std::mt19937_64& rng);

/// 0手詰み上がり局面（玉方手番・王手あり・合法手0）から直前の攻方王手1手を戻し、
/// 攻方手番の1手詰候補を生成する。
QList<Candidate> reverseFromMateEnding(const QString& mateEndingSfen, const Limits& limits,
                                       int maxCandidates, std::mt19937_64& rng);

/// 攻方手番の局面から「玉方応手1手＋攻方王手1手」を逆算し、2手前の攻方手番候補を生成する。
/// 玉移動・玉の駒取り（捨駒）・打合・移動合・玉方駒の取り返しと、
/// 駒打ち・盤上移動・成り戻し・駒取り戻し・開き王手・両王手を組み合わせ、
/// 全候補について順方向シミュレーションで完全な合法性を保証する。
QList<Candidate> generatePredecessors(const QString& sfen, const Limits& limits,
                                      int maxCandidates, std::mt19937_64& rng);

/// 目標手数を超える詰み手順（pv.size() > targetMoves）の先頭を進め、
/// ちょうど targetMoves 手の接尾辞局面を切り出す。
std::optional<SuffixSlice> sliceToTargetLength(const QString& sfen, const QStringList& pv, int targetMoves);

/// 詰将棋開始局面としての妥当性（片玉・40駒以内・二歩なし・行き所のない駒なし・初期王手なし）を検査し、
/// 正規化した攻方手番SFENを返す。不正な場合は空文字列。
QString normalizeProblemSfen(const QString& sfen, bool addRemainingToDefenderHand = true);

/// 詰み上がり局面（玉方手番・王手あり・合法手0）として妥当かを判定する。
bool isMateEnding(const QString& sfen);

} // namespace TsumeRetrogradeOps

#endif // TSUMERETROGRADEOPS_H
