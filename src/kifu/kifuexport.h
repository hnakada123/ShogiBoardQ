#ifndef KIFUEXPORT_H
#define KIFUEXPORT_H

/// @file kifuexport.h
/// @brief 棋譜の書き出し（形式ごとの書き出し関数の表）

#include <QStringList>

#include "kifuexportmetadata.h"
#include "kifuformat.h"

class KifuBranchTree;
class KifuDocument;

/**
 * @brief 棋譜を形式ごとの行にする
 *
 * 書き出しは「棋譜（分岐ツリー）と書き出しの文脈から行を返す関数」を形式ごとに並べた表で選ぶ。
 * GUI の保存・コピーと CLI の変換で同じ表を使う。
 */
namespace KifuExport {

/// 文書の書き出しの文脈（対局情報・開始局面）
KifuExportMetadata metadataOf(const KifuDocument& document);
/// format の行（SFEN・BOD は棋譜を書けないので KIF にする）。文字コードの宣言は metadata.encoding
QStringList lines(const KifuBranchTree& tree, const KifuExportMetadata& metadata, KifuFormat format);
/// 文書を format の行にする（encoding はファイルに保存する文字コード。先頭行の宣言に書く）
QStringList lines(const KifuDocument& document, KifuFormat format, KifuEncoding encoding = KifuEncoding::Utf8);

} // namespace KifuExport

#endif // KIFUEXPORT_H
