#ifndef BRANCHDISPLAYPLAN_H
#define BRANCHDISPLAYPLAN_H

#include <QString>
#include <QVector>
#include <QHash>

// 分岐候補の 1 アイテム（分岐候補欄の 1 行に相当）
struct BranchCandidateDisplayItem {
    int row   = -1;          // 切替先の行（Main=0, Var0=1, Var1=2, ...）
    int varN  = -1;          // -1=Main, 0..=VarN （表示の都合で保持）
    QString lineName;        // "Main" or "VarN"
    QString label;           // 指し手ラベル（例: "▲２六歩(27)"）
};

// ある行(r)の、ある手数(ply1)に表示する計画（「分岐候補表示あり」の中身）
struct BranchCandidateDisplay {
    int ply = 0;             // 1-based 手数
    QString baseLabel;       // その行(r)の ply 手目のラベル（基準表示）
    QVector<BranchCandidateDisplayItem> items;  // 表示候補たち
};

// 行 r → ( 手 ply1 → 計画 ) の二段 map
using BranchDisplayPlanByPly  = QHash<int, BranchCandidateDisplay>;
using BranchDisplayPlanTable  = QHash<int, BranchDisplayPlanByPly>;

#endif // BRANCHDISPLAYPLAN_H
