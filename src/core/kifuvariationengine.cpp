#include "kifuvariationengine.h"
#include <algorithm>

// ---- 依存する既存型の前提 ----
// KifDisplayItem が public メンバ prettyMove (QString) を持つ前提。
// 必要ならここをあなたの定義に合わせて変更してください。

// ============ ユーティリティ ============
static inline bool isBlackOnPly(int ply) { return (ply % 2) == 1; }

// ============ 実装 ============
KifuVariationEngine::KifuVariationEngine() {}

void KifuVariationEngine::ingest(const KifParseResult& res,
                                 const QList<QString>& /*sfenMain*/,
                                 const QVector<UsiMove>& usiMain,
                                 const QList<KifDisplayItem>& dispMain)
{
    qDebug() << "[VE] ingest() begin";
    m_dispMain = dispMain;
    m_usiMain  = usiMain;
    m_vars.clear();
    m_idx.clear();

    // 本譜
    {
        Variation v;
        v.id = 0; v.fileOrder = 0; v.isMainline = true;
        v.startPly = 1;
        v.disp = dispMain; v.usi = usiMain;
        m_vars.push_back(v);

        for (int li=0; li<v.disp.size(); ++li) {
            const int ply = v.startPly + li;
            m_idx[ply].push_back({v.id, li});
        }
        qDebug().nospace() << "[VE] mainline: moves=" << v.disp.size();
    }

    // 変化
    int order = 1;
    int nextId = 1;
    for (const KifVariation& kv : res.variations) {
        if (kv.line.disp.isEmpty()) continue;

        Variation v;
        v.id = nextId++;
        v.fileOrder = order++;
        v.isMainline = false;
        v.startPly = kv.startPly;
        v.disp = kv.line.disp;
        v.usi  = kv.line.usiMoves;
        m_vars.push_back(v);

        for (int li=0; li<v.disp.size(); ++li) {
            const int ply = v.startPly + li;
            m_idx[ply].push_back({v.id, li});
        }
        qDebug().nospace() << "[VE] var id=" << v.id
                           << " start=" << v.startPly
                           << " len=" << v.disp.size();
    }

    // インデックス概要
    qDebug() << "[VE] ingest() built index:";
    for (auto it = m_idx.constBegin(); it != m_idx.constEnd(); ++it) {
        qDebug().nospace() << "  ply " << it.key() << " -> count " << it.value().size();
    }
}

QList<BranchCandidate> KifuVariationEngine::branchCandidatesForPly(int ply,
                                                                   bool includeMainline) const
{
    qDebug().nospace() << "[VE] branchCandidatesForPly ply=" << ply
                       << " includeMainline=" << includeMainline;

    QList<BranchCandidate> out;

    const auto it = m_idx.find(ply);
    if (it == m_idx.end()) {
        qDebug() << "[VE] no index for ply -> 0 candidates";
        return out;
    }

    auto pairs = it.value(); // QVector<QPair<int,int>> (variationId, localIndex)
    qDebug().nospace() << "[VE] raw pairs=" << pairs.size();

    std::sort(pairs.begin(), pairs.end(), [&](auto a, auto b){
        const auto& va = m_vars[a.first];
        const auto& vb = m_vars[b.first];
        if (va.isMainline != vb.isMainline) return va.isMainline && !vb.isMainline;
        return va.fileOrder < vb.fileOrder;
    });

    QSet<QString> seen;
    int n = 0;
    for (auto [vid, li] : pairs) {
        const auto& v = m_vars[vid];
        if (!includeMainline && v.isMainline) {
            qDebug().nospace() << "  skip mainline vid=" << vid << " li=" << li;
            continue;
        }
        const auto& d = v.disp[li];

        BranchCandidate c;
        c.label       = pickLabel(d);
        c.variationId = vid;
        c.ply         = ply;
        c.isBlack     = isBlackOnPly(ply);
        c.isMainline  = v.isMainline;

        if (seen.contains(c.label)) {
            qDebug().nospace() << "  drop dup label=" << c.label;
            continue;
        }
        seen.insert(c.label);
        out.push_back(std::move(c));

        qDebug().nospace() << "  [" << (n++) << "] vid=" << vid
                           << " li=" << li
                           << " start=" << v.startPly
                           << " label=" << d.prettyMove
                           << " isMain=" << v.isMainline;
    }

    qDebug().nospace() << "[VE] produced candidates=" << out.size();
    return out;
}

ResolvedLine KifuVariationEngine::resolveAfterWins(int targetId) const
{
    ResolvedLine r;

    if (targetId < 0 || targetId >= m_vars.size()) return r;
    const auto& tgt = m_vars[targetId];

    // 1) プレフィクス長（1..start-1）
    const int prefixEnd = std::max(1, tgt.startPly) - 1;

    // 2) 本譜でプレフィクスを作る
    QList<KifDisplayItem> disp = m_dispMain.mid(0, prefixEnd);
    QVector<UsiMove>      usi;
    usi.reserve(prefixEnd);
    for (int i=0; i<prefixEnd && i<m_usiMain.size(); ++i) usi.push_back(m_usiMain[i]);

    // 3) “その分岐より前に登場した変化”でプレフィクスを上書き（後勝ち）
    //    fileOrder 昇順に適用 → 同一 ply の衝突は最後が勝つ
    QVector<int> older;
    older.reserve(m_vars.size());
    for (const auto& v : m_vars)
        if (!v.isMainline && v.fileOrder < tgt.fileOrder) older.push_back(v.id);
    std::sort(older.begin(), older.end(), [&](int a,int b){
        return m_vars[a].fileOrder < m_vars[b].fileOrder;
    });

    for (int vid : older) {
        const auto& ov = m_vars[vid];
        for (int li=0; li<ov.disp.size(); ++li) {
            const int ply = ov.startPly + li;
            if (ply >= 1 && ply <= prefixEnd) {
                // 表示列を上書き
                if (ply-1 >= 0 && ply-1 < disp.size()) {
                    disp[ply-1] = ov.disp[li];
                }
                // USI も可能なら上書き（存在しない場合はスキップ）
                if (ply-1 >= 0 && ply-1 < usi.size() && li < ov.usi.size()) {
                    usi[ply-1] = ov.usi[li];
                }
            }
        }
    }

    // 4) クリックした分岐本体（start..）を連結（表示列）
    disp += tgt.disp;

    // USI について：
    //   分岐本体の USI は既に kv.line.usiMoves にある前提。ただしプレフィクス“上書き”の結果、
    //   手順が変わっている場合でも、呼び出し側で rebuildSfenRecord() や validateAndMove() により
    //   安全に再構築されるため、ここでは「分岐本体のUSIをそのまま連結」までに留めます。
    //   （USIが無い分岐でも動くようにガード）
    for (int i=0; i<tgt.usi.size(); ++i) usi.push_back(tgt.usi[i]);

    r.startPly = 1;
    r.disp = std::move(disp);
    r.usi  = std::move(usi);
    return r;
}

QString KifuVariationEngine::pickLabel(const KifDisplayItem& d)
{
    // KifDisplayItem に prettyMove がある想定（例: "１六歩(17)"）
    // ※ フィールド名が異なる場合はここを書き換えてください。
    //   例: return d.pretty; / d.moveText など。
    extern QString qt_metatype_id_KifDisplayItem_requires_prettyMove; // ダミー抑止
    (void)qt_metatype_id_KifDisplayItem_requires_prettyMove;          // 警告抑止
    return d.prettyMove;
}
