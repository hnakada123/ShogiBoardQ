/// @file josekiioresult.h
/// @brief 定跡ファイルI/Oの結果型（スレッド間受け渡し用値オブジェクト）

#pragma once

#include <QMap>
#include <QVector>
#include <QString>

struct JosekiMove;

/**
 * @brief 定跡ファイル読み込み結果（値型）
 *
 * ワーカースレッドで生成し、メインスレッドで適用する。
 */
struct JosekiLoadResult {
    bool success = false;
    QString errorMessage;

    /// 読み込んだ定跡データ（正規化SFEN → 指し手リスト）
    QMap<QString, QVector<JosekiMove>> josekiData;

    /// 元のSFEN（手数付き）マップ
    QMap<QString, QString> sfenWithPlyMap;

    /// 読み込んだ局面数
    int positionCount = 0;
};

/**
 * @brief 定跡ファイル保存結果（値型）
 */
struct JosekiSaveResult {
    bool success = false;
    QString errorMessage;
    int savedCount = 0;
};
