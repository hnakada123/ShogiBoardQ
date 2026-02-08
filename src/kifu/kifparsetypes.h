#ifndef KIFPARSETYPES_H
#define KIFPARSETYPES_H

/// @file kifparsetypes.h
/// @brief KIF棋譜パース用データ型の定義

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>

#include "shogimove.h"

// KifDisplayItem は前方宣言に留める。
// 実体定義を使う翻訳単位（.cpp）側でヘッダを include すること。
struct KifDisplayItem;

/// KIFの「(キーワード)：(内容)」行を表すペア
///
struct KifGameInfoItem {
    QString key;    ///< キーワード（例: "開始日時"）
    QString value;  ///< 内容（例: "2025/03/02 09:00"）
};

// --- 分岐用データ構造 ---

/**
 * @brief 1本の手順（分岐ライン）を表す構造体
 *
 * @todo remove コメントスタイルガイド適用済み
 */
struct KifLine {
    int startPly = 1;             ///< どの手目から開始か（例: 5）
    QString baseSfen;             ///< 分岐開始局面（メインを startPly-1 まで進めたSFEN）
    QStringList usiMoves;         ///< 分岐のUSI指し手列
    QList<KifDisplayItem> disp;   ///< 表示用（▲△＋時間）: 先頭=分岐の最初の手
    QStringList sfenList;         ///< base含む局面列（0=base, 以降=各手後）
    QVector<ShogiMove> gameMoves; ///< GUI用指し手列
    bool endsWithTerminal = false; ///< 終局手で終わるか
};

/**
 * @brief 変化（どの手数から分岐するか）を表す構造体
 *
 * @todo remove コメントスタイルガイド適用済み
 */
struct KifVariation {
    int startPly = 1;  ///< 変化開始手数（例: 「変化：65手」なら 65）
    KifLine line;      ///< 変化の手順
};

/**
 * @brief KIFパース結果（本譜＋変化）を格納する構造体
 *
 * @todo remove コメントスタイルガイド適用済み
 */
struct KifParseResult {
    KifLine mainline;                  ///< 本譜
    QVector<KifVariation> variations;  ///< 変化リスト
};

#endif // KIFPARSETYPES_H
