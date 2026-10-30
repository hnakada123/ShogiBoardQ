#ifndef KIFUFILEFILTER_H
#define KIFUFILEFILTER_H

/// @file kifufilefilter.h
/// @brief 棋譜ファイルを開く・保存するダイアログの絞り込み（拡張子と見出しは KifuFormats の表から作る）

#include <QString>

namespace KifuFileFilter {

/// 棋譜ファイルを開くダイアログの絞り込み（開ける棋譜ファイル全部・形式ごと・すべてのファイル）
QString forOpening();

/// 棋譜を保存するダイアログの絞り込み（保存できる拡張子ごと・すべてのファイル。並びは KifuFormats::saveTargets）
QString forSaving();

/// 保存ダイアログで選んだパス path に拡張子が無ければ、選んだ絞り込み selectedFilter の拡張子を付ける
/// （保存できる拡張子の絞り込みでなければ .kifu）
QString pathForSaving(const QString& path, const QString& selectedFilter);

} // namespace KifuFileFilter

#endif // KIFUFILEFILTER_H
