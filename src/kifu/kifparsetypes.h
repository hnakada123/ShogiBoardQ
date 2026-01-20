#ifndef KIFPARSETYPES_H
#define KIFPARSETYPES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>

#include "shogimove.h"

// KifDisplayItem は前方宣言に留めます。
// 実体定義を使う翻訳単位（.cpp）側でヘッダを include してください。
struct KifDisplayItem;

// KIFの「(キーワード)：(内容)」行を表すペア
struct KifGameInfoItem {
    QString key;    // 例: "開始日時"
    QString value;  // 例: "2025/03/02 09:00"
};

// ==== 分岐用データ構造 ====
// 1本の手順（分岐ライン）
struct KifLine {
    int startPly = 1;            // どの手目から開始か（例: 5）
    QString baseSfen;            // 分岐開始局面（メインを startPly-1 まで進めたSFEN）
    QStringList usiMoves;        // 分岐のUSI列
    QList<KifDisplayItem> disp;  // 表示用（▲△＋時間）: 先頭=分岐の最初の手
    QStringList sfenList;        // base含む局面列（0=base, 以降=各手後）
    QVector<ShogiMove> gameMoves;// GUI用指し手列
    bool endsWithTerminal = false;
};

// 変化（どの手から始まるか）
struct KifVariation {
    int startPly = 1;  // 例: 「変化：65手」なら 65
    KifLine line;
};

// 解析結果（本譜＋変化）
struct KifParseResult {
    KifLine mainline;
    QVector<KifVariation> variations;
};

#endif // KIFPARSETYPES_H
