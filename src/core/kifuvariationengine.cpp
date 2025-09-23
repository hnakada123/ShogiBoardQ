#include "kifuvariationengine.h"
#include "sfenpositiontracer.h"   // ★ これを必ず入れる（SFEN合成で使用）

#include <QElapsedTimer>
#include <QDebug>
#include <QSet>

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

    // 0) 本譜（id=0）
    {
        Variation v;
        v.id          = 0;
        v.fileOrder   = 0;
        v.isMainline  = true;
        v.startPly    = 1;
        v.disp        = dispMain;
        v.usi         = usiMain;
        v.sourceIndex = 0;
        v.sfen        = sfenMain;   // 0..N（開始局面含む）

        m_vars.push_back(v);
        m_dispMain = dispMain;
        m_usiMain  = usiMain;
    }

    // ユーティリティ：ある基準局面から USI を順に適用して SFEN 列を合成
    auto composeFromBase = [](const QString& base, const QVector<UsiMove>& usis) -> QList<QString> {
        QList<QString> out;
        if (base.isEmpty()) return out;
        SfenPositionTracer tr;
        if (!tr.setFromSfen(base)) return {};
        out.reserve(usis.size() + 1);
        out.push_back(tr.toSfenString());      // [0] = 基準（直前局面）
        for (int i = 0; i < usis.size(); ++i) {
            tr.applyUsiMove(usis.at(i));
            out.push_back(tr.toSfenString());  // [1..] 各手後
        }
        return out;
    };

    // 1) 変化群（id は 1 始まり）
    int nextId = 1;
    for (int i = 0; i < res.variations.size(); ++i) {
        const auto& kv = res.variations[i];

        Variation v;
        v.id          = nextId++;
        v.fileOrder   = i + 1;
        v.isMainline  = false;
        v.startPly    = kv.startPly;         // グローバル手数での開始
        v.disp        = kv.line.disp;
        v.usi         = kv.line.usiMoves;
        v.sourceIndex = i;

        // --- 変化の SFEN を用意 ---
        if (!kv.line.sfenList.isEmpty()) {
            // パーサが SFEN を持っているならそれを採用
            v.sfen = kv.line.sfenList;
        } else {
            // ★合成：globalPrev = startPly-1 の局面を「もっとも深い既存行」から取る
            const int globalPrev = v.startPly - 1;
            QString basePrev;
            int baseFromVid = -1;
            int baseIdx     = -1;

            // 既に構築済みの行（本譜=先頭, 変化=その後）を「逆順」に走査して最深を選ぶ
            for (int p = m_vars.size() - 1; p >= 0; --p) {
                const auto& par = m_vars[p];
                if (par.isMainline) {
                    // 本譜は sfen[globalPrev] がそのまま「直前局面」
                    if (globalPrev >= 0 && globalPrev < par.sfen.size()) {
                        basePrev   = par.sfen.at(globalPrev);
                        baseFromVid= par.id;              // 0
                        baseIdx    = globalPrev;
                        break;
                    }
                } else {
                    // 変化 p の sfen[k] は「グローバル (p.startPly + k - 1) 手目後」を表す
                    // よって globalPrev に対応するインデックスは：
                    const int k = globalPrev - par.startPly + 1; // 0..disp.size()
                    if (k >= 0 && k < par.sfen.size()) {
                        basePrev   = par.sfen.at(k);
                        baseFromVid= par.id;
                        baseIdx    = k;
                        break;
                    }
                }
            }

            if (!basePrev.isEmpty()) {
                v.sfen = composeFromBase(basePrev, v.usi);
                qDebug().noquote().nospace()
                    << "[VE] compose var id=" << v.id
                    << " start=" << v.startPly
                    << " base.fromVid=" << baseFromVid
                    << " base.idx=" << baseIdx
                    << " out.len=" << v.sfen.size();
            } else {
                // フォールバック：本譜から（理論上ここはほぼ通らない想定）
                if (!m_vars.isEmpty() && !m_vars[0].sfen.isEmpty()) {
                    const auto& mainSfen = m_vars[0].sfen;
                    if (globalPrev >= 0 && globalPrev < mainSfen.size()) {
                        v.sfen = composeFromBase(mainSfen.at(globalPrev), v.usi);
                        qWarning().noquote()
                            << "[VE] compose fallback from MAIN at prev=" << globalPrev
                            << " var=" << v.id;
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

    // デバッグ：索引ダンプ
    {
        QStringList lines;
        for (auto it = m_idx.constBegin(); it != m_idx.constEnd(); ++it) {
            lines << QString("  ply %1 -> count %2").arg(it.key()).arg(it.value().size());
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

QString KifuVariationEngine::prevSfenFor(int variationId, int li) const
{
    // variationId に対応する Variation を探す
    const Variation* var = nullptr;
    for (const auto& v : m_vars) {
        if (v.id == variationId) { var = &v; break; }
    }
    if (!var) return {};

    // li は「この variation のローカル手インデックス」
    // prev SFEN は「その手を指す直前の局面」＝ var->sfen[li]
    if (li < 0 || li >= var->sfen.size()) return {};
    return var->sfen.at(li);
}

QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply,
                                            bool includeMainline,
                                            const QString& ctxPrevSfen) const
{
    QList<BranchCandidate> out;

    auto it = m_idx.find(ply);
    if (it == m_idx.end()) return out;

    const auto& pairs = it.value();
    QSet<QString> seen;

    for (int i = 0; i < pairs.size(); ++i) {
        const int vid = pairs[i].first;
        const int li  = pairs[i].second;

        // 該当 variation
        const Variation* var = nullptr;
        for (const auto& v : m_vars) if (v.id == vid) { var = &v; break; }
        if (!var) continue;

        // 本譜を出す/出さない
        if (!includeMainline && var->isMainline) continue;

        // ★ 直前局面：本譜/変化を問わず variation 側の SFEN から取る
        const QString prev = prevSfenFor(vid, li);

        // 文脈SFENが指定されていれば一致するものだけ通す
        if (!ctxPrevSfen.isEmpty()) {
            if (prev.isEmpty() || prev != ctxPrevSfen) continue;
        }

        // ラベル（この variation の表示配列から）
        QString label;
        if (li >= 0 && li < var->disp.size()) {
            label = pickLabel(var->disp.at(li));   // クラス内の静的/プライベートでOK
        }
        if (label.isEmpty()) continue;

        if (seen.contains(label)) continue;
        seen.insert(label);

        BranchCandidate c;
        c.label       = label;
        c.variationId = vid;
        c.ply         = ply;
        c.isMainline  = var->isMainline;
        c.isBlack     = (ply % 2 == 1);
        out.push_back(std::move(c));

        // デバッグ
        qDebug().noquote().nospace()
            << "  [add] idx=" << i
            << " vid=" << vid
            << " li=" << li
            << " start=" << var->startPly
            << " label=\"" << label << "\""
            << " isMain=" << (var->isMainline ? "true" : "false")
            << " prev(cand)=" << (prev.isEmpty() ? "<EMPTY>" : prev)
            << (ctxPrevSfen.isEmpty() ? "" :
                    QString("  prev(ctx)=%1").arg(ctxPrevSfen));
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

int KifuVariationEngine::idForSourceIndex(int sourceIndex) const {
    return m_sourceToId.value(sourceIndex, -1);
}

QList<QString> KifuVariationEngine::mainlineSfen() const
{
    return m_vars.isEmpty() ? QList<QString>() : m_vars.front().sfen;
}
QList<QString> KifuVariationEngine::sfenForVariationId(int variationId) const
{
    for (const auto& v : m_vars) if (v.id == variationId) return v.sfen;
    return {};
}
int KifuVariationEngine::variationIdFromSourceIndex(int sourceIndex) const
{
    return m_sourceToId.value(sourceIndex, -1);
}
