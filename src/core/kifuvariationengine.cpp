#include "kifuvariationengine.h"

#include <QElapsedTimer>
#include <QDebug>
#include <QHash>
#include <QPair>
#include <QVector>
#include <QList>
#include <QtGlobal>

// ------------------------------------------------------------
// KifuVariationEngine
// ------------------------------------------------------------

KifuVariationEngine::KifuVariationEngine() = default;

// ==== KifuVariationEngine 修正実装（該当関数 全コード）====

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

    // 0) メインライン（id=0）
    {
        Variation v;
        v.id          = 0;
        v.fileOrder   = 0;
        v.isMainline  = true;
        v.startPly    = 1;              // 本譜は常に 1 手目開始
        v.disp        = dispMain;
        v.usi         = usiMain;
        v.sourceIndex = 0;
        v.sfen        = sfenMain;       // ★ヘッダ定義どおり：sfenListではなく sfen

        m_vars.push_back(v);

        // 表示・USI のコピーも保持（必要に応じて利用）
        m_dispMain = dispMain;
        m_usiMain  = usiMain;
    }

    // 1) 変化群（id は 1 始まり）
    //    KifParseResult::variations は、添付コードの定義と一致
    //    kv.startPly, kv.line.disp, kv.line.usiMoves, kv.line.sfenList を使用
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
        v.sfen        = kv.line.sfenList;   // ★FIXME: 添付コードどおり sfen へ代入

        m_vars.push_back(v);

        // 「sourceIndex -> variationId」も控えておく（必要箇所で使う想定）
        m_sourceToId.insert(i, v.id);
    }

    // 2) ply -> (variationId, localIndex) の逆引きインデックスを構築
    auto addIndex = [&](int ply, int vid, int li) {
        // ヘッダ定義: QHash<int, QVector<QPair<int,int>>> m_idx;
        //            key=ply, value=(variationId, localIndex) の配列
        m_idx[ply].push_back(qMakePair(vid, li));
    };

    // 本譜（id=0）：ply = li + 1
    {
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
        qDebug() << "[VE] var id=" << v.id
                 << "src=" << v.sourceIndex
                 << "start=" << v.startPly
                 << "len=" << v.disp.size()
                 << "sfen=" << v.sfen.size();
    }

    // デバッグ表示
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

QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply,
                                            bool includeMainline,
                                            const QString& contextPrevSfen) const
{
    auto H = [&](){ return QString("[VE] branchCandidatesForPly"); };
    QElapsedTimer t; t.start();

    qDebug().noquote() << H() << " IN  ply=" << ply
                       << " includeMainline=" << includeMainline
                       << " vars=" << m_vars.size()
                       << " idx.size=" << m_idx.size()
                       << " ctxPrevSfen=" << (contextPrevSfen.isEmpty() ? "<EMPTY>" : "<...>");

    QList<BranchCandidate> out;

    // インデックスに該当 ply がなければ終了
    const auto it = m_idx.constFind(ply);
    if (it == m_idx.constEnd()) {
        qDebug().noquote() << H() << "  no index for ply ->" << ply;
        qDebug().noquote() << H() << "  OUT size=0 elapsed=" << t.elapsed() << "ms";
        return out;
    }

    const auto& pairs = it.value(); // QVector<QPair<vid, li>>
    qDebug().noquote() << H() << "  raw pairs=" << pairs.size();

    // SFEN コンテキストフィルタ
    auto passCtx = [&](int vid, int li) -> bool {
        if (contextPrevSfen.isEmpty()) return true;
        const QString prev = prevSfenFor(vid, li);   // ★ヘッダ宣言のヘルパーを利用
        return (!prev.isEmpty() && prev == contextPrevSfen);
    };

    // メインラインを含めないならフィルタ
    auto passMain = [&](int vid) -> bool {
        if (includeMainline) return true;
        return (vid != 0);
    };

    // ダブり防止（同じラベルを重複出力しない）
    QSet<QString> seen;

    int n = 0;
    for (const auto& p : pairs) {
        const int vid = p.first;
        const int li  = p.second;

        if (!passMain(vid)) continue;
        if (vid < 0 || vid >= m_vars.size()) continue;
        const auto& v = m_vars[vid];
        if (li < 0 || li >= v.disp.size()) continue;

        if (!passCtx(vid, li)) continue;

        const auto& d = v.disp.at(li);

        BranchCandidate c;
        c.label       = d.prettyMove;  // ★FIXME: 添付の表示ラベル
        c.variationId = vid;
        c.ply         = ply;
        c.isBlack     = ((ply % 2) == 1);  // 先手=奇数手番

        // 同一ラベルが既に出ていればスキップ
        if (seen.contains(c.label)) {
            continue;
        }
        seen.insert(c.label);
        out.push_back(c);

        const QString prev = prevSfenFor(vid, li);
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
    return out;
}

QList<BranchCandidate>
KifuVariationEngine::branchCandidatesForPly(int ply, bool includeMainline) const
{
    // デフォルトは「SFENコンテキストなし」で委譲
    return branchCandidatesForPly(ply, includeMainline, QString{});
}

// ---------------------------------------------------------------------
// 「勝ちになった分岐」を本譜先頭から並べ直した 1 本の ResolvedLine に変換する
//   - 先頭は常に本譜（id=0）の手順
//   - ただし、分岐の開始手までの区間は、選んだ分岐の“実表示”で上書き（非本質：ラベル統一）
//   - 以降は分岐本体の disp / usi を順に連結
// ---------------------------------------------------------------------
ResolvedLine KifuVariationEngine::resolveAfterWins(int variationId) const
{
    ResolvedLine r;

    if (variationId < 0 || variationId >= m_vars.size()) {
        // 不正 id：空で返す
        r.startPly = 1;
        r.disp.clear();
        r.usi.clear();
        return r;
    }

    const auto& tgt = m_vars[variationId];

    // 1) 先頭は必ず本譜（id=0）の表示列をベースに
    QVector<KifDisplayItem> disp = m_dispMain;
    QVector<UsiMove>        usi  = m_usiMain;

    // 2) 分岐の開始手直前までの区間（1..startPly-1）
    //    ※ std::min の型を合わせる（disp.size() は qsizetype ）
    const int prefixEnd = std::min<int>(tgt.startPly - 1, static_cast<int>(disp.size()));

    // 3) 同一 startPly の分岐が持つ「表示」および USI で、可能な範囲のプレフィクスを上書き
    for (int vpos = 1; vpos < m_vars.size(); ++vpos) {
        const auto& ov = m_vars[vpos];
        if (ov.isMainline) continue;
        if (ov.startPly != tgt.startPly) continue;

        for (int li = 0; li < ov.disp.size(); ++li) {
            const int ply = ov.startPly + li;
            if (ply < 1 || ply > prefixEnd) break;

            // 表示の上書き
            if (ply - 1 >= 0 && ply - 1 < disp.size()) {
                disp[ply - 1] = ov.disp[li];
            }
            // USI も存在する範囲で上書き
            if (ply - 1 >= 0 && ply - 1 < usi.size() && li < ov.usi.size()) {
                usi[ply - 1] = ov.usi[li];
            }
        }
    }

    // 4) 分岐本体を連結（表示は必ず分岐の表示を採用）
    disp += tgt.disp;

    // USI は存在する分だけ連結（呼び出し側で再構築/検証が入る前提）
    for (int i = 0; i < tgt.usi.size(); ++i) {
        usi.push_back(tgt.usi[i]);
    }

    r.startPly = 1;
    r.disp     = std::move(disp);
    r.usi      = std::move(usi);
    return r;
}



// ---------------------------------------------------------------------
// 指定 (vid, li) の「直前局面」を SFEN で返すヘルパー
//   - 本譜:  sfen[0] が初期局面。li 手目の直前は sfen[li]
//   - 変化:  sfen[0] が (startPly-1) の局面。li 手目の直前は sfen[li]
// ---------------------------------------------------------------------
QString KifuVariationEngine::prevSfenFor(int vid, int li) const
{
    if (vid < 0 || vid >= m_vars.size()) return QString();
    const auto& v = m_vars[vid];
    if (li < 0) return QString();
    if (li < v.sfen.size()) {
        return v.sfen.at(li);
    }
    return QString();
}
