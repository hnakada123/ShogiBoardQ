#ifndef INPUTPATH_H
#define INPUTPATH_H

/// @file inputpath.h
/// @brief 読み込むファイルの事前検査（CLI の入力と自動化 API の kifu.load で共通。GUI 非依存）

#include <QString>
#include <optional>

#include "errorkinds.h"

namespace InputPath {

/**
 * @brief 読み込むファイルを確かめる
 *
 * 空・絶対パスでない・通常のファイルでない（ディレクトリ・FIFO・デバイス。FIFO は読み終わらない）は
 * invalid_argument、無いファイルは not_found を、読む前に分類つきで返す。問題がなければ nullopt。
 * 書き出し先の検査 OutputPath::check と対で、MCP サーバー（mcp/shogiboardq_mcp/paths.py）も同じ分類にする。
 */
std::optional<Failure> check(const QString& path);

} // namespace InputPath

#endif // INPUTPATH_H
