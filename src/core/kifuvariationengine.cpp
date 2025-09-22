#include "kifuvariationengine.h"
#include "sfenpositiontracer.h"   // ★ これを必ず入れる（SFEN合成で使用）

#include <QElapsedTimer>
#include <QDebug>
#include <QSet>

#include <algorithm>

// ヘルパ：ラベルは prettyMove をそのまま使う（必要ならトリム）
QString KifuVariationEngine::pickLabel(const KifDisplayItem& d) {
    return d.prettyMove.trimmed();
}

KifuVariationEngine::KifuVariationEngine() = default;

void KifuVariationEngine::ingest(const KifParseResult& res,
                                 const QList<QString>& sfenMain,
                                 const QVector<UsiMove>& usiMain,
                                 const QList<KifDisplayItem>& dispMain)
{
    QElapsedTimer t; t.start();
    qDebug() << "[VE] ingest() begin";

    // 既存データをクリア
    m_dispMain.clear();
    m_usiMain.clear();
    m_vars.clear();
    m_idx.clear();
    m_sourceToId.clear();

    // 0) 本譜を id=0 で登録
    {
        Variation v;
        v.id          = 0;
        v.fileOrder   = 0;
        v.isMainline  = true;
        v.startPly    = 1;          // 本譜は 1 手目開始
        v.disp        = dispMain;
        v.usi         = usiMain;
        v.sourceIndex = 0;
        v.sfen        = sfenMain;   // 0..N （開始局面含む）

        m_vars.push_back(v);
        m_dispMain = dispMain;
        m_usiMain  = usiMain;
    }

    // 1) 変化群（id は 1 始まり）
    int nextId = 1;
    for (int i = 0; i < res.variations.size(); ++i) {
        const auto& kv = res.variations[i];

        Variation v;
        v.id          = nextId++;
        v.fileOrder   = i + 1;
        v.isMainline  = false;
        v.startPly    = kv.startPly;
        v.disp        = kv.line.disp;
        v.usi         = kv.line.usiMoves;
        v.sourceIndex = i;

        // 変化の SFEN：ファイルに入っていればそれを使用。空なら合成する。
        if (!kv.line.sfenList.isEmpty()) {
            v.sfen = kv.line.sfenList;
        } else {
            // 合成：本譜の (startPly-1) 局面から v.usi を順に適用して作る
            if (!m_vars.isEmpty() && !m_vars[0].sfen.isEmpty()) {
                const QList<QString>& mainSfen = m_vars[0].sfen;
                const int baseIdx = v.startPly - 1;
                if (baseIdx >= 0 && baseIdx < mainSfen.size()) {
                    SfenPositionTracer tr;
                    if (tr.setFromSfen(mainSfen.at(baseIdx))) {
                        QList<QString> list;
                        list.reserve(v.usi.size() + 1);
                        list.push_back(tr.toSfenString());   // [0] = 直前局面（startPly-1）
                        for (int li = 0; li < v.usi.size(); ++li) {
                            tr.applyUsiMove(v.usi.at(li));
                            list.push_back(tr.toSfenString()); // [1..] 各手後
                        }
                        v.sfen = list;
                    }
                }
            }
        }

        m_sourceToId.insert(i, v.id);
        m_vars.push_back(v);

        qDebug() << "[VE] var id=" << v.id
                 << "src=" << v.sourceIndex
                 << "start=" << v.startPly
                 << "len=" << v.disp.size()
                 << "sfen=" << v.sfen.size();
    }

    // 2) ply -> (variationId, localIndex) の索引を構築
    auto addIndex = [&](int ply, int vid, int li) {
        m_idx[ply].push_back(qMakePair(vid, li));
    };

    // 本譜（id=0）：ply = li + 1
    if (!m_vars.isEmpty()) {
        const auto& main = m_vars[0];
        for (int li = 0; li < main.disp.size(); ++li) {
            addIndex(li + 1, 0, li);
        }
    }

    // 変化：ply = startPly + li
    for (int vpos = 1; vpos < m_vars.size(); ++vpos) {
        const auto& v = m_vars[vpos];
        for (int li = 0; li < v.disp.size(); ++li) {
            addIndex(v.startPly + li, v.id, li);
        }
    }

    // デバッグ
    {
        QStringList lines;
        for (auto it = m_idx.constBegin(); it != m_idx.constEnd(); ++it) {
            lines << QString("  ply %1 -> count %2")
            .arg(it.key()).arg(it.value().size());
        }
        lines.sort();
        qDebug().noquote() << "[VE] ingest() built index:\n " << lines.join("\n  ");
    }

    qDebug() << "[VE] ingest() end elapsed(ms)=" << t.elapsed();
}

// 後勝ちで一本化：本譜をベースに、variationId の手を startPly-1 から上書き（足りなければ末尾に追記）
ResolvedLine KifuVariationEngine::resolveAfterWins(int variationId) const
{
    ResolvedLine rl;
    rl.startPly = 1;
    rl.disp = m_dispMain;  // 本譜をベース
    rl.usi  = m_usiMain;

    // 対象 variation を検索
    const Variation* vptr = nullptr;
    for (const auto& v : m_vars) {
        if (v.id == variationId) { vptr = &v; break; }
    }
    if (!vptr || vptr->isMainline) {
        // 見つからない or 本譜(id=0)なら、そのまま本譜を返す
        return rl;
    }
    const Variation& v = *vptr;

    const int base = qMax(0, v.startPly - 1);   // 上書き開始位置（0-based）

    // 表示（disp）を後勝ちで上書き／追記
    for (int li = 0; li < v.disp.size(); ++li) {
        const int idx = base + li;
        if (idx < rl.disp.size()) rl.disp[idx] = v.disp.at(li);
        else                      rl.disp.push_back(v.disp.at(li));
    }

    // USI も同様に上書き／追記
    for (int li = 0; li < v.usi.size(); ++li) {
        const int idx = base + li;
        if (idx < rl.usi.size()) rl.usi[idx] = v.usi.at(li);
        else                     rl.usi.push_back(v.usi.at(li));
    }

    // （SFEN は呼び出し側で既存の rebuildSfenRecord() を用いて再構築してください）
    return rl;
}

// kifuvariationengine.cpp
#include "kifuvariationengine.h"
#include "sfenpositiontracer.h"
#include <QElapsedTimer>
#include <algorithm>

// 先後（奇数手=先手、偶数手=後手）
static inline bool isBlackOnPly(int ply) { return (ply % 2) == 1; }

// idからVariation参照を取る小ヘルパ
const KifuVariationEngine::Variation* findVarById(const QList<KifuVariationEngine::Variation>& vars, int vid) {
    for (const auto& v : vars) if (v.id == vid) return &v;
    return nullptr;
}

// ★この関数は既に定義済みならそのままでOK
QString KifuVariationEngine::prevSfenFor(int vid, int li) const
{
    const Variation* var = nullptr;
    for (const auto& v : m_vars) if (v.id == vid) { var = &v; break; }
    if (!var || var->sfen.isEmpty()) return QString();

    if (var->isMainline) {
        // 本譜の手 li の直前 = main.sfen[li]
        return (li >= 0 && li < var->sfen.size()) ? var->sfen.at(li) : QString();
    }

    // 変化の手 li の直前：
    //   li==0 → 基底（startPly-1後）= sfen[0]
    //   li>=1 → 変化内で (li-1) 手指した後 = sfen[li]
    const int idx = (li == 0) ? 0 : li;
    return (idx >= 0 && idx < var->sfen.size()) ? var->sfen.at(idx) : QString();
}

QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply,
                                            bool includeMainline,
                                            const QString& ctxPrevSfen) const
{
    QList<BranchCandidate> out;

    auto it = m_idx.find(ply);
    if (it == m_idx.end()) return out;

    const auto &pairs = it.value();
    QSet<QString> seen;                  // ラベル重複ガード
    const auto &main = m_vars.front();   // id=0

    for (int i = 0; i < pairs.size(); ++i) {
        const int vid = pairs[i].first;
        const int li  = pairs[i].second;

        // バリエーションを引く
        const Variation* var = nullptr;
        for (const auto& v : m_vars) if (v.id == vid) { var = &v; break; }
        if (!var) continue;

        // 本譜候補を含めない指定ならスキップ
        if (!includeMainline && var->isMainline) continue;

        // 直前局面を算出
        QString prev =
            var->isMainline
                ? ((li >= 0 && li < main.sfen.size()) ? main.sfen.at(li) : QString())
                : prevSfenFor(vid, li);

        // 文脈があるなら一致チェック（≠なら弾く）
        if (!ctxPrevSfen.isEmpty()) {
            if (prev.isEmpty() || prev != ctxPrevSfen)
                continue;
        }

        // 表示ラベル
        const QString label =
            (li >= 0 && li < var->disp.size()) ? pickLabel(var->disp.at(li))
                                               : QString();
        if (label.isEmpty()) continue;

        // 既に同ラベルが出ていればスキップ（安全側）
        if (seen.contains(label)) continue;
        seen.insert(label);

        // 追加
        BranchCandidate c;
        c.label      = label;
        c.variationId= vid;
        c.ply        = ply;
        c.isMainline = var->isMainline;
        c.isBlack    = (ply % 2 == 1);       // （先手手番=奇数手）

        out.push_back(std::move(c));

        // （任意）デバッグ
        qDebug().noquote()
            << QString("  [add] idx=%1 vid=%2 li=%3 start=%4 label=\"%5\" isMain=%6 prevSfen=%7")
                   .arg(i).arg(vid).arg(li).arg(var->startPly).arg(c.label)
                   .arg(var->isMainline ? "true" : "false")
            << (prev.isEmpty() ? "<EMPTY>" : prev.left(60) + "...");
    }

    return out;
}

// ★ 2引数版（既存互換）: 本譜の前局面SFENを自動採用
QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply, bool includeMainline) const
{
    QString ctx;
    if (ply > 0 && !m_vars.isEmpty()) {
        const auto &main = m_vars.front();          // id=0 の本譜
        const int idx = ply - 1;                    // 「直前」の局面
        if (idx >= 0 && idx < main.sfen.size())
            ctx = main.sfen.at(idx);
    }
    return branchCandidatesForPly(ply, includeMainline, ctx); // ← 3引数版へ
}
