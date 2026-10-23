#ifndef OUTPUTPATH_H
#define OUTPUTPATH_H

/// @file outputpath.h
/// @brief 書き出し先のファイルの事前検査（CLI の出力先と自動化 API の kifu.save で共通。GUI 非依存）

#include <QString>
#include <optional>

#include "errorkinds.h"

namespace OutputPath {

/**
 * @brief 書き出し先のファイルを確かめる
 *
 * 絶対パスでない・ディレクトリ（invalid_argument）、上書きの許可なしに既存のファイル（file_exists）、
 * 置く場所のディレクトリがない（not_found）を、書く前に分類つきで返す。問題がなければ nullopt。
 * MCP サーバー（mcp/shogiboardq_mcp/paths.py）も同じ分類にする。
 * @param overwriteHint 既存のファイルのときに添える対処（CLI は "--overwrite"、自動化 API は "overwrite=true"）
 */
std::optional<Failure> check(const QString& path, bool overwrite, const QString& overwriteHint);

} // namespace OutputPath

#endif // OUTPUTPATH_H
