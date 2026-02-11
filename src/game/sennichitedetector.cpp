/// @file sennichitedetector.cpp
/// @brief 千日手検出ユーティリティクラスの実装

#include "sennichitedetector.h"
#include "shogiboard.h"
#include "movevalidator.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcGame)

QString SennichiteDetector::positionKey(const QString& sfen)
{
    // SFEN形式: "board turn hand moveNum"
    // 手数（最後のトークン）を除いた "board turn hand" を返す
    const auto lastSpace = sfen.lastIndexOf(QLatin1Char(' '));
    if (lastSpace <= 0) return sfen;
    return sfen.left(lastSpace);
}

SennichiteDetector::Result SennichiteDetector::check(const QStringList& sfenRecord)
{
    if (sfenRecord.size() < 4) return Result::None;

    const QString currentKey = positionKey(sfenRecord.last());
    if (currentKey.isEmpty()) return Result::None;

    // 同一局面キーの出現インデックスを収集
    QVector<int> occurrences;
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

    ShogiBoard board;
    MoveValidator validator;

    int p1CheckCount = 0;   // 先手が王手をかけた回数（後手番で後手の玉が王手されている回数）
    int p2CheckCount = 0;   // 後手が王手をかけた回数（先手番で先手の玉が王手されている回数）
    int totalPositions = 0; // thirdIdxとfourthIdxの間の局面数（両端除く）

    // thirdIdx+1 から fourthIdx-1 までの局面と、fourthIdx自身を調べる
    // thirdIdx+1 は3回目の出現の直後（次の手が指された局面）
    for (int i = thirdIdx + 1; i <= fourthIdx; ++i) {
        board.setSfen(sfenRecord.at(i));
        ++totalPositions;

        // 手番を判定
        const QStringList tokens = sfenRecord.at(i).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() < 2) continue;

        const bool sideToMoveIsBlack = (tokens.at(1) == QLatin1String("b"));
        const MoveValidator::Turn turn = sideToMoveIsBlack
                                             ? MoveValidator::BLACK
                                             : MoveValidator::WHITE;

        // 手番側の玉が王手されているか
        const int checks = validator.checkIfKingInCheck(turn, board.boardData());
        if (checks > 0) {
            // 手番側の玉が王手されている = 直前の手（相手の手）が王手
            if (sideToMoveIsBlack) {
                // 先手番で先手玉が王手されている → 直前の後手の手が王手
                ++p2CheckCount;
            } else {
                // 後手番で後手玉が王手されている → 直前の先手の手が王手
                ++p1CheckCount;
            }
        }
    }

    if (totalPositions == 0) return Result::Draw;

    // 一方が全ての手で王手を続けていたら連続王手の千日手
    if (p1CheckCount == totalPositions) {
        qCInfo(lcGame) << "Sennichite: continuous check by P1 (sente)";
        return Result::ContinuousCheckByP1;
    }
    if (p2CheckCount == totalPositions) {
        qCInfo(lcGame) << "Sennichite: continuous check by P2 (gote)";
        return Result::ContinuousCheckByP2;
    }

    qCInfo(lcGame) << "Sennichite: draw by repetition";
    return Result::Draw;
}
