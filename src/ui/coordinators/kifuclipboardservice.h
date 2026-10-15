#ifndef KIFUCLIPBOARDSERVICE_H
#define KIFUCLIPBOARDSERVICE_H

/// @file kifuclipboardservice.h
/// @brief 棋譜クリップボードサービスの定義


#include "gamerecordmodel.h"

/// KIF/KI2形式の棋譜をクリップボードに書き込む。
namespace KifuClipboardService {

/// KIF形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
[[nodiscard]] bool copyKif(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx);

/// KI2形式で棋譜をクリップボードにコピー
/// @return 成功した場合true
[[nodiscard]] bool copyKi2(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx);

} // namespace KifuClipboardService

#endif // KIFUCLIPBOARDSERVICE_H
