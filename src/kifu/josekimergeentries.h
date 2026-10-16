#ifndef JOSEKIMERGEENTRIES_H
#define JOSEKIMERGEENTRIES_H

/// @file josekimergeentries.h
/// @brief 棋譜から定跡にマージする指し手の一覧を作る

#include <QList>
#include <QString>
#include <QStringList>

#include "josekitypes.h"

namespace JosekiMergeEntries {

/**
 * @brief 表示中の手順からマージの候補を作る
 * @param sfens 各手を指す前の局面（[i] が i+1 手目の前）
 * @param usiMoves 各手の USI の指し手
 * @param moveTexts 各手の棋譜の表記（[i] が i+1 手目。無ければ USI の指し手を出す）
 * @param currentPly 表示中の手数（その手に印を付ける）
 */
QList<KifuMergeEntry> fromLine(const QStringList &sfens, const QStringList &usiMoves,
                               const QStringList &moveTexts, int currentPly);

/// 棋譜ファイルの本譜からマージの候補を作る。読めなければ false（errorMessage が空なら指し手がない）
bool fromKifFile(const QString &kifFilePath, QList<KifuMergeEntry> &entries, QString *errorMessage);

} // namespace JosekiMergeEntries

#endif // JOSEKIMERGEENTRIES_H
