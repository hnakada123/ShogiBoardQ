/// @file sennichitedetector.cpp
/// @brief 千日手検出ユーティリティクラスの実装

#include "sennichitedetector.h"
#include "sfenutils.h"
#include "shogiposition.h"
#include "enginemovevalidator.h"

#include "logcategories.h"

QString SennichiteDetector::positionKey(const QString& sfen)
{
    // 手数を除いた "board turn hand"（持ち駒は標準の並び）
    return SfenUtils::normalizeSfenKey(sfen);
}

SennichiteDetector::Result SennichiteDetector::check(const QStringList& sfenRecord)
{
    if (sfenRecord.size() < 4) return Result::None;

    const QString currentKey = positionKey(sfenRecord.last());
    if (currentKey.isEmpty()) return Result::None;

    // 同一局面キーの出現インデックスを収集
    QList<int> occurrences;
    for (int i = 0; i < sfenRecord.size(); ++i) {
        if (positionKey(sfenRecord.at(i)) == currentKey) {
            occurrences.append(i);
        }
    }

    if (occurrences.size() < 4) return Result::None;

    // 4回以上出現 → 連続王手判定
    const int thirdIdx  = occurrences.at(occurrences.size() - 2);  // 3回目
    const int fourthIdx = occurrences.last();                       // 4回目（末尾）

    return checkContinuousCheck(sfenRecord, thirdIdx, fourthIdx);
}

SennichiteDetector::Result SennichiteDetector::checkContinuousCheck(
    const QStringList& sfenRecord,
    int thirdIdx,
    int fourthIdx)
{
    // 3回目と4回目の間の局面を調べる
    // 各局面について王手判定を行い、一方の側が全て王手なら連続王手
    //
    // SFENの手番フィールド:
    //   "b" = 先手（Black）の手番 → 直前に後手が指した
    //   "w" = 後手（White）の手番 → 直前に先手が指した
    //
    // 「手番側の玉が王手されている」= 直前の相手の手が王手だった

    if (fourthIdx - thirdIdx < 2) return Result::Draw;

    Position board;

    // 一方の指し手がすべて王手かどうかは、その側が指した直後の局面（相手の手番の局面）だけで数える。
    // 王手をかけた側の手番の局面まで分母に入れると、毎手王手でも成立しない
    int blackCheckCount = 0;   // 先手が王手をかけた回数（後手番で後手の玉が王手されている回数）
    int whiteCheckCount = 0;   // 後手が王手をかけた回数（先手番で先手の玉が王手されている回数）
    int blackMoves = 0;        // thirdIdx と fourthIdx の間で先手が指した手の数（後手番の局面数）
    int whiteMoves = 0;        // 同じく後手が指した手の数（先手番の局面数）

    // thirdIdx+1 から fourthIdx までの局面を調べる
    // thirdIdx+1 は3回目の出現の直後（次の手が指された局面）
    for (int i = thirdIdx + 1; i <= fourthIdx; ++i) {
        if (const auto position = Position::fromSfen(sfenRecord.at(i))) board = *position;

        // 手番を判定
        const QStringList tokens = sfenRecord.at(i).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() < 2) continue;

        const bool sideToMoveIsBlack = (tokens.at(1) == QLatin1String("b"));
        const Turn turn = sideToMoveIsBlack
                                                   ? Turn::Black
                                                   : Turn::White;

        // 手番側の玉が王手されているか（= 直前の相手の手が王手）
        const bool inCheck = EngineMoveValidator::isInCheck(board, turn);
        if (sideToMoveIsBlack) {
            // 先手番の局面 = 直前に後手が指した
            ++whiteMoves;
            if (inCheck) ++whiteCheckCount;
        } else {
            // 後手番の局面 = 直前に先手が指した
            ++blackMoves;
            if (inCheck) ++blackCheckCount;
        }
    }

    // 一方が全ての手で王手を続けていたら連続王手の千日手
    if (blackMoves > 0 && blackCheckCount == blackMoves) {
        qCInfo(lcGame) << "Sennichite: continuous check by black (sente)";
        return Result::ContinuousCheckByBlack;
    }
    if (whiteMoves > 0 && whiteCheckCount == whiteMoves) {
        qCInfo(lcGame) << "Sennichite: continuous check by white (gote)";
        return Result::ContinuousCheckByWhite;
    }

    qCInfo(lcGame) << "Sennichite: draw by repetition";
    return Result::Draw;
}
