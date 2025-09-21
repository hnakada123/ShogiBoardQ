#include "kifuvariationengine.h"
#include <algorithm>
#include <QElapsedTimer>

// ---- 依存する既存型の前提 ----
// KifDisplayItem が public メンバ prettyMove (QString) を持つ前提。
// 必要ならここをあなたの定義に合わせて変更してください。

// ============ ユーティリティ ============
static inline bool isBlackOnPly(int ply) { return (ply % 2) == 1; }

// ============ 実装 ============
KifuVariationEngine::KifuVariationEngine() {}

void KifuVariationEngine::ingest(const KifParseResult& res,
                                 const QList<QString>& sfenMain,   // ←使う
                                 const QVector<UsiMove>& usiMain,
                                 const QList<KifDisplayItem>& dispMain)
{
    qDebug() << "[VE] ingest() begin";
    m_dispMain = dispMain;
    m_usiMain  = usiMain;
    m_vars.clear();
    m_idx.clear();
    m_sourceToId.clear();

    // --- 本譜 ---
    {
        Variation v;
        v.id = 0; v.fileOrder = 0; v.isMainline = true;
        v.startPly = 1;
        v.disp = dispMain; v.usi = usiMain;
        v.sourceIndex = -1;
        v.sfen = sfenMain;  // ★本譜のSFENを丸ごと保持（0=初期局面）

        m_vars.push_back(v);
        for (int li=0; li<v.disp.size(); ++li) {
            const int ply = v.startPly + li;
            m_idx[ply].push_back({v.id, li});
        }
        qDebug().nospace() << "[VE] mainline: moves=" << v.disp.size()
                           << " sfen=" << v.sfen.size();
    }

    // --- 変化 ---
    int order = 1, nextId = 1;
    for (int vi = 0; vi < res.variations.size(); ++vi) {
        const KifVariation& kv = res.variations.at(vi);
        if (kv.line.disp.isEmpty()) continue;

        Variation v;
        v.id = nextId++;
        v.fileOrder = order++;
        v.isMainline = false;
        v.startPly = kv.startPly;
        v.disp = kv.line.disp;
        v.usi  = kv.line.usiMoves;
        v.sourceIndex = vi;
        v.sfen = kv.line.sfenList;        // ★想定： [0]=基底, [1..]=各手後

        if (v.sfen.isEmpty()) {
            qDebug().noquote()
            << "[VE][WARN] variation id=" << v.id
            << " has empty sfenList. Context filter will be limited.";
        }

        m_vars.push_back(v);
        m_sourceToId.insert(vi, v.id);

        for (int li=0; li<v.disp.size(); ++li) {
            const int ply = v.startPly + li;
            m_idx[ply].push_back({v.id, li});
        }
        qDebug().nospace()
            << "[VE] var id=" << v.id
            << " src=" << v.sourceIndex
            << " start=" << v.startPly
            << " len=" << v.disp.size()
            << " sfen=" << v.sfen.size();
    }

    // インデックス概要
    qDebug() << "[VE] ingest() built index:";
    for (auto it = m_idx.constBegin(); it != m_idx.constEnd(); ++it) {
        qDebug().nospace() << "  ply " << it.key() << " -> count " << it.value().size();
    }

    // ★ 任意：対応表も一目で分かるように吐く（デバッグ用）
    if (!m_sourceToId.isEmpty()) {
        qDebug() << "[VE] sourceIndex -> id map:";
        // 注意：QHash は順序がないので、キーでソートしたい場合は QList に取り出して sort
        QList<int> keys = m_sourceToId.keys();
        std::sort(keys.begin(), keys.end());
        for (int src : keys) {
            qDebug().nospace() << "  src " << src << " -> id " << m_sourceToId.value(src);
        }
    }
}

QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply,
                                            bool includeMainline,
                                            const QString& contextPrevSfen) const
{
    // INログ
    static int s_callSeq = 0;
    const int call = ++s_callSeq;
    QElapsedTimer t; t.start();
    auto H = [call]() { return QString("[VE] branchCandidatesForPly#%1").arg(call); };

    qDebug().noquote() << H() << " IN  ply=" << ply
                       << " includeMainline=" << includeMainline
                       << " vars=" << m_vars.size()
                       << " idx.size=" << m_idx.size()
                       << " ctxPrevSfen=" << (contextPrevSfen.isEmpty() ? "<EMPTY>" : contextPrevSfen);

    QList<BranchCandidate> out;

    const auto it = m_idx.constFind(ply);
    if (it == m_idx.constEnd()) {
        qDebug().noquote() << H() << " no index for ply -> 0";
        qDebug().noquote() << H() << " OUT size=0 elapsed=" << t.elapsed() << "ms";
        return out;
    }

    auto pairs = it.value(); // (vid, li)
    qDebug().noquote() << H() << " raw pairs=" << pairs.size();

    std::sort(pairs.begin(), pairs.end(), [&](auto a, auto b){
        const auto& va = m_vars[a.first];
        const auto& vb = m_vars[b.first];
        if (va.isMainline != vb.isMainline) return va.isMainline && !vb.isMainline;
        return va.fileOrder < vb.fileOrder;
    });

    QSet<QString> seen;
    int n = 0;
    for (auto [vid, li] : pairs) {
        if (vid < 0 || vid >= m_vars.size()) continue;
        const auto& v = m_vars[vid];

        // 本譜を含めない設定ならスキップ
        if (!includeMainline && v.isMainline) {
            qDebug().noquote() << "  skip mainline vid=" << vid << " li=" << li;
            continue;
        }

        // （ループ内）
        if (!contextPrevSfen.isEmpty()) {
            const QString prev = prevSfenFor(vid, li); // ← 直前局面SFENを取得
            if (prev != contextPrevSfen) {
                qDebug().noquote() << "  skip context-mismatch vid=" << vid
                                   << " li=" << li
                                   << " prev=" << (prev.isEmpty() ? "<EMPTY>" : prev);
                continue;
            }
        }

        // ★文脈フィルタ：直前局面が一致するものだけ通す
        QString prev = prevSfenFor(vid, li);
        if (!contextPrevSfen.isEmpty() && !prev.isEmpty() && prev != contextPrevSfen) {
            qDebug().noquote() << "  skip context-mismatch vid=" << vid
                               << " li=" << li
                               << " prev=" << prev;
            continue;
        }

        if (li < 0 || li >= v.disp.size()) continue;
        const auto& d = v.disp[li];

        BranchCandidate c;
        c.label       = pickLabel(d);
        c.variationId = vid;
        c.ply         = ply;
        c.isBlack     = isBlackOnPly(ply);
        c.isMainline  = v.isMainline;

        if (seen.contains(c.label)) {
            qDebug().noquote() << "  drop dup label=" << c.label;
            continue;
        }
        seen.insert(c.label);
        out.push_back(c);

        qDebug().noquote()
            << "  [add] idx=" << (n++)
            << " vid=" << vid
            << " li="  << li
            << " start=" << v.startPly
            << " label=\"" << d.prettyMove << "\""
            << " isMain=" << v.isMainline
            << " prevSfen=" << (prev.isEmpty() ? "<none>" : prev);
    }

    qDebug().noquote() << H() << " OUT size=" << out.size()
                       << " elapsed=" << t.elapsed() << "ms";
    for (int i = 0; i < out.size(); ++i) {
        const auto& c = out[i];
        qDebug().noquote()
            << "  [out] " << i
            << ": label=\"" << c.label << "\""
            << " vid=" << c.variationId
            << " ply=" << c.ply
            << " isMain=" << c.isMainline
            << " isBlack=" << c.isBlack;
    }

    return out;
}

// 既存シグネチャは後方互換で残し、フィルタなし（従来挙動）に委譲
QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply, bool includeMainline) const
{
    return branchCandidatesForPly(ply, includeMainline, QString{});
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

// vid の li（0ベース手インデックス）を指す直前局面（ply-1）を返す。
// main: sfen[li]
// var : sfen[li]（sfen[0]が startPly-1 の基底）
QString KifuVariationEngine::prevSfenFor(int vid, int li) const
{
    if (vid < 0 || vid >= m_vars.size()) return QString();
    const auto& v = m_vars[vid];
    if (li < 0) return QString();
    if (v.isMainline) {
        // 本譜: sfen[0]=初期局面、li手目の直前＝sfen[li]
        if (li < v.sfen.size()) return v.sfen.at(li);
    } else {
        // 変化: sfen[0]=startPly-1 の局面、li手目の直前＝sfen[li]
        if (li < v.sfen.size()) return v.sfen.at(li);
    }
    return QString();
}
