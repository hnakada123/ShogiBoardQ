#ifndef KIFUEXPORTMETADATA_H
#define KIFUEXPORTMETADATA_H

/// @file kifuexportmetadata.h
/// @brief UI に依存しない棋譜出力メタデータとヘッダ生成

#include <QDateTime>
#include "kifparsetypes.h"
#include "playmode.h"

/// 対局者ひとり分の持ち時間（対局情報と棋譜の書き出しで使う）
struct KifuTimeControlSide {
    qint64 baseMs = 0;       ///< 持ち時間（ミリ秒）
    qint64 byoyomiMs = 0;    ///< 秒読み（ミリ秒）
    qint64 incrementMs = 0;  ///< 1手ごとの加算（ミリ秒）
};

inline bool operator==(const KifuTimeControlSide& a, const KifuTimeControlSide& b)
{
    return a.baseMs == b.baseMs && a.byoyomiMs == b.byoyomiMs && a.incrementMs == b.incrementMs;
}

inline bool operator!=(const KifuTimeControlSide& a, const KifuTimeControlSide& b)
{
    return !(a == b);
}

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
    KifuTimeControlSide blackTime;         ///< 先手（下手）の持ち時間
    KifuTimeControlSide whiteTime;         ///< 後手（上手）の持ち時間
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
/// 手合割が駒落ち（平手・その他・未設定以外）か
bool isHandicap(const QString& handicapLabel);
/// 対局者の見出し。駒落ちは柿木形式の KIF と同じく「下手」「上手」、それ以外は「先手」「後手」
QString blackPlayerKey(const QString& handicapLabel);
QString whitePlayerKey(const QString& handicapLabel);
/// 書き出す棋譜の局面図（持駒・手番）と終局行で、対局者を下手・上手と呼ぶか。
/// 対局情報の見出し（下手・上手か先手・後手か）に合わせ、見出しに対局者がなければ開始局面の手合割で決める
bool usesHandicapNames(const QList<KifGameInfoItem>& header, const QString& startSfen);
/// 対局情報の持ち時間。秒読みは「mm:ss+秒」、フィッシャー加算は「mm:ss+秒秒加算」で区別する
QString timeControlText(qint64 baseMs, qint64 byoyomiMs, qint64 incrementMs);
/// 先後の持ち時間。同じなら1つで、違えば「先手 mm:ss+秒 / 後手 mm:ss+秒」（駒落ちは下手・上手）
QString timeControlText(const KifuTimeControlSide& black, const KifuTimeControlSide& white, bool handicap);
}

#endif // KIFUEXPORTMETADATA_H
