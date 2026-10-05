#ifndef KIFUEXPORTMETADATA_H
#define KIFUEXPORTMETADATA_H

/// @file kifuexportmetadata.h
/// @brief UI に依存しない棋譜出力メタデータとヘッダ生成

#include <QDateTime>
#include "kifparsetypes.h"
#include "playmode.h"

struct KifuExportMetadata {
    bool gameInfoProvided = false; ///< 空のメタデータも明示的な編集結果として扱う
    QList<KifGameInfoItem> gameInfoItems;  ///< 明示された対局情報（空でも gameInfoProvided なら採用）
    QString startSfen;
    PlayMode playMode = PlayMode::NotStarted;
    QString human1;
    QString human2;
    QString engine1;
    QString engine2;

    // --- 時間制御情報（CSA出力用） ---
    bool hasTimeControl = false;           ///< 時間制御が有効かどうか
    int initialTimeMs = 0;                 ///< 初期持ち時間（ミリ秒）
    int byoyomiMs = 0;                     ///< 秒読み（ミリ秒）
    int fischerIncrementMs = 0;            ///< フィッシャー加算（ミリ秒）
    QDateTime gameStartDateTime;           ///< 対局開始日時
    QDateTime gameEndDateTime;             ///< 対局終了日時
};

namespace KifuExportMetadataBuilder {
/// Compact は従来の簡易KIF保存の日時書式を維持する。
enum class HeaderStyle { Full, Compact };
QList<KifGameInfoItem> collect(const KifuExportMetadata& ctx, HeaderStyle style = HeaderStyle::Full);
void resolvePlayerNames(const KifuExportMetadata& ctx, QString& black, QString& white);
/// 開始局面から対局情報の手合割（平手 / その他）を決める
QString handicapLabel(const QString& startSfen);
/// 対局情報の持ち時間。秒読みは「mm:ss+秒」、フィッシャー加算は「mm:ss+秒秒加算」で区別する
QString timeControlText(qint64 baseMs, qint64 byoyomiMs, qint64 incrementMs);
}

#endif // KIFUEXPORTMETADATA_H
