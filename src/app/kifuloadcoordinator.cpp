#include "kifuloadcoordinator.h"
#include "kiftosfenconverter.h"
#include "sfenpositiontracer.h"
#include "recordpane.h"              // これまで通り（前方宣言側）
#include "kifurecordlistmodel.h"     // ← 実体定義（必須）
#include "kifubranchlistmodel.h"     // ← 実体定義（必須）
#include "branchdisplayplan.h"
#include "navigationpresenter.h"
#include "engineanalysistab.h"
#include "csatosfenconverter.h"

#include <QDebug>
#include <QStyledItemDelegate>
#include <QAbstractItemView>         // view->model() を使うなら
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QTableWidget>
#include <QPainter>
#include <QFileInfo>

// デバッグのオン/オフ（必要に応じて false に）
static bool kGM_VERBOSE = true;

static inline QString pickLabelForDisp(const KifDisplayItem& d)
{
    return d.prettyMove;
}

inline int rankLetterToNum(QChar r) { // 'a'..'i' -> 1..9
    ushort u = r.toLower().unicode();
    return (u < 'a' || u > 'i') ? -1 : int(u - 'a') + 1;
}

inline QChar dropLetterWithSide(QChar upper, bool black) {
    return black ? upper.toUpper() : upper.toLower();
}

// 成駒トークン("+P"等) -> 1文字表現。非成駒はトークンそのまま1文字。
inline QChar tokenToOneChar(const QString& tok) {
    if (tok.isEmpty()) return QLatin1Char(' ');
    if (tok.size() == 1) return tok.at(0);
    static const QHash<QString,QChar> map = {
                                              {"+P",'Q'},{"+L",'M'},{"+N",'O'},{"+S",'T'},{"+B",'C'},{"+R",'U'},
                                              {"+p",'q'},{"+l",'m'},{"+n",'o'},{"+s",'t'},{"+b",'c'},{"+r",'u'},
                                              };
    const auto it = map.find(tok);
    return it == map.end() ? QLatin1Char(' ') : *it;
}

// ★打ちの fromSquare を駒台座標にマップ
inline QPoint dropFromSquare(QChar dropUpper, bool black) {
    const int x = black ? 9 : 10; // 先手=9, 後手=10
    int y = -1;
    switch (dropUpper.toUpper().unicode()) {
    case 'P': y = black ? 0 : 8; break;
    case 'L': y = black ? 1 : 7; break;
    case 'N': y = black ? 2 : 6; break;
    case 'S': y = black ? 3 : 5; break;
    case 'G': y = 4;            break; // 共通
    case 'B': y = black ? 5 : 3; break;
    case 'R': y = black ? 6 : 2; break;
    default:  y = -1;           break;
    }
    return QPoint(x, y);
}

static inline QString lineNameForRow(int row) {
    return (row == 0) ? QStringLiteral("Main") : QStringLiteral("Var%1").arg(row - 1);
}

// 1セルを "file-rank" 表示（USI基準とL2R基準の両方を出す）
static inline QString idxHuman(int idx) {
    const int col = idx % 9;       // 0..8   左→右
    const int row = idx / 9;       // 0..8   上→下
    const int fileL2R = col + 1;   // 1..9   左→右
    const int rankTop = row + 1;   // 1..9   上→下
    const int fileUSI = 9 - col;   // 9..1   右→左（一般的なUSIの筋）
    const int rankUSI = 9 - row;   // 9..1   下→上（一般的なUSIの段）
    return QStringLiteral("[idx=%1 L2R(%2,%3) USI(%4,%5)]")
        .arg(idx).arg(fileL2R).arg(rankTop).arg(fileUSI).arg(rankUSI);
}

// 盤面(81マス)をトークン列に展開（空は ""。駒は "P","p","+P" のように '+' 付きも保持）
static QVector<QString> sfenBoardTo81Tokens(const QString& sfen)
{
    const QString board = sfen.section(QLatin1Char(' '), 0, 0); // 盤面部分
    QVector<QString> cells; cells.reserve(81);

    for (int i = 0; i < board.size() && cells.size() < 81; ++i) {
        const QChar ch = board.at(i);
        if (ch == QLatin1Char('/')) continue;
        if (ch.isDigit()) {
            const int n = ch.digitValue();
            for (int k = 0; k < n; ++k) cells.push_back(QString()); // 空マス
            continue;
        }
        if (ch == QLatin1Char('+')) {
            if (i + 1 < board.size()) {
                cells.push_back(QStringLiteral("+") + board.at(i + 1));
                ++i;
            }
            continue;
        }
        // 通常駒1文字
        cells.push_back(QString(ch));
    }
    while (cells.size() < 81) cells.push_back(QString());

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] sfenBoardTo81Tokens parsed"
                           << " len=" << cells.size()
                           << " board=\"" << board << "\"";
    }
    return cells;
}

static inline bool tokenEmpty(const QString& t) { return t.isEmpty(); }
static inline bool tokenPromoted(const QString& t) { return (!t.isEmpty() && t.at(0) == QLatin1Char('+')); }
static inline QChar tokenBasePiece(const QString& t) {
    if (t.isEmpty()) return QChar();
    return t.back(); // '+P' → 'P', 'p' → 'p'
}

// SFENペアから 1手分の ShogiMove を復元（差分なし=終局などは false）
static bool deriveMoveFromSfenPair(const QString& prevSfen,
                                   const QString& nextSfen,
                                   ShogiMove* out)
{
    const QVector<QString> A = sfenBoardTo81Tokens(prevSfen);
    const QVector<QString> B = sfenBoardTo81Tokens(nextSfen);

    int fromIdx = -1, toIdx = -1;
    QString fromTok, toTokPrev, toTokNew;

    // どのセルが変わったか（詳細ログ用）
    QVector<int> diffs; diffs.reserve(4);

    for (int i = 0; i < 81; ++i) {
        const QString& a = A.at(i);
        const QString& b = B.at(i);
        if (a == b) continue;

        diffs.push_back(i);

        // 移動元：a に駒があり b が空
        if (!tokenEmpty(a) && tokenEmpty(b)) {
            fromIdx = i;
            fromTok = a;
            continue;
        }
        // 移動先：b に駒があり a と異なる
        if (!tokenEmpty(b) && a != b) {
            toIdx     = i;
            toTokNew  = b;
            toTokPrev = a; // 取りがあった場合は a に相手駒がいた
        }
    }

    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] deriveMoveFromSfenPair  diffs=" << diffs.size();
        for (int i = 0; i < diffs.size(); ++i) {
            const int d = diffs.at(i);
            qDebug().noquote()
                << "      diff[" << i << "] idx=" << d
                << "  A=\"" << A.at(d) << "\""
                << "  B=\"" << B.at(d) << "\""
                << "  " << idxHuman(d);
        }
        qDebug().noquote() << "      picked fromIdx=" << fromIdx
                           << (fromIdx>=0 ? (" tok=\""+fromTok+"\" "+idxHuman(fromIdx)) : QString())
                           << "  toIdx=" << toIdx
                           << (toIdx>=0 ? (" tokPrev=\""+toTokPrev+"\" tokNew=\""+toTokNew+"\" "+idxHuman(toIdx)) : QString());
    }

    if (fromIdx < 0 && toIdx < 0) {
        // 盤が全く同じ → 投了など「着手なし」
        if (kGM_VERBOSE) qDebug() << "[GM] no board delta (resign/terminal/comment only)";
        return false;
    }

    // 盤座標 ← idx
    auto idxToPointL2R     = [](int idx)->QPoint { return QPoint(idx % 9, idx / 9); };
    auto idxToPointFlipped = [](int idx)->QPoint { return QPoint(8 - (idx % 9), idx / 9); };

    // 出力フィールド（※最終的に out へ入れるのは FLIP 側）
    QPoint from(-1, -1), to(-1, -1);
    QChar moving, captured;
    bool isPromotion = false;

    if (fromIdx < 0 && toIdx >= 0) {
        // 打つ手（ドロップ）
        from = QPoint(-1, -1);
        // ★ 採用は FLIP 側
        to   = idxToPointFlipped(toIdx);
        moving    = tokenBasePiece(toTokNew);              // 駒台から打った駒
        captured  = QChar();                               // 取りは無い
        isPromotion = false;                               // 打ちは成りなし
    } else if (fromIdx >= 0 && toIdx >= 0) {
        // 通常移動（★ 採用は FLIP 側）
        from     = idxToPointFlipped(fromIdx);
        to       = idxToPointFlipped(toIdx);
        moving   = tokenBasePiece(fromTok);               // 元の升の駒（非成り形）
        captured = tokenEmpty(toTokPrev) ? QChar() : tokenBasePiece(toTokPrev);
        isPromotion = tokenPromoted(toTokNew);
    } else {
        if (kGM_VERBOSE) qDebug() << "[GM] inconsistent from/to detection";
        return false;
    }

    if (kGM_VERBOSE) {
        // 参考ログ：L2R と FLIP の両方を出す（採用は FLIP）
        QPoint l2rFrom(-1,-1), l2rTo(-1,-1);
        if (fromIdx >= 0) l2rFrom = idxToPointL2R(fromIdx);
        if (toIdx   >= 0) l2rTo   = idxToPointL2R(toIdx);

        QPoint flipFrom(-1,-1), flipTo(-1,-1);
        if (fromIdx >= 0) flipFrom = idxToPointFlipped(fromIdx);
        if (toIdx   >= 0) flipTo   = idxToPointFlipped(toIdx);

        qDebug().noquote()
            << "      L2R  from=" << l2rFrom << " to=" << l2rTo;
        qDebug().noquote()
            << "      FLIP from=" << flipFrom << " to=" << flipTo << "  <-- chosen";

        qDebug().noquote()
            << "      moving=" << moving
            << " captured=" << (captured.isNull() ? QChar(' ') : captured)
            << " promoted=" << (isPromotion ? "T" : "F");
    }

    if (out) *out = ShogiMove(from, to, moving, captured, isPromotion);
    return true;
}

using BCDI = ::BranchCandidateDisplayItem;

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves,
                                         QVector<ResolvedRow>& resolvedRows,
                                         QStringList& positionStrList,
                                         int& activeResolvedRow,
                                         int& activePly,
                                         int& currentSelectedPly,
                                         int& currentMoveIndex,
                                         QStringList* sfenRecord,
                                         QTableWidget* gameInfoTable,
                                         QDockWidget* gameInfoDock,
                                         EngineAnalysisTab* analysisTab,
                                         QTabWidget* tab,
                                         ShogiView* shogiView,
                                         RecordPane* recordPane,
                                         KifuRecordListModel* kifuRecordModel,
                                         KifuBranchListModel* kifuBranchModel,
                                         BranchCandidatesController* branchCtl,
                                         QTableView* kifuBranchView,
                                         QHash<int, QMap<int, ::BranchCandidateDisplay>>& branchDisplayPlan,
                                         QObject* parent)
    : QObject(parent)
    , m_gameMoves(gameMoves)                // ← 参照メンバに束縛（同一実体を共有）
    , m_resolvedRows(resolvedRows)          // ← 同上
    , m_positionStrList(positionStrList)    // ← 同上
    , m_activeResolvedRow(activeResolvedRow)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex)
    , m_sfenRecord(sfenRecord)
    , m_gameInfoTable(gameInfoTable)
    , m_gameInfoDock(gameInfoDock)
    , m_analysisTab(analysisTab)
    , m_tab(tab)
    , m_shogiView(shogiView)
    , m_recordPane(recordPane)
    , m_kifuRecordModel(kifuRecordModel)
    , m_kifuBranchModel(kifuBranchModel)
    , m_branchCtl(branchCtl)
    , m_kifuBranchView(kifuBranchView)
    , m_branchDisplayPlan(branchDisplayPlan)
{
    // 必要ならデバッグ時にチェック
    // Q_ASSERT(m_sfenRecord && "sfenRecord must not be null");
    // ここで初期同期が必要ならシグナル発火や内部初期化を追加してください。
}

void KifuLoadCoordinator::loadCsaFromFile(const QString& filePath)
{
    // --- IN ログ ---
    qDebug().noquote() << "[MAIN] loadCsaFromFile IN file=" << filePath;

    // ★ ロード中フラグ（applyResolvedRowAndSelect 等の分岐更新を抑止）
    m_loadingKifu = true;

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    KifParseResult res;
    QString parseWarn;
    if (!CsaToSfenConverter::parse(filePath, res, &parseWarn)) {
        qWarning().noquote() << "[GM] CSA parse failed:" << filePath << parseWarn;
        m_loadingKifu = false;  // ★ 失敗時もフラグ解除しておく
        return;
    }
    if (!parseWarn.isEmpty()) {
        qWarning().noquote() << "[GM] CSA parse warn:" << parseWarn;
    }

    // resをデバッグ出力
    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] KifParseResult dump:";
        if (!parseWarn.isEmpty()) {
            qDebug().noquote() << "  [parseWarn]" << parseWarn;
        }

        qDebug().noquote() << "  Mainline:";
        qDebug().noquote() << "    baseSfen: " << res.mainline.baseSfen;
        qDebug().noquote() << "    usiMoves: " << res.mainline.usiMoves;
        qDebug().noquote() << "    disp:";
        int mainIdx = 0;
        for (const auto& d : std::as_const(res.mainline.disp)) {
            qDebug().noquote() << "      [" << mainIdx << "] prettyMove: " << d.prettyMove;
            if (!d.comment.isEmpty()) {
                qDebug().noquote() << "           comment: " << d.comment;
            } else {
                qDebug().noquote() << "           comment: <none>";
            }
            qDebug().noquote() << "           timeText: " << d.timeText;
            ++mainIdx;
        }
    }

    // 先手/後手名などヘッダ反映
    {
        const QList<KifGameInfoItem> infoItems = CsaToSfenConverter::extractGameInfo(filePath);
        populateGameInfo(infoItems);
        applyPlayersFromGameInfo(infoItems);
    }

    // KIF/CSA 共通の後処理に委譲
    applyParsedResultCommon_(filePath, initialSfen, teaiLabel,
                             res, parseWarn, "loadCsaFromFile");
}

void KifuLoadCoordinator::loadKifuFromFile(const QString& filePath)
{
    // --- IN ログ ---
    qDebug().noquote() << "[MAIN] loadKifuFromFile IN file=" << filePath;

    // ★ ロード中フラグ（applyResolvedRowAndSelect 等の分岐更新を抑止）
    m_loadingKifu = true;

    // 1) 初期局面（手合割）を決定
    QString teaiLabel;
    const QString initialSfen = prepareInitialSfen(filePath, teaiLabel);

    // 2) 解析（本譜＋分岐＋コメント）を一括取得
    KifParseResult res;
    QString parseWarn;
    KifToSfenConverter::parseWithVariations(filePath, res, &parseWarn);

    // resをデバッグ出力
    if (kGM_VERBOSE) {
        qDebug().noquote() << "[GM] KifParseResult dump:";
        if (!parseWarn.isEmpty()) {
            qDebug().noquote() << "  [parseWarn]" << parseWarn;
        }

        qDebug().noquote() << "  Mainline:";
        qDebug().noquote() << "    baseSfen: " << res.mainline.baseSfen;
        qDebug().noquote() << "    usiMoves: " << res.mainline.usiMoves;
        qDebug().noquote() << "    disp:";
        int mainIdx = 0;
        for (const auto& d : std::as_const(res.mainline.disp)) {
            qDebug().noquote() << "      [" << mainIdx << "] prettyMove: " << d.prettyMove;
            if (!d.comment.isEmpty()) {
                qDebug().noquote() << "           comment: " << d.comment;
            } else {
                qDebug().noquote() << "           comment: <none>";
            }
            qDebug().noquote() << "           timeText: " << d.timeText;
            ++mainIdx;
        }

        qDebug().noquote() << "  Variations:";
        int varNo = 0;
        for (const KifVariation& var : std::as_const(res.variations)) {
            qDebug().noquote() << "  [Var " << varNo << "]";
            qDebug().noquote() << "    startPly: " << var.startPly;
            qDebug().noquote() << "    baseSfen: " << var.line.baseSfen;
            qDebug().noquote() << "    usiMoves: " << var.line.usiMoves;
            qDebug().noquote() << "    disp:";
            int dispIdx = 0;
            for (const auto& d : std::as_const(var.line.disp)) {
                qDebug().noquote() << "      [" << dispIdx << "] prettyMove: " << d.prettyMove;
                if (!d.comment.isEmpty()) {
                    qDebug().noquote() << "           comment: " << d.comment;
                } else {
                    qDebug().noquote() << "           comment: <none>";
                }
                qDebug().noquote() << "           timeText: " << d.timeText;
                ++dispIdx;
            }
            ++varNo;
        }
    }

    // 先手/後手名などヘッダ反映
    {
        const QList<KifGameInfoItem> infoItems = KifToSfenConverter::extractGameInfo(filePath);
        populateGameInfo(infoItems);
        applyPlayersFromGameInfo(infoItems);
    }

    // KIF/CSA 共通の後処理に委譲
    applyParsedResultCommon_(filePath, initialSfen, teaiLabel,
                             res, parseWarn, "loadKifuFromFile");
}

void KifuLoadCoordinator::applyParsedResultCommon_(
    const QString& filePath,
    const QString& initialSfen,
    const QString& teaiLabel,
    const KifParseResult& res,
    const QString& parseWarn,
    const char* callerTag)
{
    // 本譜（表示／USI）
    const QList<KifDisplayItem>& disp = res.mainline.disp;
    m_usiMoves = res.mainline.usiMoves;

    // 終局/中断判定（見た目文字列で簡易判定）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) {
            if (s.contains(kw)) return true;
        }
        return false;
    };
    const bool hasTerminal = (!disp.isEmpty() && isTerminalPretty(disp.back().prettyMove));

    if (m_usiMoves.isEmpty() && !hasTerminal && disp.isEmpty()) {
        const QString errorMessage =
            tr("読み込み失敗 %1 から指し手を取得できませんでした。").arg(filePath);
        emit errorOccurred(errorMessage);
        qDebug().noquote() << "[MAIN]" << callerTag << "OUT (no moves)";
        m_loadingKifu = false; // 早期return時も必ず解除
        return;
    }

    // 3) 本譜の SFEN 列と m_gameMoves を再構築
    rebuildSfenRecord(initialSfen, m_usiMoves, hasTerminal);
    rebuildGameMoves(initialSfen, m_usiMoves);

    // 3.5) USI position コマンド列を構築（0..N）
    //     initialSfen は prepareInitialSfen() が返す手合い込みの SFEN
    //     m_usiMoves は 1..N の USI 文字列（"7g7f" 等）
    m_positionStrList.clear();
    m_positionStrList.reserve(m_usiMoves.size() + 1);

    const QString base = QStringLiteral("position sfen %1").arg(initialSfen);
    m_positionStrList.push_back(base);  // 0手目：moves なし

    QStringList acc; // 先頭からの累積
    acc.reserve(m_usiMoves.size());
    for (int i = 0; i < m_usiMoves.size(); ++i) {
        acc.push_back(m_usiMoves.at(i));
        // i+1 手目：先頭から i+1 個の moves を連結
        m_positionStrList.push_back(base + QStringLiteral(" moves ") + acc.join(' '));
    }

    // （任意）ログで確認
    qDebug().noquote() << "[USI] position list built. count=" << m_positionStrList.size();
    if (!m_positionStrList.isEmpty()) {
        qDebug().noquote() << "[USI] pos[0]=" << m_positionStrList.first();
        if (m_positionStrList.size() > 1) {
            qDebug().noquote() << "[USI] pos[1]=" << m_positionStrList.at(1);
        }
    }

    // 4) 棋譜表示へ反映（本譜）
    emit displayGameRecord(disp);

    // 5) 本譜スナップショットを保持（以降の解決・描画に使用）
    m_dispMain = disp;          // 表示列（1..N）
    m_sfenMain = *m_sfenRecord; // 0..N の局面列
    m_gmMain   = m_gameMoves;   // 1..N のUSIムーブ

    // 6) 変化を取りまとめ（必要に応じて保持：Plan生成やツリー表示では m_resolvedRows を主に使用）
    m_variationsByPly.clear();
    m_variationsSeq.clear();
    for (const KifVariation& kv : std::as_const(res.variations)) {
        KifLine L = kv.line;
        L.startPly = kv.startPly;         // “その変化が始まる絶対手数（1-origin）”
        if (L.disp.isEmpty()) continue;
        m_variationsByPly[L.startPly].push_back(L);
        m_variationsSeq.push_back(L);     // 入力順（KIF/CSA出現順）を保持
    }

    // 7) 棋譜テーブルの初期選択（開始局面を選択）
    if (m_recordPane && m_recordPane->kifuView()) {
        QTableView* view = m_recordPane->kifuView();
        if (view->model() && view->model()->rowCount() > 0) {
            const QModelIndex idx0 = view->model()->index(0, 0);
            if (view->selectionModel()) {
                view->selectionModel()->setCurrentIndex(
                    idx0,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            view->scrollTo(idx0, QAbstractItemView::PositionAtTop);
        }
    }

    // 8) 解決行を1本（本譜のみ）作成 → 0手適用
    m_resolvedRows.clear();

    ResolvedRow r;
    r.startPly = 1;
    r.parent   = -1;           // ★本譜
    r.disp     = disp;          // 1..N
    r.sfen     = *m_sfenRecord; // 0..N
    r.gm       = m_gameMoves;   // 1..N
    r.varIndex = -1;            // 本譜

    m_resolvedRows.push_back(r);
    m_activeResolvedRow = 0;
    m_activePly         = 0;

    // apply 内では m_loadingKifu を見て分岐候補の更新を抑止
    applyResolvedRowAndSelect(/*row=*/0, /*selPly=*/0);

    // 9) UIの整合
    emit enableArrowButtons();
    logImportSummary(filePath, m_usiMoves, disp, teaiLabel, parseWarn, QString());

    // 10) 解決済み行を構築（親探索規則で親子関係を決定）
    buildResolvedLinesAfterLoad();

    // 11) 分岐レポート → Plan 構築（Plan方式の基礎データ）
    dumpBranchSplitReport();
    buildBranchCandidateDisplayPlan();
    dumpBranchCandidateDisplayPlan();

    // ★ 追加：読み込み直後に “+付与 & 行着色” をまとめて反映（Main 行が表示中）
    applyBranchMarksForCurrentLine_();

    // 12) 分岐ツリーへ供給（黄色ハイライトは applyResolvedRowAndSelect 内で同期）
    if (m_analysisTab) {
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        rows.reserve(m_resolvedRows.size());

        // ★ detach 回避：QList を const 化して range-for
        for (const auto& rr : std::as_const(m_resolvedRows)) {
            EngineAnalysisTab::ResolvedRowLite x;
            x.startPly = rr.startPly;
            x.disp     = rr.disp;
            x.sfen     = rr.sfen;
            x.parent   = rr.parent;      // ★ 親インデックスをコピー
            rows.push_back(std::move(x));
        }

        m_analysisTab->setBranchTreeRows(rows);
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, /*ply=*/0, /*centerOn=*/true);
    }

    // 13) VariationEngine への投入（他機能で必要な可能性があるため保持）
    if (!m_varEngine) {
        m_varEngine = std::make_unique<KifuVariationEngine>();
    }
    {
        QVector<UsiMove> usiMain;
        usiMain.reserve(m_usiMoves.size());
        for (const auto& u : std::as_const(m_usiMoves)) {
            usiMain.push_back(UsiMove(u));
        }
        m_varEngine->ingest(res, m_sfenMain, usiMain, m_dispMain);
    }

    // ←ここで SFEN を各行に流し込む
    ensureResolvedRowsHaveFullSfen();
    // ←そして表を出す
    dumpAllRowsSfenTable();

    ensureResolvedRowsHaveFullGameMoves();
    dumpAllLinesGameMoves();

    // 14) （Plan方式化に伴い）WL 構築や従来の候補再計算は廃止
    // 15) ブランチ候補ワイヤリング（planActivated -> applyResolvedRowAndSelect）
    if (!m_branchCtl) {
        emit setupBranchCandidatesWiring_(); // 内部で planActivated の connect を済ませる
    }

    if (m_kifuBranchModel) {
        // 起動直後は候補を出さない（0手目）：モデルクリア＆ビュー非表示
        m_kifuBranchModel->clearBranchCandidates();
        m_kifuBranchModel->setHasBackToMainRow(false);
        if (QTableView* view =
            m_recordPane ? m_recordPane->branchView() : m_kifuBranchView) {
            view->setVisible(true);
            view->setEnabled(false);
        }
    }

    // ★ ロード完了 → 抑止解除
    m_loadingKifu = false;

    // 16) ツリーは読み込み後ロック（ユーザ操作で枝を生やさない）
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked after load";

    qDebug().noquote() << "[MAIN]" << callerTag << "OUT";
}

QString KifuLoadCoordinator::prepareInitialSfen(const QString& filePath, QString& teaiLabel) const
{
    const QString sfen = KifToSfenConverter::detectInitialSfenFromFile(filePath, &teaiLabel);
    return sfen.isEmpty()
               ? QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
               : sfen;
}

void KifuLoadCoordinator::populateGameInfo(const QList<KifGameInfoItem>& items)
{
    m_gameInfoTable->clearContents();
    m_gameInfoTable->setRowCount(items.size());

    for (int row = 0; row < items.size(); ++row) {
        const auto& it = items.at(row);
        auto *keyItem   = new QTableWidgetItem(it.key);
        auto *valueItem = new QTableWidgetItem(it.value);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        m_gameInfoTable->setItem(row, 0, keyItem);
        m_gameInfoTable->setItem(row, 1, valueItem);
    }

    m_gameInfoTable->resizeColumnToContents(0);

    // まだタブに載ってなければ、このタイミングで追加しておくと確実
    addGameInfoTabIfMissing();
}

void KifuLoadCoordinator::addGameInfoTabIfMissing()
{
    if (!m_tab) return;

    // Dock で表示していたら解除
    if (m_gameInfoDock && m_gameInfoDock->widget() == m_gameInfoTable) {
        m_gameInfoDock->setWidget(nullptr);
        m_gameInfoDock->deleteLater();
        m_gameInfoDock = nullptr;
    }

    // まだタブに無ければ追加
    if (m_tab->indexOf(m_gameInfoTable) == -1) {
        int anchorIdx = -1;

        // 1) EngineAnalysisTab（検討タブ）の直後に入れる
        if (m_analysisTab)
            anchorIdx = m_tab->indexOf(m_analysisTab);

        // 2) 念のため、タブタイトルで「コメント/Comments」を探してその直後に入れるフォールバック
        if (anchorIdx < 0) {
            for (int i = 0; i < m_tab->count(); ++i) {
                const QString t = m_tab->tabText(i);
                if (t.contains(tr("コメント")) || t.contains("Comments", Qt::CaseInsensitive)) {
                    anchorIdx = i;
                    break;
                }
            }
        }

        const int insertPos = (anchorIdx >= 0) ? anchorIdx + 1 : m_tab->count();
        m_tab->insertTab(insertPos, m_gameInfoTable, tr("対局情報"));
    }
}

QString KifuLoadCoordinator::findGameInfoValue(const QList<KifGameInfoItem>& items,
                                      const QStringList& keys) const
{
    for (const auto& it : items) {
        // KifGameInfoItem.key は「先手」「後手」等（末尾コロンは normalize 済み）
        if (keys.contains(it.key)) {
            const QString v = it.value.trimmed();
            if (!v.isEmpty()) return v;
        }
    }
    return QString();
}

void KifuLoadCoordinator::applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items)
{
    // 優先度：
    // 1) 先手／後手
    // 2) 下手／上手（→ 下手=Black, 上手=White）
    // 3) 先手省略名／後手省略名
    QString black = findGameInfoValue(items, { QStringLiteral("先手") });
    QString white = findGameInfoValue(items, { QStringLiteral("後手") });

    // 下手/上手にも対応（必要な側だけ補完）
    const QString shitate = findGameInfoValue(items, { QStringLiteral("下手") });
    const QString uwate   = findGameInfoValue(items, { QStringLiteral("上手") });

    if (black.isEmpty() && !shitate.isEmpty())
        black = shitate;                   // 下手 → Black
    if (white.isEmpty() && !uwate.isEmpty())
        white = uwate;                     // 上手 → White

    // 省略名でのフォールバック
    if (black.isEmpty())
        black = findGameInfoValue(items, { QStringLiteral("先手省略名") });
    if (white.isEmpty())
        white = findGameInfoValue(items, { QStringLiteral("後手省略名") });

    // 取得できた方だけ反映（既存表示を尊重）
    if (!black.isEmpty() && m_shogiView)
        m_shogiView->setBlackPlayerName(black);
    if (!white.isEmpty() && m_shogiView)
        m_shogiView->setWhitePlayerName(white);
}

void KifuLoadCoordinator::rebuildSfenRecord(const QString& initialSfen,
                                            const QStringList& usiMoves,
                                            bool hasTerminal)
{
    const QStringList list = SfenPositionTracer::buildSfenRecord(initialSfen, usiMoves, hasTerminal);
    if (!m_sfenRecord) m_sfenRecord = new QStringList;
    *m_sfenRecord = list; // COW
}

void KifuLoadCoordinator::rebuildGameMoves(const QString& initialSfen,
                                           const QStringList& usiMoves)
{
    m_gameMoves = SfenPositionTracer::buildGameMoves(initialSfen, usiMoves);
}

// 現在表示用の棋譜列（disp）を使ってモデルを再構成し、selectPly 行を選択・同期する
void KifuLoadCoordinator::showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly)
{
    // いま表示中の棋譜列を保持（分岐⇄本譜の復帰で再利用）
    m_dispCurrent = disp;

    // （既存）モデルへ反映：ここで displayGameRecord(disp) が呼ばれ、
    // その過程で m_currentMoveIndex が 0 に戻る実装になっている
    displayGameRecord(disp);

    // ★ 追加：現在表示中の行に対する「分岐あり手」マーキングをモデルへ反映
    applyBranchMarksForCurrentLine_();

    // ★ RecordPane 内のビューを使う
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view || !view->model()) return;

    // 行数（0 は「=== 開始局面 ===」、1..N が各手）
    const int rc  = view->model()->rowCount();
    const int row = qBound(0, selectPly, rc > 0 ? rc - 1 : 0);

    // 0列目インデックス（選択は行単位）
    const QModelIndex idx = view->model()->index(row, 0);
    if (auto* sel = view->selectionModel()) {
        sel->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    } else {
        view->setCurrentIndex(idx);
    }
    view->scrollTo(idx, QAbstractItemView::PositionAtCenter);

    // displayGameRecord() が 0 に戻した “現在の手数” を、選択行へ復元
    m_currentSelectedPly = row;
    m_currentMoveIndex   = row;

    // 盤面・ハイライトも現在手に同期
    emit syncBoardAndHighlightsAtRow(row);
}

void KifuLoadCoordinator::showBranchCandidatesFromPlan(int row, int ply1)
{
    // モデル/ビュー参照
    QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView;

    // 行・手の安全化（不正時はクリア＆ボタン非表示）
    if (ply1 <= 0 || row < 0 || row >= m_resolvedRows.size()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }

        // 「本譜に戻る」ボタンは隠す
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }

        // 文脈保存
        m_branchPlyContext  = qMax(0, ply1);
        m_activeResolvedRow = qBound(0, row, m_resolvedRows.size() - 1);
        return;
    }

    // 行→(手→計画) テーブルから該当プランを取得
    const auto rowIt = m_branchDisplayPlan.constFind(row);
    if (rowIt == m_branchDisplayPlan.constEnd()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }

        // ボタン非表示
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }

        m_branchPlyContext  = ply1;
        m_activeResolvedRow = row;
        return;
    }

    const auto& byPly = rowIt.value();
    const auto itP    = byPly.constFind(ply1);
    if (itP == byPly.constEnd()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }

        // ボタン非表示
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }

        m_branchPlyContext  = ply1;
        m_activeResolvedRow = row;
        return;
    }

    const BranchCandidateDisplay& plan = itP.value();

    // （必要なら）現在指し手のラベルを取得しておく（候補リスト選択合わせ用）
    QString currentLbl;
    {
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        if (li >= 0 && li < disp.size()) {
            currentLbl = pickLabelForDisp(disp.at(li));
        }
    }

    // --- BranchCandidatesController 経由で候補を更新（クリック→局面反映の経路を保持） ---
    if (m_branchCtl) {
        QVector<BranchCandidateDisplayItem> pubItems;
        pubItems.reserve(plan.items.size());
        for (const auto& it : plan.items) {
            BranchCandidateDisplayItem x;
            x.row      = it.row;
            x.varN     = it.varN;
            x.lineName = it.lineName;
            x.label    = it.label;
            pubItems.push_back(x);
        }
        m_branchCtl->refreshCandidatesFromPlan(ply1, pubItems, plan.baseLabel);
    } else if (m_kifuBranchModel) {
        // フォールバック：Controller 未注入でも最低限の表示だけは行う（クリック遷移は不可）
        QList<KifDisplayItem> rows;
        rows.reserve(plan.items.size());
        for (const auto& it : plan.items) {
            rows.push_back(KifDisplayItem(it.label, QString(), QString(), ply1));
        }
        m_kifuBranchModel->setHasBackToMainRow(false);
        m_kifuBranchModel->setBranchCandidatesFromKif(rows);
    }

    // --- ビューの可視/有効状態だけ先に反映（既定選択はここでは行わない） ---
    if (view && m_kifuBranchModel) {
        const int rows = m_kifuBranchModel->rowCount();
        view->setVisible(true);
        view->setEnabled(rows > 0);
    }

    // --- ここがポイント：描画後に「現在の手」と一致する候補行を探して選択し直す ---
    if (view && m_kifuBranchModel && view->model() && view->selectionModel()) {
        // 先頭の手数（"3 " 等）を剥がして比較する
        // （ラムダ禁止のため簡易に都度 remove で対応）
        QString wantKey = currentLbl;
        {
            // ^\s*\d+\s* を削除
            QRegularExpression re(QStringLiteral(R"(^\s*\d+\s*)"));
            wantKey.remove(re);
        }

        int foundRow = -1;
        const int rcount = m_kifuBranchModel->rowCount();
        int r = 0;
        while (r < rcount) {
            const QModelIndex idx = m_kifuBranchModel->index(r, 0);
            QString cell = m_kifuBranchModel->data(idx, Qt::DisplayRole).toString();
            // こちらも先頭の手数を剥がして比較
            {
                QRegularExpression re(QStringLiteral(R"(^\s*\d+\s*)"));
                cell.remove(re);
            }
            if (cell == wantKey && !wantKey.isEmpty()) {
                foundRow = r;
                break;
            }
            ++r;
        }

        QItemSelectionModel* sel = view->selectionModel();
        if (foundRow >= 0) {
            const QModelIndex pick = m_kifuBranchModel->index(foundRow, 0);
            sel->setCurrentIndex(pick, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            view->scrollTo(pick, QAbstractItemView::PositionAtCenter);
        } else {
            // 見つからない場合は勝手に先頭を選ばず、選択をクリアしておく
            sel->clearSelection();
        }
    }

    // --- 「本譜に戻る」ボタンの表示制御とスロット接続（ラムダ無し） ---
    if (m_recordPane) {
        if (auto* btn = m_recordPane->backToMainButton()) {
            const bool hasRows = (m_kifuBranchModel && m_kifuBranchModel->rowCount() > 0);
            btn->setVisible(hasRows);
            if (hasRows) {
                QObject::connect(btn, &QPushButton::clicked,
                                 this, &KifuLoadCoordinator::onBackToMainButtonClicked_,
                                 Qt::UniqueConnection);
            }
        }
    }

    // UI 状態
    m_branchPlyContext  = ply1;
    m_activeResolvedRow = row;
}

void KifuLoadCoordinator::onBackToMainButtonClicked_()
{
    // varIndex == -1 の行が「本譜」
    int mainRow = 0;
    const int n = m_resolvedRows.size();
    for (int i = 0; i < n; ++i) {
        if (m_resolvedRows.at(i).varIndex < 0) { mainRow = i; break; }
    }

    const int safePly = (m_branchPlyContext < 0) ? 0 : m_branchPlyContext;

    // ★ Sticky の自動保持を一度だけ無効化するため、
    //   「直前の行」を本譜(=0行)として扱わせる
    m_activeResolvedRow = mainRow;

    applyResolvedRowAndSelect(mainRow, safePly);
}

// ====== アクティブ行に対する「分岐あり手」の再計算 → ビュー再描画 ======
void KifuLoadCoordinator::updateKifuBranchMarkersForActiveRow()
{
    m_branchablePlySet.clear();

    // まずビュー参照を取得（nullでも安全に抜ける）
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);

    if (m_resolvedRows.isEmpty()) {
        if (view && view->viewport()) view->viewport()->update();
        return;
    }

    const int active = qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1);
    const auto& r = m_resolvedRows[active];

    // r.disp: 1..N の手表示, r.sfen: 0..N の局面列
    for (int ply = 1; ply <= r.disp.size(); ++ply) {
        const int idx = ply - 1;                 // sfen は ply-1 が基底
        if (idx < 0 || idx >= r.sfen.size()) continue;

        const auto itPly = m_branchIndex.constFind(ply);
        if (itPly == m_branchIndex.constEnd()) continue;

        const QString base = r.sfen.at(idx);
        const auto itBase = itPly->constFind(base);
        if (itBase == itPly->constEnd()) continue;

        // 同じ手目に候補が2つ以上 → 分岐点としてマーキング
        if (itBase->size() >= 2) {
            m_branchablePlySet.insert(ply);
        }
    }

    // デリゲート装着（未装着ならここで装着）
    ensureBranchRowDelegateInstalled();

    // 再描画
    if (view && view->viewport()) view->viewport()->update();
}

void KifuLoadCoordinator::ensureBranchRowDelegateInstalled()
{
    // RecordPane から棋譜ビューを取得
    QTableView* view = (m_recordPane ? m_recordPane->kifuView() : nullptr);
    if (!view) return;

    if (!m_branchRowDelegate) {
        // デリゲートをビューの子として作成し、ビューに適用
        m_branchRowDelegate = new BranchRowDelegate(view);
        view->setItemDelegate(m_branchRowDelegate);
    } else {
        // 念のため親が違う場合は付け替え
        if (m_branchRowDelegate->parent() != view) {
            m_branchRowDelegate->setParent(view);
            view->setItemDelegate(m_branchRowDelegate);
        }
    }

    // 「分岐あり」マーカーの集合をデリゲートへ渡す
    m_branchRowDelegate->setMarkers(&m_branchablePlySet);
}

KifuLoadCoordinator::BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

KifuLoadCoordinator::BranchRowDelegate::~BranchRowDelegate() = default;

void KifuLoadCoordinator::BranchRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt(option);
    QStyledItemDelegate::initStyleOption(&opt, index);

    const bool isBranchable = (m_marks && m_marks->contains(index.row()));

    // 選択時のハイライトと衝突しないように、未選択時のみオレンジ背景
    if (isBranchable && !(opt.state & QStyle::State_Selected)) {
        painter->save();
        painter->fillRect(opt.rect, QColor(255, 220, 160));
        painter->restore();
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

void KifuLoadCoordinator::logImportSummary(const QString& filePath,
                                  const QStringList& usiMoves,
                                  const QList<KifDisplayItem>& disp,
                                  const QString& teaiLabel,
                                  const QString& warnParse,
                                  const QString& warnConvert) const
{
    if (!warnParse.isEmpty())
        qWarning().noquote() << "[KIF parse warnings]\n" << warnParse.trimmed();
    if (!warnConvert.isEmpty())
        qWarning().noquote() << "[KIF convert warnings]\n" << warnConvert.trimmed();

    qDebug().noquote() << QStringLiteral("KIF読込: %1手（%2）")
                              .arg(usiMoves.size())
                              .arg(QFileInfo(filePath).fileName());
    for (int i = 0; i < qMin(5, usiMoves.size()); ++i) {
        qDebug().noquote() << QStringLiteral("USI[%1]: %2")
        .arg(i + 1)
            .arg(usiMoves.at(i));
    }

    qDebug().noquote() << QStringLiteral("手合割: %1")
                              .arg(teaiLabel.isEmpty()
                                       ? QStringLiteral("平手(既定)")
                                       : teaiLabel);

    // 本譜（表示用）。コメントがあれば直後に出力。
    for (const auto& it : disp) {
        const QString time = it.timeText.isEmpty()
        ? QStringLiteral("00:00/00:00:00")
        : it.timeText;
        qDebug().noquote() << QStringLiteral("「%1」「%2」").arg(it.prettyMove, time);
        if (!it.comment.trimmed().isEmpty()) {
            qDebug().noquote() << QStringLiteral("  └ コメント: %1")
                                      .arg(it.comment.trimmed());
        }
    }

    // SFEN（抜粋）
    if (m_sfenRecord) {
        for (int i = 0; i < qMin(12, m_sfenRecord->size()); ++i) {
            qDebug().noquote() << QStringLiteral("%1) %2")
            .arg(i)
                .arg(m_sfenRecord->at(i));
        }
    }

    // m_gameMoves（従来通り）
    qDebug() << "m_gameMoves size:" << m_gameMoves.size();
    for (int i = 0; i < m_gameMoves.size(); ++i) {
        qDebug().noquote() << QString("%1) ").arg(i + 1) << m_gameMoves[i];
    }
}

void KifuLoadCoordinator::buildResolvedLinesAfterLoad()
{
    // 既存: Resolved行の構築や m_varEngine->ingest(), rebuildBranchWhitelist() の後
    m_branchTreeLocked = true;
    qDebug() << "[BRANCH] tree locked (after buildResolvedLinesAfterLoad)";

    // 本関数は「後勝ち」をやめ、各変化の“親”を
    // 直前→さらに前…と遡って「b(=この変化の startPly) > a(=先行変化の startPly)」
    // を満たす最初の変化行にする（無ければ本譜 row=0）という規則で構築する。

    m_resolvedRows.clear();

    // --- 行0 = 本譜 ---
    ResolvedRow mainRow;
    mainRow.startPly = 1;
    mainRow.parent   = -1;           // ★追加：本譜の親は -1 を明示
    mainRow.disp     = m_dispMain;   // 1..N（表示）
    mainRow.sfen     = m_sfenMain;   // 0..N（局面）
    mainRow.gm       = m_gmMain;     // 1..N（USIムーブ）
    mainRow.varIndex = -1;           // 本譜
    m_resolvedRows.push_back(mainRow);

    if (m_variationsSeq.isEmpty()) {
        qDebug().noquote() << "[RESOLVED] only mainline (no variations).";
        return;
    }

    // 変化 vi → 解決済み行 index の写像（empty の変化は -1）
    QVector<int> varToRowIndex(m_variationsSeq.size(), -1);

    // 親行を解決するローカルラムダ：
    // 直前→さらに前…へ遡り、startPly(prev) < startPly(cur) を満たす
    // 先行変化を見つけたら、その変化の“解決済み行 index”を返す。無ければ 0（本譜）。
    auto resolveParentRowIndex = [&](int vi)->int {
        const int b = qMax(1, m_variationsSeq.at(vi).startPly);
        for (int prev = vi - 1; prev >= 0; --prev) {
            const KifLine& p = m_variationsSeq.at(prev);
            if (p.disp.isEmpty()) continue;          // 空の変化は親候補にしない
            const int a = qMax(1, p.startPly);
            if (b > a && varToRowIndex.at(prev) >= 0) {
                return varToRowIndex.at(prev);        // その変化の“行 index”
            }
        }
        return 0; // 見つからなければ本譜
    };

    // --- 行1.. = 各変化 ---
    for (int vi = 0; vi < m_variationsSeq.size(); ++vi) {
        const KifLine& v = m_variationsSeq.at(vi);
        if (v.disp.isEmpty()) {
            varToRowIndex[vi] = -1;
            continue;
        }

        const int start = qMax(1, v.startPly);   // 1-origin
        const int parentRow = resolveParentRowIndex(vi);
        const ResolvedRow& base = m_resolvedRows.at(parentRow);

        // 1) プレフィクス（1..start-1）を“親行”から切り出し、足りない分は本譜で補完
        QList<KifDisplayItem> prefixDisp = base.disp;
        if (prefixDisp.size() > start - 1) prefixDisp.resize(start - 1);

        QVector<ShogiMove> prefixGm = base.gm;
        if (prefixGm.size() > start - 1) prefixGm.resize(start - 1);

        QStringList prefixSfen = base.sfen;
        if (prefixSfen.size() > start) prefixSfen.resize(start);

        // 親行が短い場合は本譜で延長（disp/gm/sfen を一致させる）
        while (prefixDisp.size() < start - 1 && prefixDisp.size() < m_dispMain.size()) {
            const int idx = prefixDisp.size(); // 0.. → 手数は idx+1
            prefixDisp.append(m_dispMain.at(idx));
        }
        while (prefixGm.size() < start - 1 && prefixGm.size() < m_gmMain.size()) {
            const int idx = prefixGm.size();
            prefixGm.append(m_gmMain.at(idx));
        }
        while (prefixSfen.size() < start && prefixSfen.size() < m_sfenMain.size()) {
            const int sidx = prefixSfen.size(); // 0.. → sfen は 0=初期,1=1手後...
            prefixSfen.append(m_sfenMain.at(sidx));
        }

        // 2) 変化本体を連結
        ResolvedRow row;
        row.startPly = start;
        row.parent   = parentRow;    // ★追加：親行 index を保存
        row.varIndex = vi;

        row.disp = prefixDisp;
        row.disp += v.disp;

        row.gm = prefixGm;
        row.gm += v.gameMoves;

        // v.sfenList は [0]=基底（start-1 手後）, [1..]=各手後 を想定
        row.sfen = prefixSfen;                  // 0..start-1 は既に parent→本譜で整合
        if (v.sfenList.size() >= 2) {
            row.sfen += v.sfenList.mid(1);      // start.. の各手後
        } else if (v.sfenList.size() == 1) {
            // 念のため：基底のみしか無い場合でも、prefix は揃っているので何もしない
        }

        // 追加
        m_resolvedRows.push_back(row);
        varToRowIndex[vi] = m_resolvedRows.size() - 1;
    }

    qDebug().noquote() << "[RESOLVED] lines=" << m_resolvedRows.size();

    auto varIdOf = [&](int varIndex)->int {
        if (varIndex < 0) return -1; // 本譜
        if (m_varEngine) {
            const int id = m_varEngine->varIdForSourceIndex(varIndex);
            return (id >= 0) ? id : (varIndex + 1); // フォールバック
        }
        return varIndex + 1; // 既存仕様に準拠（空行スキップ条件が一致している前提）
    };

    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        const auto& r = m_resolvedRows[i];
        QStringList pm; pm.reserve(r.disp.size());
        for (const auto& d : r.disp) pm << d.prettyMove;

        const bool isMain = (i == 0);
        const QString label = isMain
                                  ? "Main"
                                  : QString("Var%1(id=%2)").arg(r.varIndex).arg(varIdOf(r.varIndex)); // ★ id 併記

        // ★修正：表示も保存済みの r.parent を使用（再計算しない）
        qDebug().noquote()
            << "  [" << label << "]"
            << " parent=" << r.parent
            << " start="  << r.startPly
            << " ndisp="  << r.disp.size()
            << " nsfen="  << r.sfen.size()
            << " seq=「 " << pm.join(" / ") << " 」";
    }
}

// 行番号から表示名を作る（Main / VarN）
QString KifuLoadCoordinator::rowNameFor_(int row) const
{
    if (row < 0 || row >= m_resolvedRows.size()) return QString("<?>");
    const auto& rr = m_resolvedRows[row];
    return (rr.varIndex < 0) ? QStringLiteral("Main")
                             : QStringLiteral("Var%1").arg(rr.varIndex);
}

// 1始まり hand-ply のラベル（無ければ ""）
QString KifuLoadCoordinator::labelAt_(const ResolvedRow& rr, int ply) const
{
    const int li = ply - 1;
    if (li < 0 || li >= rr.disp.size()) return QString();
    return pickLabelForDisp(rr.disp.at(li));
}

// 1..p までの完全一致（両方に手が存在し、かつ全ラベル一致）なら true
bool KifuLoadCoordinator::prefixEqualsUpTo_(int rowA, int rowB, int p) const
{
    if (rowA < 0 || rowA >= m_resolvedRows.size()) return false;
    if (rowB < 0 || rowB >= m_resolvedRows.size()) return false;
    const auto& A = m_resolvedRows[rowA];
    const auto& B = m_resolvedRows[rowB];
    for (int k = 1; k <= p; ++k) {
        const QString a = labelAt_(A, k);
        const QString b = labelAt_(B, k);
        if (a.isEmpty() || b.isEmpty() || a != b) return false;
    }
    return true;
}

void KifuLoadCoordinator::dumpBranchSplitReport() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 行ごとに出力
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        const QString header = rowNameFor_(r);
        qDebug().noquote() << header;

        const int maxPly = rr.disp.size();
        for (int p = 1; p <= maxPly; ++p) {
            const QString curLbl = labelAt_(rr, p);
            // この行と「1..p まで完全一致」する仲間を抽出
            QList<int> group;
            for (int j = 0; j < m_resolvedRows.size(); ++j) {
                if (prefixEqualsUpTo_(r, j, p)) group << j;
            }

            // その仲間たちの「次手 (p+1)」を集計（存在するものだけ）
            struct NextMove { QString who; QString lbl; };
            QVector<NextMove> nexts; nexts.reserve(group.size());
            QSet<QString> uniq;

            // ★ detach 回避：QList を const 化して range-for
            for (int j : std::as_const(group)) {
                const QString lblNext = labelAt_(m_resolvedRows[j], p + 1);
                if (lblNext.isEmpty()) continue; // ここで終端は候補に出さない
                const QString who = rowNameFor_(j);
                nexts.push_back({who, lblNext});
                uniq.insert(lblNext);
            }

            const QString curOrEmpty = curLbl.isEmpty() ? QStringLiteral("<EMPTY>") : curLbl;

            if (uniq.size() > 1) {
                // 分岐あり：各ライン名と次手を「、」で列挙
                QStringList parts;
                parts.reserve(nexts.size());
                for (const auto& nm : nexts) {
                    parts << QStringLiteral("%1 %2").arg(nm.who, nm.lbl);
                }

                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐あり %3")
                           // ★ multi-arg を使用（p は number 化）
                           .arg(QString::number(p),
                                curOrEmpty,
                                parts.join(QStringLiteral("、")));
            } else {
                qDebug().noquote()
                << QStringLiteral("%1 %2 分岐なし")
                        // ★ multi-arg を使用
                        .arg(QString::number(p), curOrEmpty);
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

void KifuLoadCoordinator::dumpBranchCandidateDisplayPlan() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        // ply1 は 1-based
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    // 行ごとに
    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        const int len = m_resolvedRows[r].disp.size();
        const auto itRow = m_branchDisplayPlan.constFind(r);

        for (int ply1 = 1; ply1 <= len; ++ply1) {
            const QString base = labelAt(r, ply1);
            bool has = false;
            QVector<BranchCandidateDisplayItem> items;

            if (itRow != m_branchDisplayPlan.constEnd()) {
                const auto& mp = itRow.value();
                auto itP = mp.constFind(ply1);
                if (itP != mp.constEnd()) {
                    has   = true;
                    items = itP.value().items;
                }
            }

            if (!has) {
                const QString baseOrEmpty = base.isEmpty() ? QStringLiteral("<EMPTY>") : base;
                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐候補表示なし")
                           .arg(QString::number(ply1), baseOrEmpty);
            } else {
                // "Main ▲…、Var0 ▲…、Var2 ▲…" の並びを構築
                QStringList parts;
                parts.reserve(items.size());
                for (const auto& it : std::as_const(items)) {  // ← detach 回避
                    parts << QStringLiteral("%1 %2").arg(it.lineName, it.label);
                }
                const QString baseOrEmpty = base.isEmpty() ? QStringLiteral("<EMPTY>") : base;
                qDebug().noquote()
                    << QStringLiteral("%1 %2 分岐候補表示あり %3")
                           .arg(QString::number(ply1),
                                baseOrEmpty,
                                parts.join(QStringLiteral("、")));  // ← multi-arg
            }
        }

        // 行間の空行
        qDebug().noquote() << "";
    }
}

void KifuLoadCoordinator::ensureResolvedRowsHaveFullSfen()
{
    if (m_resolvedRows.isEmpty()) return;

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen BEGIN";

    // Main の SFEN（VEが空なら既存の行0を使う）
    QStringList veMain = (m_varEngine ? m_varEngine->mainlineSfen() : QStringList());
    if (veMain.isEmpty() && !m_resolvedRows[0].sfen.isEmpty())
        veMain = m_resolvedRows[0].sfen;

    auto sfenFromVeForRow = [&](const ResolvedRow& rr)->QStringList {
        if (rr.varIndex < 0) {
            // 本譜
            return veMain;
        }
        if (!m_varEngine) return {};
        const int vid = m_varEngine->variationIdFromSourceIndex(rr.varIndex);
        return m_varEngine->sfenForVariationId(vid);
    };

    const int rowCount = m_resolvedRows.size();
    for (int r = 0; r < rowCount; ++r) {
        auto& rr = m_resolvedRows[r];

        const int need = rr.disp.size() + 1;     // 0..N
        const int s    = qMax(1, rr.startPly);   // 1-origin
        const int base = s - 1;                  // 直前局面の添字

        // 親行（無ければ Main=0）
        int parentRow = 0;
        if (rr.parent >= 0 && rr.parent < rowCount) {
            parentRow = rr.parent;
        }

        // 親のプレフィクス（親が空なら mainline を使う）
        const QStringList parentPrefix =
            (!m_resolvedRows[parentRow].sfen.isEmpty()
                 ? m_resolvedRows[parentRow].sfen
                 : veMain);

        // この行の VE 配列（base を先頭とする 0..M）
        const QStringList veRow = sfenFromVeForRow(rr);

        // 合成先。既存をベースにするが、0..base は必ず親で上書きする
        QStringList full = rr.sfen;

        // --- 1) 0..base を親の SFEN で「強制上書き」 -----------------------
        //     ※ ここが今回の肝：古い/別行の値が残らないようにする
        for (int i = 0; i <= base; ++i) {
            if (i >= parentPrefix.size()) break;   // 親に無ければ打ち切り
            const QString pv = parentPrefix.at(i);
            if (i < full.size()) {
                full[i] = pv;                      // 常に上書き
            } else if (!pv.isEmpty()) {
                full.append(pv);                   // 既知のものだけ追加
            } else {
                break;                             // 空は追加しない
            }
        }

        // デバッグ（境界確認）
        auto hashS = [](const QString& s){ return qHash(s); };
        auto safeAt = [&](const QStringList& a, int i)->QString{
            return (0<=i && i<a.size() ? a.at(i) : QString());
        };

        qDebug().noquote()
            << "[SFEN] row=" << r
            << " base=" << base
            << " pre(base-1)#=" << hashS(safeAt(full, base-1))
            << " pre(base)#="   << hashS(safeAt(full, base))
            << " pre(base+1)#=" << hashS(safeAt(full, base+1));


        // --- 2) VE の部分配列を base から重ねる（空は書かない／ギャップは埋めない） ---
        bool gap = false;
        if (!veRow.isEmpty()) {
            for (int j = 0; j < veRow.size(); ++j) {
                const int pos = base + j;
                const QString v = veRow.at(j);
                if (v.isEmpty()) continue;

                if (pos < full.size()) {
                    full[pos] = v;
                } else if (pos == full.size()) {
                    full.append(v);
                } else {
                    // 間の index が未充足（空で埋めない方針なので打ち切り）
                    gap = true;
                    break;
                }
            }
        } else {
            // veRow が空：壊さずスキップ（警告のみ）
            if (full.size() < need) {
                qWarning().noquote()
                << "[SFEN] SKIP(fill) row=" << r
                << " startPly=" << rr.startPly
                << " need=" << need
                << " prefix(sz=" << parentPrefix.size() << ")"
                << " veRow(sz=0)  -- keep r.sfen as-is";
            }
        }

        // --- 3) 検査＆警告 ---------------------------------------------------
        bool missing = (full.size() < need);
        if (!missing) {
            for (int i = 0; i < need; ++i) {
                if (full.at(i).isEmpty()) { missing = true; break; }
            }
        }
        if (missing || gap) {
            qWarning().noquote()
            << "[SFEN] WARN(fill) row=" << r
            << " startPly=" << rr.startPly
            << " need=" << need
            << " prefix(sz=" << parentPrefix.size() << ")"
            << " veRow(sz=" << veRow.size() << ")"
            << (gap ? " GAP" : "");
        }

        // デバッグ（境界確認：合成後）

        qDebug().noquote()
            << "[SFEN] row=" << r
            << " post(base-1)#=" << hashS(safeAt(full, base-1))
            << " post(base)#="   << hashS(safeAt(full, base))
            << " post(base+1)#=" << hashS(safeAt(full, base+1));


        rr.sfen = std::move(full);
    }

    qDebug() << "[SFEN] ensureResolvedRowsHaveFullSfen END";
}

void KifuLoadCoordinator::dumpAllRowsSfenTable() const
{
    if (m_resolvedRows.isEmpty()) return;

    auto labelAt = [&](int row, int ply1)->QString {
        const int li = ply1 - 1;
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << (r == 0 ? "Main" : QString("Var%1").arg(r - 1));

        // 0: 開始局面
        const QString s0 = (!rr.sfen.isEmpty() ? rr.sfen.first() : QStringLiteral("<SFEN MISSING>"));
        qDebug().noquote() << QStringLiteral("0 開始局面 %1").arg(s0);

        // 1..N
        for (int ply1 = 1; ply1 <= rr.disp.size(); ++ply1) {
            const QString lbl  = labelAt(r, ply1);
            QString sfen = (ply1 >= 0 && ply1 < rr.sfen.size()) ? rr.sfen.at(ply1) : QString();
            if (sfen.isEmpty()) sfen = QStringLiteral("<SFEN MISSING>");

            qDebug().noquote()
                << QStringLiteral("%1 %2 %3")
                       .arg(QString::number(ply1),
                            lbl.isEmpty() ? QStringLiteral("<EMPTY>") : lbl,
                            sfen);
        }
        qDebug().noquote() << "";
    }
}

// 各 ResolvedRow の SFEN 列から gm（ShogiMove 列）を復元する（詳細ログ付き）
void KifuLoadCoordinator::ensureResolvedRowsHaveFullGameMoves()
{
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves BEGIN";
    for (int i = 0; i < m_resolvedRows.size(); ++i) {
        auto& r = m_resolvedRows[i];

        const int nsfen = r.sfen.size();     // 0..N
        const int ndisp = r.disp.size();     // 1..N
        // resign など「盤が変わらない終端」は 1 手として数えないので、基本は min(ndisp, nsfen-1)
        const int want  = qMax(0, qMin(ndisp, nsfen - 1));

        const QString label = (i == 0)
                                  ? QStringLiteral("Main")
                                  : QStringLiteral("Var%1(id=%2)")
                                        .arg(r.varIndex)
                                        .arg(m_varEngine ? m_varEngine->varIdForSourceIndex(r.varIndex)
                                                         : (r.varIndex + 1));

        qDebug().noquote()
            << QString("[GM] row=%1 \"%2\" start=%3 nsfen=%4 ndisp=%5 current_gm=%6 want=%7")
                   .arg(i).arg(label).arg(r.startPly).arg(nsfen).arg(ndisp).arg(r.gm.size()).arg(want);

        // サイズが一致していればスキップ（必要に応じて強制再構築したい場合はこの if を外す）
        if (r.gm.size() == want) {
            qDebug().noquote() << QString("[GM] row=%1 keep (size match)").arg(i);
            continue;
        }

        r.gm.clear();

        for (int ply1 = 1; ply1 <= want; ++ply1) {
            const QString prev = r.sfen.at(ply1 - 1);
            const QString next = r.sfen.at(ply1);
            const QString pretty = r.disp.at(ply1 - 1).prettyMove;

            ShogiMove mv;
            const bool ok = deriveMoveFromSfenPair(prev, next, &mv); // ★ 内部で FLIP（USI向き）を採用

            if (!ok) {
                // 盤が同一 → 投了やコメントのみなど（gm へは積まない）
                qDebug().noquote()
                    << QString("[GM] row=%1 ply=%2 \"%3\" : NO-DELTA (terminal/comment)")
                           .arg(i).arg(ply1).arg(pretty);
                continue;
            }

            r.gm.push_back(mv);

            auto qcharToStr = [](QChar c)->QString { return c.isNull() ? QString(" ") : QString(c); };
            qDebug().noquote()
                << QStringLiteral("[GM] row=%1 ply=%2 \"%3\"  From:(%4,%5) To:(%6,%7) Moving:%8 Captured:%9 Promotion:%10")
                       .arg(QString::number(i),
                            QString::number(ply1),
                            pretty)
                       .arg(QString::number(mv.fromSquare.x()),
                            QString::number(mv.fromSquare.y()),
                            QString::number(mv.toSquare.x()))
                       .arg(QString::number(mv.toSquare.y()),
                            qcharToStr(mv.movingPiece),
                            qcharToStr(mv.capturedPiece))
                       .arg(mv.isPromotion ? QStringLiteral("true") : QStringLiteral("false"));
        }

        qDebug().noquote()
            << QString("[GM] row=%1 built gm.size=%2 / want=%3").arg(i).arg(r.gm.size()).arg(want);
    }
    qDebug() << "[GM] ensureResolvedRowsHaveFullGameMoves END";
}

void KifuLoadCoordinator::dumpAllLinesGameMoves() const
{
    if (m_resolvedRows.isEmpty()) return;

    // 終局判定（見た目ベース）
    static const QStringList kTerminalKeywords = {
        QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"),
        QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰"),
    };
    auto isTerminalPretty = [&](const QString& s)->bool {
        for (const auto& kw : kTerminalKeywords) if (s.contains(kw)) return true;
        return false;
    };

    auto lineNameFor = [](int row)->QString {
        return (row == 0) ? QStringLiteral("Main")
                          : QStringLiteral("Var%1").arg(row - 1);
    };
    auto labelAt = [&](const ResolvedRow& rr, int li)->QString {
        return (li >= 0 && li < rr.disp.size())
                 ? pickLabelForDisp(rr.disp.at(li))
                 : QString();
    };
    auto fmtPt = [](const QPoint& p)->QString {
        return QStringLiteral("(%1,%2)").arg(p.x()).arg(p.y());
    };

    // QChar/char/整数 いずれでも文字列化できる汎用ヘルパ
    auto pieceToQString = [](auto v)->QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, QChar>) {
            return v.isNull() ? QString() : QString(v);
        } else if constexpr (std::is_same_v<T, char>) {
            return v ? QString(QChar(v)) : QString();
        } else if constexpr (std::is_integral_v<T>) {
            return v ? QString(QChar(static_cast<ushort>(v))) : QString();
        } else {
            return QString();
        }
    };

    for (int r = 0; r < m_resolvedRows.size(); ++r) {
        const auto& rr = m_resolvedRows[r];
        qDebug().noquote() << lineNameFor(r);

        // 0) 開始局面（From/To 等は出さない）
        qDebug().noquote() << "0 開始局面";

        const int M = rr.disp.size();
        for (int li = 0; li < M; ++li) {
            const QString pretty = labelAt(rr, li);
            const bool terminal  = isTerminalPretty(pretty);

            // ヘッダ（手数 + 見た目ラベル）
            const QString head = QStringLiteral("%1 %2").arg(li + 1).arg(pretty);

            if (terminal || li >= rr.gm.size()) {
                // 投了など or GM不足 → そのまま
                qDebug().noquote() << head;
                continue;
            }

            const ShogiMove& mv = rr.gm.at(li);

            // ドロップ（打つ手）は fromSquare が無効座標の可能性
            const bool hasFrom = (mv.fromSquare.x() >= 0 && mv.fromSquare.y() >= 0);
            const QString fromS = hasFrom ? fmtPt(mv.fromSquare) : QString();
            const QString toS   = fmtPt(mv.toSquare);

            const QString moving = pieceToQString(mv.movingPiece);
            const QString cap    = pieceToQString(mv.capturedPiece);
            const bool promoted  = mv.isPromotion;   // ← 正しいフィールド名

            qDebug().noquote()
                << head
                << " From:" << fromS
                << " To:" << toS
                << " Moving:" << moving
                << " Captured:" << cap
                << " Promotion:" << (promoted ? "true" : "false");
        }

        qDebug().noquote() << "";
    }
}

void KifuLoadCoordinator::buildBranchCandidateDisplayPlan()
{
    m_branchDisplayPlan.clear();

    const int R = m_resolvedRows.size();
    if (R == 0) return;

    auto labelAt = [&](int row, int li)->QString {
        const auto& disp = m_resolvedRows[row].disp;
        return (li >= 0 && li < disp.size()) ? pickLabelForDisp(disp.at(li)) : QString();
    };

    auto prefixEquals = [&](int r1, int r2, int uptoLi)->bool {
        // li=0 のときは「初手より前の共通部分」は空なので常に一致とみなす
        for (int i = 0; i < uptoLi; ++i) {
            if (labelAt(r1, i) != labelAt(r2, i)) return false;
        }
        return true;
    };

    // 各行 r の各ローカル添字 li（0-based）について、
    // 「初手から li-1 まで完全一致する行」をグループ化し、
    // その li 手目に 2 種類以上の指し手があれば分岐とみなす。
    // ★ 表示は “その手（li）” に出す（＝1手先に送らない）
    for (int r = 0; r < R; ++r) {
        const int len = m_resolvedRows[r].disp.size();
        if (len == 0) continue;

        for (int li = 0; li < len; ++li) {
            // この行 r と「初手から li-1 まで一致」する行
            QVector<int> group;
            group.reserve(R);
            for (int g = 0; g < R; ++g) {
                if (li < m_resolvedRows[g].disp.size() && prefixEquals(r, g, li)) {
                    group.push_back(g);
                }
            }
            if (group.size() <= 1) continue; // 比較相手がいない

            // グループの li 手目ラベルを集計
            QHash<QString, QVector<int>> labelToRows;
            for (int g : group) {
                const QString lbl = labelAt(g, li);
                labelToRows[lbl].push_back(g);
            }
            if (labelToRows.size() <= 1) continue; // 全員同じ指し手 → 分岐ではない

            // 表示先 ply（1-based）。li は 0-based
            const int targetPly = li + 1;
            if (targetPly > m_resolvedRows[r].disp.size()) continue;

            // 見出し（この行 r の li 手目）
            const QString baseForDisplay = labelAt(r, li);

            // ===== 重複整理（自分の行 > Main(=row0) > 若い VarN）=====
            struct TmpKeep { QString lbl; int keepRow; };
            QVector<TmpKeep> keeps; keeps.reserve(labelToRows.size());

            for (auto it = labelToRows.constBegin(); it != labelToRows.constEnd(); ++it) {
                const QString lbl = it.key();
                const QVector<int>& rowsWithLbl = it.value();

                int keep = -1;

                // 1) 自分の行 r を最優先
                bool hasSelf = false;
                for (int cand : rowsWithLbl) {
                    if (cand == r) { keep = cand; hasSelf = true; break; }
                }

                // 2) 自分が無ければ Main(row=0)
                if (!hasSelf) {
                    bool hasMain = false;
                    for (int cand : rowsWithLbl) {
                        if (cand == 0) { keep = 0; hasMain = true; break; }
                    }

                    // 3) それも無ければ最小 row（VarN の若い方）
                    if (!hasMain) {
                        keep = rowsWithLbl.first();
                        for (int cand : rowsWithLbl) {
                            if (cand < keep) keep = cand;
                        }
                    }
                }

                keeps.push_back({ lbl, keep });
            }

            // 表示順: Main が先、次に row 昇順
            std::sort(keeps.begin(), keeps.end(), [](const TmpKeep& a, const TmpKeep& b){
                if (a.keepRow == 0 && b.keepRow != 0) return true;
                if (a.keepRow != 0 && b.keepRow == 0) return false;
                return a.keepRow < b.keepRow;
            });

            QVector<::BranchCandidateDisplayItem> items;
            items.reserve(keeps.size());
            for (const auto& k : keeps) {
                ::BranchCandidateDisplayItem itx;
                itx.row      = k.keepRow;
                itx.varN     = (k.keepRow == 0 ? -1 : k.keepRow - 1);
                itx.lineName = lineNameForRow(k.keepRow); // "Main" / "VarN"
                itx.label    = k.lbl;
                items.push_back(itx);
            }

            // 保存
            ::BranchCandidateDisplay plan;
            plan.ply       = targetPly;
            plan.baseLabel = baseForDisplay;
            plan.items     = std::move(items);
            m_branchDisplayPlan[r].insert(targetPly, std::move(plan));
        }
    }
}

void KifuLoadCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    // 既存の接続を解除（重複接続/古いタブへのぶら下がり防止）
    if (m_analysisTab) {
        QObject::disconnect(
            m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this,         &KifuLoadCoordinator::applyResolvedRowAndSelect
            );
    }

    m_analysisTab = tab;

    // 新しいタブに配線（「分岐ツリーのクリック → 行適用＆選択」）
    if (m_analysisTab) {
        QObject::connect(
            m_analysisTab, &EngineAnalysisTab::branchNodeActivated,
            this,          &KifuLoadCoordinator::applyResolvedRowAndSelect,
            Qt::UniqueConnection
            );
    }

    // 既存の Presenter 依存も更新（従来どおり）
    if (m_navPresenter) {
        NavigationPresenter::Deps d;
        d.coordinator = this;
        d.analysisTab = m_analysisTab;
        m_navPresenter->setDeps(d);
    }
}

void KifuLoadCoordinator::ensureNavigationPresenter_()
{
    if (m_navPresenter) return;

    NavigationPresenter::Deps d;
    d.coordinator = this;
    d.analysisTab = m_analysisTab;

    m_navPresenter = new NavigationPresenter(d, this);

    // 必要なら通知を拾ってログや別UIに伝播
    // connect(m_navPresenter, &NavigationPresenter::branchUiUpdated,
    //         this, &KifuLoadCoordinator::onBranchUiUpdated); // ※スロット用意時
}

void KifuLoadCoordinator::applyResolvedRowAndSelect(int row, int selPly)
{
    // デバッグ：入口
    qDebug().noquote()
        << "[KLC] applyResolvedRowAndSelect enter"
        << "row=" << row << "selPly=" << selPly
        << " resolvedRows.size=" << m_resolvedRows.size()
        << " recPtr=" << static_cast<const void*>(m_sfenRecord)
        << " recSize(before)=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

    // ------- 安全化と早期リターン -------
    if (m_resolvedRows.isEmpty()) {
        // 解決行が未構築のケースでは、現在の disp を使って最低限の同期だけ行う
        const int pick = (selPly < 0) ? 0 : selPly;
        showRecordAtPly(m_dispCurrent, pick);
        ensureNavigationPresenter_();
        m_navPresenter->refreshAll(/*row=*/0, pick);
        qDebug() << "[KLC] applyResolvedRowAndSelect leave (no resolved rows)";
        return;
    }

    // 呼び出し引数の一旦安全化
    const int nRows   = m_resolvedRows.size();
    int       safeRow = (row < 0) ? 0 : ((row >= nRows) ? (nRows - 1) : row);
    int       safePly = (selPly < 0) ? 0 : selPly;

    // ======== Sticky 分岐ロジック ========
    // 直前に Var 行を表示していた場合、ツリーで「分岐前」のノードを押しても
    // その Var 行を維持する（= 本譜へ勝手に戻さない）
    // 条件:
    //   1) 今回要求 row が本譜(=0)
    //   2) 直前のアクティブ行が Var 行 (varIndex >= 0)
    //   3) 今回の手数 safePly が、その Var 行の startPly より前
    const int prevRow = (m_activeResolvedRow < 0)
                            ? 0
                            : ((m_activeResolvedRow >= nRows) ? (nRows - 1) : m_activeResolvedRow);

    if (safeRow == 0 && prevRow >= 0 && prevRow < nRows) {
        const ResolvedRow& prev = m_resolvedRows.at(prevRow);
        if (prev.varIndex >= 0) {
            const int varStart = (prev.startPly <= 0) ? 1 : prev.startPly;
            if (safePly < varStart) {
                qDebug() << "[KLC][sticky] keep variation row instead of main:"
                         << "prevRow=" << prevRow << "varStart=" << varStart
                         << "requested ply=" << safePly;
                safeRow = prevRow; // ← 本譜に戻さず Var 行を維持
            }
        }
    }
    // ======== Sticky 分岐ここまで ========

    // ★★★★★ 現在の解決行/手をまず更新（以降の処理はこの状態を前提） ★★★★★
    m_activeResolvedRow = safeRow;
    m_activePly         = safePly;

    // 以降：表示列/局面列/USIムーブ列を、この safeRow の行に切り替え
    const ResolvedRow& rr = m_resolvedRows.at(safeRow);

    // 1..N の表示列
    m_dispCurrent = rr.disp;

    // 0..N の SFEN 列（共有実体を書き換え/COW）
    if (m_sfenRecord) {
        if (!rr.sfen.isEmpty()) {
            *m_sfenRecord = rr.sfen;
            qDebug().noquote() << "[KLC] SFEN overwritten from resolvedRows  size="
                               << m_sfenRecord->size();
        } else {
            qDebug().noquote() << "[KLC] rr.sfen is EMPTY -> keep existing shared SFEN (size="
                               << m_sfenRecord->size() << ")";
        }
    }

    // 1..N の USI ムーブ列
    m_gameMoves = rr.gm;

    // ------- 棋譜テーブルへ反映 & 選択（safePly 行を選択） -------
    showRecordAtPly(m_dispCurrent, safePly);

    // 「分岐あり」マーカーの再計算と描画更新（本譜/変化の切替に追随）
    updateKifuBranchMarkersForActiveRow();

    m_loadingKifu = false;

    // ------- 分岐候補（Plan 方式）とツリーハイライトまで一括更新 -------
    ensureNavigationPresenter_();
    m_navPresenter->refreshAll(safeRow, safePly);

    // デバッグ：出口
    qDebug().noquote()
        << "[KLC] applyResolvedRowAndSelect leave"
        << " recSize(after)=" << (m_sfenRecord ? m_sfenRecord->size() : -1);
}

// 既存：分岐候補モデルの構築・表示更新を担う関数
void KifuLoadCoordinator::showBranchCandidates(int row, int ply)
{
    // Plan ベースの表示へ一本化
    showBranchCandidatesFromPlan(row, ply);

    // 表示更新後にツリーハイライトだけ通知（逆呼びになるため refreshAll は呼ばない）
    ensureNavigationPresenter_();
    m_navPresenter->updateAfterBranchListChanged(row, ply);
}

void KifuLoadCoordinator::onMainMoveRowChanged(int selPly)
{
    // selPly を安全化
    const int safePly = qMax(0, selPly);

    // 読み込み中はスキップ（再帰的な更新抑止）
    if (m_loadingKifu) return;

    // “いまアクティブな解決行” を安全化
    const int row = (m_resolvedRows.isEmpty()
                         ? 0
                         : qBound(0, m_activeResolvedRow, m_resolvedRows.size() - 1));

    // 盤/棋譜/ハイライトは Coordinator の既存ユーティリティに一任
    applyResolvedRowAndSelect(row, safePly);
}

void KifuLoadCoordinator::setBranchCandidatesController(BranchCandidatesController* ctl)
{
    m_branchCtl = ctl;
}

// ===== KifuLoadCoordinator.cpp: ライブ対局から分岐ツリーを更新 =====
QList<KifDisplayItem> KifuLoadCoordinator::collectDispFromRecordModel_() const
{
    QList<KifDisplayItem> disp;

    if (!m_kifuRecordModel) return disp;

    const int rows = m_kifuRecordModel->rowCount();
    if (rows <= 1) return disp; // 0 行目は「=== 開始局面 ===」

    // 「ASCII数字(1..,23..)+空白任意+ (▲|△)」の時だけ、その数字部分を剥がす。
    // 例: "1 ▲７六歩" / "1▲７六歩" / "23▲..." / "23 ▲..." は除去対象。
    // 例: "▲７六歩" / "△７六歩" / "７六歩"（先頭が全角数字）は対象外。
    static const QRegularExpression kDupMoveNoPattern(
        QStringLiteral("^\\s*([0-9]+)\\s*(?=[▲△])")
        );

    for (int r = 1; r < rows; ++r) {
        const QModelIndex idxMove = m_kifuRecordModel->index(r, 0);
        const QModelIndex idxTime = m_kifuRecordModel->index(r, 1);

        QString move = m_kifuRecordModel->data(idxMove, Qt::DisplayRole).toString();
        const QString time = m_kifuRecordModel->data(idxTime, Qt::DisplayRole).toString();

        // ★ 重複付与検知時のみ、先頭の ASCII 手数を除去
        move.remove(kDupMoveNoPattern);

        const int ply = r; // ヘッダを除いた 1 始まりの絶対手数
        disp.push_back(KifDisplayItem(move, time, QString(), ply));
    }

    return disp;
}

// ライブ（HvH/HvE）対局の 1手追加ごとに分岐ツリーを更新するエントリポイント
void KifuLoadCoordinator::updateBranchTreeFromLive(int currentPly)
{
    // 1) 現在の棋譜モデルから disp を再構成
    const QList<KifDisplayItem> dispLive = collectDispFromRecordModel_();
    if (!m_analysisTab) return;

    // 2) 本譜行（row=0 相当：parent==-1）を特定（なければ作成）
    int mainRow = -1;
    {
        const int n = m_resolvedRows.size();
        for (int i = 0; i < n; ++i) {
            if (m_resolvedRows.at(i).parent < 0) { mainRow = i; break; }
        }
        if (mainRow < 0) {
            ResolvedRow main;
            main.startPly = 1;
            main.parent   = -1;
            main.disp     = dispLive;
            main.sfen     = QStringList();
            main.gm       = QVector<ShogiMove>();
            main.varIndex = -1;
            m_resolvedRows.push_back(main);
            mainRow = m_resolvedRows.size() - 1;
            m_activeResolvedRow = mainRow;
        }
    }

    // 3) アンカー手（「現在の局面から開始」）の決定
    const int anchorPly = (m_branchPlyContext >= 0)
                              ? m_branchPlyContext
                              : qMax(0, m_currentSelectedPly);
    const bool startFromCurrentPos = (anchorPly > 0);

    // 4) 行の更新（または新規追加）
    int highlightRow = mainRow;
    int highlightAbsPly = qBound(0, currentPly, dispLive.size());

    if (!startFromCurrentPos) {
        // 4-a) 通常の新規対局：本譜をそのまま置換
        m_resolvedRows[mainRow].disp = dispLive;
        highlightRow    = mainRow;
        highlightAbsPly = qBound(0, currentPly, m_resolvedRows.at(mainRow).disp.size());
    } else {
        // 4-b) 「現在の局面」からの 2局目以降
        const int startPly = anchorPly + 1;        // 分岐開始の絶対手
        const int suffixStart = qBound(0, anchorPly, dispLive.size());

        // 親の prefix を切り出す
        int parentRow = m_activeResolvedRow;
        if (parentRow < 0 || parentRow >= m_resolvedRows.size()) parentRow = mainRow;

        QList<KifDisplayItem> merged = m_resolvedRows.at(parentRow).disp;
        if (merged.size() > startPly - 1) merged.resize(startPly - 1);

        const auto& liveConst = std::as_const(dispLive);
        for (int i = suffixStart; i < liveConst.size(); ++i) {
            merged.push_back(liveConst.at(i));
        }

        // 既存のライブ枝を探す（自分自身 または 同一条件の行）
        int liveRowIdx = -1;

        // 1. 自分自身チェック
        if (m_activeResolvedRow >= 0 && m_activeResolvedRow < m_resolvedRows.size()) {
            const auto& current = m_resolvedRows.at(m_activeResolvedRow);
            if (current.varIndex <= -2 && current.startPly == startPly) {
                liveRowIdx = m_activeResolvedRow;
            }
        }
        // 2. リスト検索
        if (liveRowIdx < 0) {
            const int n = m_resolvedRows.size();
            for (int i = 0; i < n; ++i) {
                const ResolvedRow& r = m_resolvedRows.at(i);
                if (r.parent == parentRow && r.startPly == startPly && r.varIndex <= -2) {
                    liveRowIdx = i;
                    break;
                }
            }
        }

        if (liveRowIdx < 0) {
            // 新規作成
            ResolvedRow br;
            br.startPly = startPly;
            br.parent   = parentRow;
            br.disp     = merged;
            br.sfen     = QStringList();
            br.gm       = QVector<ShogiMove>();
            br.varIndex = -2;
            m_resolvedRows.push_back(br);
            liveRowIdx = m_resolvedRows.size() - 1;
        } else {
            // 既存更新
            m_resolvedRows[liveRowIdx].disp = merged;
        }

        m_activeResolvedRow = liveRowIdx;

        const int relPlayed = qMax(0, currentPly - anchorPly);
        highlightRow    = liveRowIdx;
        highlightAbsPly = (startPly - 1) + relPlayed;
        highlightAbsPly = qBound(startPly - 1,
                                 highlightAbsPly,
                                 startPly - 1 + (liveConst.size() - suffixStart));
    }

    // ---------------------------------------------------------
    // ★ 行の並び替え
    //    親子関係を維持しつつ、兄弟間では「開始手数が遅い（大きい）」順に並べる。
    //    これにより、親行の直下には「奥の分岐」が配置され、罫線の交差を防げる。
    // ---------------------------------------------------------
    {
        // 1. 親子関係マップ構築
        QMap<int, QVector<int>> childrenMap;
        for (int i = 0; i < m_resolvedRows.size(); ++i) {
            int p = m_resolvedRows[i].parent;
            childrenMap[p].push_back(i);
        }

        // 2. 兄弟間のソート (startPly 降順: 大きい方が先)
        for (auto it = childrenMap.begin(); it != childrenMap.end(); ++it) {
            QVector<int>& siblings = it.value();
            std::sort(siblings.begin(), siblings.end(), [&](int a, int b){
                const auto& rA = m_resolvedRows.at(a);
                const auto& rB = m_resolvedRows.at(b);

                // ★ 修正: startPly が大きい（遅い）方を先にする
                if (rA.startPly != rB.startPly) {
                    return rA.startPly > rB.startPly;
                }
                return a < b;
            });
        }

        // 3. DFSで新しい順序リストを作成
        QVector<int> sortedIndices;
        sortedIndices.reserve(m_resolvedRows.size());

        std::function<void(int)> dfs = [&](int parentIdx){
            if (!childrenMap.contains(parentIdx)) return;
            const auto& children = childrenMap[parentIdx];
            for (int childIdx : children) {
                sortedIndices.push_back(childIdx);
                dfs(childIdx);
            }
        };

        // ルート(-1)から開始
        dfs(-1);

        // 4. 並べ替えの適用
        if (sortedIndices.size() == m_resolvedRows.size()) {
            QMap<int, int> oldToNew;
            for (int i = 0; i < sortedIndices.size(); ++i) {
                oldToNew[sortedIndices[i]] = i;
            }

            QVector<ResolvedRow> newRows;
            newRows.reserve(m_resolvedRows.size());

            for (int oldIdx : sortedIndices) {
                ResolvedRow row = m_resolvedRows[oldIdx];
                // 親インデックスを新しい番号に更新
                if (row.parent >= 0) {
                    row.parent = oldToNew.value(row.parent, -1);
                }
                newRows.push_back(row);
            }

            m_resolvedRows = newRows;

            if (highlightRow >= 0) {
                highlightRow = oldToNew.value(highlightRow, 0);
            }
            m_activeResolvedRow = highlightRow;
        }
    }

    // 現在の行に対して、最新の SFEN と GameMoves を保存する
    if (highlightRow >= 0 && highlightRow < m_resolvedRows.size()) {
        if (m_activeResolvedRow == highlightRow) {
            if (m_sfenRecord) {
                m_resolvedRows[highlightRow].sfen = *m_sfenRecord;
            }
            m_resolvedRows[highlightRow].gm = m_gameMoves;
        }
    }

    // 5) EngineAnalysisTab へ供給
    QVector<EngineAnalysisTab::ResolvedRowLite> rowsLite;
    rowsLite.reserve(m_resolvedRows.size());
    const auto& rowsConst = std::as_const(m_resolvedRows);
    for (const ResolvedRow& r : rowsConst) {
        EngineAnalysisTab::ResolvedRowLite x;
        x.startPly = r.startPly;
        x.disp     = r.disp;
        x.sfen     = r.sfen;
        x.parent   = r.parent;
        rowsLite.push_back(std::move(x));
    }
    m_analysisTab->setBranchTreeRows(rowsLite);

    // 6) ハイライト
    m_analysisTab->highlightBranchTreeAt(highlightRow, highlightAbsPly, /*centerOn=*/true);
}

void KifuLoadCoordinator::applyBranchMarksForCurrentLine_()
{
    if (!m_kifuRecordModel) return;

    QSet<int> marks; // ply1=1..N（モデルの行番号と一致。0は開始局面なので除外）

    const auto itRow = m_branchDisplayPlan.constFind(m_activeResolvedRow);
    if (itRow != m_branchDisplayPlan.cend()) {
        // byPly: QMap<int (ply1), BranchCandidateDisplay>
        const QMap<int, BranchCandidateDisplay>& byPly = itRow.value();

        const QList<int> keys = byPly.keys(); // QMap → 安全に昇順
        for (int i = 0; i < keys.size(); ++i) {
            const int ply1 = keys.at(i);
            if (ply1 > 0) marks.insert(ply1);
        }
    }

    m_kifuRecordModel->setBranchPlyMarks(marks);
}

void KifuLoadCoordinator::rebuildBranchPlanAndMarksForLive(int /*currentPly*/)
{
    // --- セーフティ ---
    if (m_resolvedRows.isEmpty()) {
        if (m_kifuRecordModel) {
            const QSet<int> empty;
            m_kifuRecordModel->setBranchPlyMarks(empty);
        }
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        return;
    }

    // 1) 分岐候補Planの再構築（m_branchDisplayPlan を更新）
    buildBranchCandidateDisplayPlan();

    // 2) 「現在の局面から開始」のアンカー手（= 1局目の途中で選択した手）を固定取得
    const int anchorPly = (m_branchPlyContext >= 0)
                              ? m_branchPlyContext
                              : qMax(0, m_currentSelectedPly);

    // 3) ＋/オレンジの付与対象となる「アンカー行」を求める
    //    updateBranchTreeFromLive() 実行直後は m_activeResolvedRow が
    //    ライブ枝（varIndex<=-2）になるので、その親がアンカー行。
    int anchorRow = 0; // 本譜を既定
    if (m_activeResolvedRow >= 0 && m_activeResolvedRow < m_resolvedRows.size()) {
        if (m_resolvedRows[m_activeResolvedRow].varIndex <= -2) {
            anchorRow = qMax(0, m_resolvedRows[m_activeResolvedRow].parent);
        } else {
            anchorRow = m_activeResolvedRow;
        }
    }

    // 4) デリゲート用マーカー／モデルの '+' は「一時的に」anchorRow を
    //    アクティブとして再計算してから元に戻す（文脈は動かさない）
    const int oldActive = m_activeResolvedRow;
    m_activeResolvedRow = anchorRow;
    updateKifuBranchMarkersForActiveRow();   // 行背景（オレンジ）の再計算
    applyBranchMarksForCurrentLine_();       // 行末の '+' 再付与
    m_activeResolvedRow = oldActive;

    // 5) 分岐候補欄は UI のみ更新（★ m_branchPlyContext を変更しない）
    refreshBranchCandidatesUIOnly_(anchorRow, anchorPly);
}

void KifuLoadCoordinator::refreshBranchCandidatesUIOnly_(int row, int ply1)
{
    QTableView* view = m_recordPane ? m_recordPane->branchView() : m_kifuBranchView;

    // 安全化
    if (ply1 <= 0 || row < 0 || row >= m_resolvedRows.size()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }
        return;
    }

    const auto itRow = m_branchDisplayPlan.constFind(row);
    if (itRow == m_branchDisplayPlan.constEnd()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }
        return;
    }

    const QMap<int, BranchCandidateDisplay>& byPly = itRow.value();
    const auto itP = byPly.constFind(ply1);
    if (itP == byPly.constEnd()) {
        if (m_kifuBranchModel) {
            m_kifuBranchModel->clearBranchCandidates();
            m_kifuBranchModel->setHasBackToMainRow(false);
        }
        if (view) { view->setVisible(true); view->setEnabled(false); }
        if (m_recordPane) {
            if (auto* btn = m_recordPane->backToMainButton()) btn->setVisible(false);
        }
        return;
    }

    const BranchCandidateDisplay& plan = itP.value();

    // Controller 経由で候補を流し込む（クリック遷移は従来どおり生きる）
    if (m_branchCtl) {
        QVector<BranchCandidateDisplayItem> pubItems;
        pubItems.reserve(plan.items.size());
        for (const auto& it : plan.items) {
            BranchCandidateDisplayItem x;
            x.row      = it.row;
            x.varN     = it.varN;
            x.lineName = it.lineName;
            x.label    = it.label;
            pubItems.push_back(x);
        }
        m_branchCtl->refreshCandidatesFromPlan(ply1, pubItems, plan.baseLabel);
    } else if (m_kifuBranchModel) {
        QList<KifDisplayItem> rows;
        rows.reserve(plan.items.size());
        for (const auto& it : plan.items) {
            rows.push_back(KifDisplayItem(it.label, QString(), QString(), ply1));
        }
        m_kifuBranchModel->setHasBackToMainRow(false);
        m_kifuBranchModel->setBranchCandidatesFromKif(rows);
    }

    if (view && m_kifuBranchModel) {
        const int rc = m_kifuBranchModel->rowCount();
        view->setVisible(true);
        view->setEnabled(rc > 0);
        if (rc > 0) view->selectRow(0);
    }
}
