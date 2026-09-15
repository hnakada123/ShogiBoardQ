#ifndef KIFUAPPLYLOGGER_H
#define KIFUAPPLYLOGGER_H

/// @file kifuapplylogger.h
/// @brief 棋譜インポートのログ出力ヘルパの定義

#include <QList>
#include <QString>
#include <QStringList>

#include "shogimove.h"

struct KifDisplayItem;
struct KifParseResult;

namespace KifuApplyLogger {

/// 解析した本譜・分岐の詳細をデバッグログへ出力する。
void dumpMainline(const KifParseResult& res, const QString& parseWarn);
void dumpVariationsDebug(const KifParseResult& res);

void logImportSummary(const QString& filePath,
                      const QStringList& usiMoves,
                      const QList<KifDisplayItem>& disp,
                      const QString& teaiLabel,
                      const QString& warnParse,
                      const QString& warnConvert,
                      const QStringList* sfenHistory,
                      const QList<ShogiMove>* gameMoves);

} // namespace KifuApplyLogger

#endif // KIFUAPPLYLOGGER_H
