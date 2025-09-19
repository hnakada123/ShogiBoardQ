#ifndef KIFUVARIATIONENGINE_H
#define KIFUVARIATIONENGINE_H

#include <QList>
#include <QVector>
#include <QHash>
#include <QPair>
#include <QString>
#include <QSet>

// ★ 実体定義を提供するヘッダを必ずインクルードする
// KifDisplayItem / KifLine / KifVariation / KifParseResult が定義されているヘッダ
#include "kiftosfenconverter.h"   // ← あなたのプロジェクトではここに KifParseResult 等が定義されています

#include <QList>
#include <QVector>
#include <QHash>
#include <QPair>
#include <QString>
#include <QSet>

// 指し手1手のUSI文字列（例: "7g7f", "P*5e", "7g7f+" など）
using UsiMove = QString;

// 分岐候補1件（分岐欄に表示するラベル＋内部ID）
struct BranchCandidate {
    QString label;   // 例: "１六歩(17)"
    int     variationId = -1; // エンジン内部で付与する連番ID
    int     ply = 0;          // 何手目の候補か（クリック時の参照用）
    bool    isBlack = false;  // 先後（色分け用）
    bool    isMainline = false;
};

// “後勝ちで合成”した1本分（UIへ返す最終形）
struct ResolvedLine {
    int startPly = 1;                 // 見た目上 1 から
    QList<KifDisplayItem> disp;       // 1..N 表示列
    QVector<UsiMove>      usi;        // 1..N USI列（可能な限り前半上書きを反映）
    // SFENは MainWindow 側の rebuildSfenRecord() を用いて再構築してください
};

// 純ロジック：分岐候補の列挙と「後勝ち」合成を担当（UI/Qtビューから独立）
class KifuVariationEngine {
public:
    KifuVariationEngine();

    // 読み込み直後に呼ぶ：本譜と分岐一式を取り込み、内部インデックスを構築
    // sfenMain はここでは保持しません（SFEN再構築は呼び出し側の既存関数を使うため）
    void ingest(const KifParseResult& res,
                const QList<QString>& sfenMain,        // 0..N（未使用だが将来拡張のため受けておく）
                const QVector<UsiMove>& usiMain,       // 1..N
                const QList<KifDisplayItem>& dispMain  // 1..N
                );

    // ある手数 ply に対する“分岐候補”を返す。includeMainline=true で本譜の手も候補に含める。
    QList<BranchCandidate> branchCandidatesForPly(int ply,
                                                  bool includeMainline = true) const;

    // 分岐候補がクリックされた際：後勝ちで一本の列を合成して返す
    ResolvedLine resolveAfterWins(int variationId) const;

private:
    struct Variation {
        int id = -1;
        int fileOrder = 0;     // KIF登場順。本譜=0、変化=1,2,…
        int startPly = 1;
        QList<KifDisplayItem> disp;  // 表示列
        QVector<UsiMove>      usi;   // USI列（disp と同じ長さ or 空でも可）
        bool isMainline = false;
    };

    // インデックス
    QList<KifDisplayItem> m_dispMain;    // 本譜 1..N
    QVector<UsiMove>      m_usiMain;     // 本譜 1..N
    QList<Variation>      m_vars;        // 本譜 + 変化群（fileOrder昇順）
    // ply -> [(variationId, localIdx)]  localIdx はその variation 内での 0 始まり
    QHash<int, QVector<QPair<int,int>>> m_idx;

    static QString pickLabel(const KifDisplayItem& d); // "１六歩(17)" を返す（prettyMove 流用）
};

#endif // KIFUVARIATIONENGINE_H
