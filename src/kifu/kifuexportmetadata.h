#ifndef KIFUEXPORTMETADATA_H
#define KIFUEXPORTMETADATA_H

/// @file kifuexportmetadata.h
/// @brief UI に依存しない棋譜出力メタデータとヘッダ生成

#include <QDateTime>
#include "kifparsetypes.h"
#include "matchplayers.h"
#include "timecontrol.h"

/**
 * @brief 棋譜を書き出すときの対局情報と対局の記録
 *
 * 対局情報（gameInfoItems）は書かれたとおりに書く（日時や対局者を作り出さない）。棋譜の文書からは
 * KifuExport::metadataOf で作る。
 */
struct KifuExportMetadata {
    QList<KifGameInfoItem> gameInfoItems;  ///< 書き出す対局情報（そのまま書く）
    QString startSfen;
    MatchPlayers players;  ///< CSA の N+・N- の補い（対局情報に対局者が無いとき。名前が無ければ先手・後手・Engine）

    // --- 時間制御情報（CSA出力用） ---
    TimeControl timeControl;               ///< 持ち時間（双方とも時間制限がなければ書かない）
    QDateTime gameStartDateTime;           ///< 対局開始日時
    QDateTime gameEndDateTime;             ///< 対局終了日時
};

namespace KifuExportMetadataBuilder {
void resolvePlayerNames(const KifuExportMetadata& ctx, QString& black, QString& white);
/// 開始局面から対局情報の手合割（平手 / その他）を決める
QString handicapLabel(const QString& startSfen);
/// 手合割が駒落ち（平手・その他・未設定以外）か
bool isHandicap(const QString& handicapLabel);
/// 対局者の見出し。駒落ちは柿木形式の KIF と同じく「下手」「上手」、それ以外は「先手」「後手」
QString blackPlayerKey(const QString& handicapLabel);
QString whitePlayerKey(const QString& handicapLabel);
/// 対局情報の対局者名（先手・後手。駒落ちは下手・上手。なければ省略名、それもなければ空）
QString playerName(const QList<KifGameInfoItem>& header, Turn side);
/// 書き出す棋譜の局面図（持駒・手番）と終局行で、対局者を下手・上手と呼ぶか。
/// 対局情報の見出し（下手・上手か先手・後手か）に合わせ、見出しに対局者がなければ開始局面の手合割で決める
bool usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen);
}

#endif // KIFUEXPORTMETADATA_H
