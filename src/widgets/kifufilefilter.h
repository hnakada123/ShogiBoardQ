#ifndef KIFUFILEFILTER_H
#define KIFUFILEFILTER_H

/// @file kifufilefilter.h
/// @brief 棋譜ファイルを開くダイアログの絞り込み（拡張子は KifuFormats の表から作る）

#include <QString>

namespace KifuFileFilter {

/// 棋譜ファイルを開くダイアログの絞り込み（開ける棋譜ファイル全部・形式ごと・すべてのファイル）
QString forOpening();

} // namespace KifuFileFilter

#endif // KIFUFILEFILTER_H
