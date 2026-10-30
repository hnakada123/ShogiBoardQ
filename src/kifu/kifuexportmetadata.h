#ifndef KIFUEXPORTMETADATA_H
#define KIFUEXPORTMETADATA_H

/// @file kifuexportmetadata.h
/// @brief 棋譜を書き出すときの文脈（対局情報・開始局面・文字コード）

#include "kifparsetypes.h"
#include "kifuformat.h"

/**
 * @brief 棋譜を書き出すときの対局情報・開始局面・文字コード
 *
 * 対局情報（gameInfoItems）は書かれたとおりに書く（日時や対局者を作り出さない。規則は GameInfo）。
 * 棋譜の文書からは KifuExport::metadataOf で作る。
 */
struct KifuExportMetadata {
    QList<KifGameInfoItem> gameInfoItems;  ///< 書き出す対局情報（そのまま書く）
    QString startSfen;
    /// 書いた行を保存する文字コード（KIF・KI2・CSA の先頭行の宣言に書く。保存先は KifuSaveFile が決める）
    KifuEncoding encoding = KifuEncoding::Utf8;
};

#endif // KIFUEXPORTMETADATA_H
