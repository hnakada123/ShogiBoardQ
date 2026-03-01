#ifndef KIFUAPPLYLOGGER_H
#define KIFUAPPLYLOGGER_H

/// @file kifuapplylogger.h
/// @brief 棋譜インポートのログ出力ヘルパの定義

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include "shogimove.h"

struct KifDisplayItem;

namespace KifuApplyLogger {

void logImportSummary(const QString& filePath,
                      const QStringList& usiMoves,
                      const QList<KifDisplayItem>& disp,
                      const QString& teaiLabel,
                      const QString& warnParse,
                      const QString& warnConvert,
                      const QStringList* sfenHistory,
                      const QVector<ShogiMove>* gameMoves);

} // namespace KifuApplyLogger

#endif // KIFUAPPLYLOGGER_H
