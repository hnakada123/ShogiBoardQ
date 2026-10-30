#ifndef KIFUEXPORTMETADATA_H
#define KIFUEXPORTMETADATA_H

/// @file kifuexportmetadata.h
/// @brief 棋譜を書き出すときの文脈（対局情報と開始局面）

#include "kifparsetypes.h"

/**
 * @brief 棋譜を書き出すときの対局情報と開始局面
 *
 * 対局情報（gameInfoItems）は書かれたとおりに書く（日時や対局者を作り出さない。規則は GameInfo）。
 * 棋譜の文書からは KifuExport::metadataOf で作る。
 */
struct KifuExportMetadata {
    QList<KifGameInfoItem> gameInfoItems;  ///< 書き出す対局情報（そのまま書く）
    QString startSfen;
};

#endif // KIFUEXPORTMETADATA_H
